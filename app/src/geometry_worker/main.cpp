#include <open3d/geometry/KDTreeFlann.h>
#include <open3d/geometry/KDTreeSearchParam.h>
#include <open3d/geometry/PointCloud.h>
#include <open3d/geometry/TriangleMesh.h>
#include <open3d/io/PointCloudIO.h>
#include <open3d/io/TriangleMeshIO.h>

#include <meshoptimizer.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

constexpr double kDensityTrimFraction = 0.02;
constexpr double kFaceKeepFraction = 0.75;
constexpr double kSimplificationErrorFraction = 0.0015;

struct VertexRecord
{
    float position[3]{};
    float normal[3]{};
    std::uint8_t color[3]{185, 185, 185};
};

struct RefinementStats
{
    std::size_t inputPoints = 0;
    std::size_t cleanedPoints = 0;
    std::size_t open3dVertices = 0;
    std::size_t open3dFaces = 0;
    std::size_t finalVertices = 0;
    std::size_t finalFaces = 0;
};

std::string utf8Path(const fs::path& path)
{
    const auto bytes = path.u8string();
    return std::string(bytes.begin(), bytes.end());
}

template <typename T>
void writeBinary(std::ostream& output, const T& value)
{
    output.write(reinterpret_cast<const char*>(&value), sizeof(T));
}

std::string jsonEscape(const std::string& value)
{
    std::string escaped;
    escaped.reserve(value.size() + 8);
    for (const char character : value) {
        switch (character) {
        case '\\':
            escaped += "\\\\";
            break;
        case '"':
            escaped += "\\\"";
            break;
        case '\n':
            escaped += "\\n";
            break;
        case '\r':
            escaped += "\\r";
            break;
        case '\t':
            escaped += "\\t";
            break;
        default:
            escaped += character;
            break;
        }
    }
    return escaped;
}

double medianPointSpacing(const open3d::geometry::PointCloud& cloud)
{
    open3d::geometry::KDTreeFlann tree(cloud);
    const std::size_t step = std::max<std::size_t>(1, cloud.points_.size() / 10000);
    std::vector<double> spacings;
    std::vector<int> indices;
    std::vector<double> distances;
    for (std::size_t index = 0; index < cloud.points_.size(); index += step) {
        if (tree.SearchKNN(cloud.points_[index], 2, indices, distances) == 2
            && distances[1] > 0.0) {
            spacings.push_back(std::sqrt(distances[1]));
        }
    }
    if (spacings.empty()) {
        throw std::runtime_error("Open3D could not estimate point spacing");
    }
    const auto middle = spacings.begin() + spacings.size() / 2;
    std::nth_element(spacings.begin(), middle, spacings.end());
    return *middle;
}

double meshDiagonal(const open3d::geometry::TriangleMesh& mesh)
{
    if (mesh.vertices_.empty()) {
        return 1.0;
    }
    Eigen::Vector3d minimum = mesh.vertices_.front();
    Eigen::Vector3d maximum = mesh.vertices_.front();
    for (const Eigen::Vector3d& vertex : mesh.vertices_) {
        minimum = minimum.cwiseMin(vertex);
        maximum = maximum.cwiseMax(vertex);
    }
    const double diagonal = (maximum - minimum).norm();
    return diagonal > 0.0 ? diagonal : 1.0;
}

std::vector<VertexRecord> vertexRecords(const open3d::geometry::TriangleMesh& mesh)
{
    std::vector<VertexRecord> records(mesh.vertices_.size());
    for (std::size_t index = 0; index < mesh.vertices_.size(); ++index) {
        const Eigen::Vector3d& position = mesh.vertices_[index];
        VertexRecord& record = records[index];
        record.position[0] = static_cast<float>(position.x());
        record.position[1] = static_cast<float>(position.y());
        record.position[2] = static_cast<float>(position.z());
        if (index < mesh.vertex_normals_.size()) {
            const Eigen::Vector3d& normal = mesh.vertex_normals_[index];
            record.normal[0] = static_cast<float>(normal.x());
            record.normal[1] = static_cast<float>(normal.y());
            record.normal[2] = static_cast<float>(normal.z());
        }
        if (index < mesh.vertex_colors_.size()) {
            const Eigen::Vector3d& color = mesh.vertex_colors_[index];
            record.color[0] = static_cast<std::uint8_t>(std::clamp(color.x() * 255.0, 0.0, 255.0));
            record.color[1] = static_cast<std::uint8_t>(std::clamp(color.y() * 255.0, 0.0, 255.0));
            record.color[2] = static_cast<std::uint8_t>(std::clamp(color.z() * 255.0, 0.0, 255.0));
        }
    }
    return records;
}

