cmake_minimum_required(VERSION 3.21)

project(Vision3DInspector
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

option(VISION3DINSPECTOR_BUILD_TESTS "Build Qt Test targets" ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets OpenGL OpenGLWidgets)

set(VISION3DINSPECTOR_SOURCES
    src/main.cpp
    src/app/MainWindow.cpp
    src/app/MainWindow.h
    src/core/assets/AssetRecord.cpp
    src/core/assets/AssetRecord.h
    src/core/assets/ThumbnailCache.cpp
    src/core/assets/ThumbnailCache.h
    src/core/project/ProjectManifest.cpp
    src/core/project/ProjectManifest.h
    src/core/project/ProjectManager.cpp
    src/core/project/ProjectManager.h
    src/core/process/ProcessRunner.cpp
    src/core/process/ProcessRunner.h
    src/core/device/DeviceMarker.cpp
    src/core/device/DeviceMarker.h
    src/core/geometry/BoundingBox.cpp
    src/core/geometry/BoundingBox.h
    src/core/geometry/MarkerHitTesting.cpp
    src/core/geometry/MarkerHitTesting.h
    src/core/geometry/Ray.cpp
    src/core/geometry/Ray.h
    src/core/geometry/SurfaceHit.h
    src/core/geometry/MeshPicking.cpp
    src/core/geometry/MeshPicking.h
    src/core/mesh/MeshData.cpp
    src/core/mesh/MeshData.h
    src/core/mesh/PlyMeshLoader.cpp
    src/core/mesh/PlyMeshLoader.h
    src/core/viewer/CameraController.cpp
    src/core/viewer/CameraController.h
    src/core/reconstruction/ReconstructionArtifactValidator.cpp
    src/core/reconstruction/ReconstructionArtifactValidator.h
    src/core/reconstruction/ReconstructionController.cpp
    src/core/reconstruction/ReconstructionController.h
    src/core/reconstruction/ReconstructionTask.cpp
    src/core/reconstruction/ReconstructionTask.h
    src/core/reconstruction/ReconstructionWorkspace.cpp
    src/core/reconstruction/ReconstructionWorkspace.h
    src/backend/BackendProbeResult.h
    src/backend/ReconstructionBackend.h
    src/backend/ColmapBackend.cpp
    src/backend/ColmapBackend.h
    src/backend/ReconstructionEngineLocator.cpp
    src/backend/ReconstructionEngineLocator.h
    src/widgets/BackendPanel.cpp
    src/widgets/BackendPanel.h
    src/widgets/DeveloperSettingsDialog.cpp
    src/widgets/DeveloperSettingsDialog.h
    src/widgets/ImageAssetModel.cpp
    src/widgets/ImageAssetModel.h
    src/widgets/ImageBrowserPanel.cpp
    src/widgets/ImageBrowserPanel.h
    src/widgets/ImagePreviewWidget.cpp
    src/widgets/ImagePreviewWidget.h
    src/widgets/LogPanel.cpp
    src/widgets/LogPanel.h
    src/widgets/ProjectPanel.cpp
    src/widgets/ProjectPanel.h
    src/widgets/ReconstructionPanel.cpp
    src/widgets/ReconstructionPanel.h
    src/render/MeshRenderer.cpp
    src/render/MeshRenderer.h
    src/render/MarkerRenderer.cpp
    src/render/MarkerRenderer.h
    src/widgets/ModelViewerWidget.cpp
    src/widgets/ModelViewerWidget.h
)

set(VISION3DINSPECTOR_COMMON_SOURCES ${VISION3DINSPECTOR_SOURCES})
list(REMOVE_ITEM VISION3DINSPECTOR_COMMON_SOURCES
    src/main.cpp
)

add_executable(Vision3DInspector WIN32 ${VISION3DINSPECTOR_SOURCES})

target_compile_features(Vision3DInspector PRIVATE cxx_std_17)
target_include_directories(Vision3DInspector PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(Vision3DInspector PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::OpenGL
    Qt6::OpenGLWidgets
)

if(MSVC)
    target_compile_options(Vision3DInspector PRIVATE /W4 /permissive- /FS)
endif()

option(BUILD_TESTING "Build and register tests" ON)
if(BUILD_TESTING AND VISION3DINSPECTOR_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

qt_finalize_executable(Vision3DInspector)

add_executable(Vision3DMainWindowSmoke
    src/smoke/MainWindowSmokeMain.cpp
    src/smoke/MainWindowIntegrationSmoke.cpp
    src/smoke/MainWindowIntegrationSmoke.h
    ${VISION3DINSPECTOR_COMMON_SOURCES}
)

target_compile_features(Vision3DMainWindowSmoke PRIVATE cxx_std_17)
target_include_directories(Vision3DMainWindowSmoke PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(Vision3DMainWindowSmoke PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::OpenGL
    Qt6::OpenGLWidgets
)

if(MSVC)
    target_compile_options(Vision3DMainWindowSmoke PRIVATE /W4 /permissive- /FS)
endif()

qt_finalize_executable(Vision3DMainWindowSmoke)