std::vector<std::uint32_t> triangleIndices(const open3d::geometry::TriangleMesh& mesh)
{
    std::vector<std::uint32_t> indices;
    indices.reserve(mesh.triangles_.size() * 3);
    for (const Eigen::Vector3i& triangle : mesh.triangles_) {
        indices.push_back(static_cast<std::uint32_t>(triangle[0]));
        indices.push_back(static_cast<std::uint32_t>(triangle[1]));
        indices.push_back(static_cast<std::uint32_t>(triangle[2]));
    }
    return indices;
}

void writePly(const fs::path& path,
              const std::vector<VertexRecord>& vertices,
              const std::vector<std::uint32_t>& indices)
{
    if (vertices.empty() || indices.empty() || indices.size() % 3 != 0) {
        throw std::runtime_error("cannot write an empty triangle mesh");
    }
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        throw std::runtime_error("cannot open output PLY: " + path.string());
    }
    output << "ply\nformat binary_little_endian 1.0\n"
           << "element vertex " << vertices.size() << "\n"
           << "property float x\nproperty float y\nproperty float z\n"
           << "property float nx\nproperty float ny\nproperty float nz\n"
           << "property uchar red\nproperty uchar green\nproperty uchar blue\n"
           << "element face " << indices.size() / 3 << "\n"
           << "property list uchar int vertex_indices\nend_header\n";
    for (const VertexRecord& vertex : vertices) {
        for (const float value : vertex.position) {
            writeBinary(output, value);
        }
        for (const float value : vertex.normal) {
            writeBinary(output, value);
        }
        output.write(reinterpret_cast<const char*>(vertex.color), 3);
    }
    for (std::size_t index = 0; index < indices.size(); index += 3) {
        const std::uint8_t count = 3;
        const std::int32_t a = static_cast<std::int32_t>(indices[index]);
        const std::int32_t b = static_cast<std::int32_t>(indices[index + 1]);
        const std::int32_t c = static_cast<std::int32_t>(indices[index + 2]);
        writeBinary(output, count);
        writeBinary(output, a);
        writeBinary(output, b);
        writeBinary(output, c);
    }
    if (!output) {
        throw std::runtime_error("failed while writing output PLY: " + path.string());
    }
}

std::vector<VertexRecord> simplifyMesh(const open3d::geometry::TriangleMesh& mesh,
                                       std::vector<std::uint32_t>* outputIndices,
                                       double diagonal)
{
    std::vector<VertexRecord> vertices = vertexRecords(mesh);
    std::vector<std::uint32_t> sourceIndices = triangleIndices(mesh);
    if (vertices.empty() || sourceIndices.size() < 3) {
        throw std::runtime_error("meshoptimizer input has no triangles");
    }

    std::vector<std::uint32_t> cacheOptimized(sourceIndices.size());
    meshopt_optimizeVertexCache(cacheOptimized.data(),
                                sourceIndices.data(),
                                sourceIndices.size(),
                                vertices.size());
    const std::size_t targetFaces = std::max<std::size_t>(1,
        static_cast<std::size_t>((sourceIndices.size() / 3) * kFaceKeepFraction));
    const std::size_t targetIndices = std::max<std::size_t>(3, targetFaces * 3);
    std::vector<std::uint32_t> simplified(sourceIndices.size());
    float simplificationError = 0.0f;
    const std::size_t simplifiedCount = meshopt_simplify(
        simplified.data(),
        cacheOptimized.data(),
        cacheOptimized.size(),
        vertices.front().position,
        vertices.size(),
        sizeof(VertexRecord),
        targetIndices,
        static_cast<float>(diagonal * kSimplificationErrorFraction),
        meshopt_SimplifyLockBorder | meshopt_SimplifyErrorAbsolute,
        &simplificationError);
    if (simplifiedCount < 3 || simplifiedCount % 3 != 0) {
        throw std::runtime_error("meshoptimizer simplification produced no triangles");
    }
    simplified.resize(simplifiedCount);

    std::vector<std::uint32_t> finalIndices(simplified.size());
    meshopt_optimizeVertexCache(finalIndices.data(),
                                simplified.data(),
                                simplified.size(),
                                vertices.size());
    std::vector<VertexRecord> fetched(vertices.size());
    const std::size_t fetchedCount = meshopt_optimizeVertexFetch(fetched.data(),
                                                                  finalIndices.data(),
                                                                  finalIndices.size(),
                                                                  vertices.data(),
                                                                  vertices.size(),
                                                                  sizeof(VertexRecord));
    fetched.resize(fetchedCount);
    *outputIndices = std::move(finalIndices);
    return fetched;
}

fs::path projectRootFromRefinementDirectory(const fs::path& refinementDirectory)
{
    fs::path projectRoot = refinementDirectory;
    for (int level = 0; level < 5; ++level) {
        projectRoot = projectRoot.parent_path();
    }
    if (projectRoot.empty()) {
        throw std::runtime_error("refinement output is not under a project workspace");
    }
    return projectRoot;
}

std::string relativeProjectPath(const fs::path& path, const fs::path& projectRoot)
{
    std::error_code error;
    const fs::path relative = fs::relative(path, projectRoot, error);
    const std::string relativeText = relative.generic_string();
    if (error || relative.empty()
        || relativeText == ".."
        || relativeText.compare(0, 3, "../") == 0) {
        throw std::runtime_error("output path is outside the project workspace");
    }
    return relativeText;
}

void writeRefinementJson(const fs::path& outputDirectory,
                         const fs::path& cloudPath,
                         const RefinementStats& stats)
{
    const fs::path projectRoot = projectRootFromRefinementDirectory(outputDirectory);
    const std::string inputCloud = relativeProjectPath(cloudPath, projectRoot);
    const std::string open3dMesh = relativeProjectPath(outputDirectory / "open3d-mesh.ply",
                                                       projectRoot);
    const std::string finalMesh = relativeProjectPath(outputDirectory / "final-mesh.ply",
                                                      projectRoot);
    std::ofstream output(outputDirectory / "refinement.json", std::ios::trunc);
    if (!output) {
        throw std::runtime_error("cannot write refinement.json");
    }
    output << "{\n"
           << "  \"pipelineVersion\": \"stage5b2-open3d-meshoptimizer-v1\",\n"
           << "  \"inputCloud\": \"" << jsonEscape(inputCloud) << "\",\n"
           << "  \"open3dMesh\": \"" << jsonEscape(open3dMesh) << "\",\n"
           << "  \"finalMesh\": \"" << jsonEscape(finalMesh) << "\",\n"
           << "  \"inputPoints\": " << stats.inputPoints << ",\n"
           << "  \"open3dVertices\": " << stats.open3dVertices << ",\n"
           << "  \"open3dFaces\": " << stats.open3dFaces << ",\n"
           << "  \"finalVertices\": " << stats.finalVertices << ",\n"
           << "  \"finalFaces\": " << stats.finalFaces << ",\n"
           << "  \"status\": \"completed\"\n"
           << "}\n";
}

RefinementStats refine(const fs::path& cloudPath, const fs::path& outputDirectory)
{
    if (!fs::is_regular_file(cloudPath)) {
        throw std::runtime_error("input fused point cloud does not exist: " + cloudPath.string());
    }
    fs::create_directories(outputDirectory);

    auto cloud = std::make_shared<open3d::geometry::PointCloud>();
    if (!open3d::io::ReadPointCloud(utf8Path(cloudPath), *cloud)) {
        throw std::runtime_error("Open3D failed to read fused point cloud");
    }
    RefinementStats stats;
    stats.inputPoints = cloud->points_.size();
    cloud->RemoveNonFinitePoints(true, true);
    cloud->RemoveDuplicatedPoints();
    cloud = std::get<0>(cloud->RemoveStatisticalOutliers(20, 2.0));
    stats.cleanedPoints = cloud->points_.size();
    if (stats.cleanedPoints < 100) {
        throw std::runtime_error("Open3D cleaning left too few points");
    }

    const double spacing = medianPointSpacing(*cloud);
    cloud->EstimateNormals(open3d::geometry::KDTreeSearchParamHybrid(spacing * 5.0, 40));
    cloud->OrientNormalsConsistentTangentPlane(30);

    auto reconstruction = open3d::geometry::TriangleMesh::CreateFromPointCloudPoisson(
        *cloud, 9, 0.0f, 1.1f, false, -1);
    std::shared_ptr<open3d::geometry::TriangleMesh> mesh = std::get<0>(reconstruction);
    const std::vector<double>& density = std::get<1>(reconstruction);
    if (!mesh || mesh->triangles_.empty() || density.size() != mesh->vertices_.size()) {
        throw std::runtime_error("Open3D Poisson reconstruction returned an empty mesh");
    }

    std::vector<double> sortedDensity = density;
    const std::size_t cutoffIndex = static_cast<std::size_t>(
        kDensityTrimFraction * static_cast<double>(sortedDensity.size() - 1));
    std::nth_element(sortedDensity.begin(),
                     sortedDensity.begin() + cutoffIndex,
                     sortedDensity.end());
    const double cutoff = sortedDensity[cutoffIndex];
    std::vector<bool> removeMask(density.size(), false);
    for (std::size_t index = 0; index < density.size(); ++index) {
        removeMask[index] = density[index] < cutoff;
    }
    mesh->RemoveVerticesByMask(removeMask);
    mesh->RemoveDegenerateTriangles();
    mesh->RemoveDuplicatedTriangles();
    mesh->RemoveUnreferencedVertices();
    mesh->ComputeVertexNormals();

    stats.open3dVertices = mesh->vertices_.size();
    stats.open3dFaces = mesh->triangles_.size();
    if (!open3d::io::WriteTriangleMesh(utf8Path(outputDirectory / "open3d-mesh.ply"), *mesh)) {
        throw std::runtime_error("Open3D could not write open3d-mesh.ply");
    }

    std::vector<std::uint32_t> finalIndices;
    const std::vector<VertexRecord> finalVertices = simplifyMesh(
        *mesh, &finalIndices, meshDiagonal(*mesh));
    stats.finalVertices = finalVertices.size();
    stats.finalFaces = finalIndices.size() / 3;
    writePly(outputDirectory / "final-mesh.ply", finalVertices, finalIndices);
    writeRefinementJson(outputDirectory, cloudPath, stats);

    std::cout << "GEOMETRY input_points=" << stats.inputPoints
              << " cleaned_points=" << stats.cleanedPoints
              << " open3d_vertices=" << stats.open3dVertices
              << " open3d_faces=" << stats.open3dFaces
              << " final_vertices=" << stats.finalVertices
              << " final_faces=" << stats.finalFaces << '\n';
    return stats;
}

} // namespace

int wmain(int argc, wchar_t** argv)
{
    try {
        if (argc != 6 || std::wstring(argv[1]) != L"refine"
            || std::wstring(argv[2]) != L"--cloud"
            || std::wstring(argv[4]) != L"--output") {
            std::wcerr << L"Usage: Vision3DGeometryWorker.exe refine --cloud <fused.ply> --output <directory>\n";
            return 2;
        }
        const RefinementStats stats = refine(fs::path(argv[3]), fs::path(argv[5]));
        (void)stats;
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << "GeometryWorker ERROR: " << exception.what() << '\n';
        return 1;
    }
}
