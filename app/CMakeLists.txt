cmake_minimum_required(VERSION 3.21)

project(Vision3DInspector
    VERSION 0.3.0
    LANGUAGES CXX
)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

option(VISION3DINSPECTOR_BUILD_TESTS "Build Qt Test targets" ON)
option(VISION3DINSPECTOR_BUILD_GEOMETRY_WORKER
       "Build the Open3D and meshoptimizer geometry worker when local SDKs are available"
       ON)

set(VISION3D_OPEN3D_ROOT
    "${CMAKE_CURRENT_LIST_DIR}/../third_party/open3d/0.20.0"
    CACHE PATH "Open3D 0.20.0 C++ package root")
set(VISION3D_MESHOPTIMIZER_ROOT
    "${CMAKE_CURRENT_LIST_DIR}/../third_party/meshoptimizer/1.3"
    CACHE PATH "meshoptimizer source root")

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets OpenGL OpenGLWidgets Network SerialBus Qml Quick QuickWidgets QuickControls2)
find_package(OpenCV 4.14.0 REQUIRED COMPONENTS core imgproc imgcodecs)

set(VISION3D_OPEN3D_TARGET "")
if(VISION3DINSPECTOR_BUILD_GEOMETRY_WORKER
   AND EXISTS "${VISION3D_OPEN3D_ROOT}/CMake/Open3DConfig.cmake")
    list(APPEND CMAKE_PREFIX_PATH "${VISION3D_OPEN3D_ROOT}/CMake")
    find_package(Open3D CONFIG QUIET)
    if(TARGET Open3D::Open3D)
        set(VISION3D_OPEN3D_TARGET Open3D::Open3D)
    elseif(TARGET Open3D)
        set(VISION3D_OPEN3D_TARGET Open3D)
    endif()
endif()

set(VISION3DINSPECTOR_SOURCES
    src/main.cpp
    src/app/MainWindow.cpp
    src/app/MainWindow.h
    src/app/AppShellViewModel.cpp
    src/app/AppShellViewModel.h
    src/qml/qml.qrc
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
    src/core/device/GaugeAsset.cpp
    src/core/device/GaugeAsset.h
    src/core/device/GaugeStatus.h
    src/core/device/ModbusTcpBinding.h
    src/core/realtime/GaugeDataSource.cpp
    src/core/realtime/GaugeDataSource.h
    src/core/realtime/ModbusTcpGaugeDataSource.cpp
    src/core/realtime/ModbusTcpGaugeDataSource.h
    src/core/realtime/GaugeLiveState.cpp
    src/core/realtime/GaugeLiveState.h
    src/core/realtime/RealtimeMonitoringController.cpp
    src/core/realtime/RealtimeMonitoringController.h
    src/core/device/GaugeProfile.cpp
    src/core/device/GaugeProfile.h
    src/core/inspection/InspectionRecord.cpp
    src/core/inspection/InspectionRecord.h
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
    src/widgets/GaugeAssetDialog.cpp
    src/widgets/GaugeAssetDialog.h
    src/widgets/GaugeStatusRuleDialog.cpp
    src/widgets/GaugeStatusRuleDialog.h
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
    src/widgets/VisualGaugeReadingDialog.cpp
    src/widgets/VisualGaugeReadingDialog.h
    src/core/vision/VisualGaugeReader.cpp
    src/core/vision/VisualGaugeReader.h
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

add_subdirectory(src/core/ai)
add_executable(Vision3DInspector WIN32 ${VISION3DINSPECTOR_SOURCES})

target_compile_features(Vision3DInspector PRIVATE cxx_std_17)
target_include_directories(Vision3DInspector PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    ${OpenCV_INCLUDE_DIRS}
)
target_link_libraries(Vision3DInspector PRIVATE
    Vision3DAiJobClient
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::OpenGL
    Qt6::OpenGLWidgets
    Qt6::Network
    Qt6::SerialBus
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickWidgets
    Qt6::QuickControls2
    ${OpenCV_LIBS}
)

if(MSVC)
    target_compile_options(Vision3DInspector PRIVATE /W4 /permissive- /FS)
endif()

option(BUILD_TESTING "Build and register tests" ON)
if(BUILD_TESTING AND VISION3DINSPECTOR_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# The application uses ordinary QRC-loaded QQuickWidget files.  Avoid the
# deploy-time qmlimportscanner here because the bundled scanner cannot decode
# this repository's Chinese source path on Windows; runtime imports are still
# resolved by Qt's QML engine from the compiled resource system.

add_executable(Vision3DMainWindowSmoke
    src/smoke/MainWindowSmokeMain.cpp
    src/smoke/MainWindowIntegrationSmoke.cpp
    src/smoke/MainWindowIntegrationSmoke.h
    ${VISION3DINSPECTOR_COMMON_SOURCES}
)

target_compile_features(Vision3DMainWindowSmoke PRIVATE cxx_std_17)
target_include_directories(Vision3DMainWindowSmoke PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    ${OpenCV_INCLUDE_DIRS}
)
target_link_libraries(Vision3DMainWindowSmoke PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::OpenGL
    Qt6::OpenGLWidgets
    Qt6::Network
    Qt6::SerialBus
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickWidgets
    Qt6::QuickControls2
    ${OpenCV_LIBS}
)

if(MSVC)
    target_compile_options(Vision3DMainWindowSmoke PRIVATE /W4 /permissive- /FS)
endif()


function(copy_opencv_runtime target_name)
    if(TARGET opencv_world)
        get_target_property(_opencv_release_dll opencv_world IMPORTED_LOCATION_RELEASE)
        get_target_property(_opencv_debug_dll opencv_world IMPORTED_LOCATION_DEBUG)
    elseif(TARGET opencv_core)
        get_target_property(_opencv_release_dll opencv_core IMPORTED_LOCATION_RELEASE)
        get_target_property(_opencv_debug_dll opencv_core IMPORTED_LOCATION_DEBUG)
    endif()
    if(_opencv_release_dll AND NOT _opencv_release_dll MATCHES "-NOTFOUND$")
        add_custom_command(TARGET ${target_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND}
                "-DOPENCV_RELEASE_DLL=${_opencv_release_dll}"
                "-DOPENCV_DEBUG_DLL=${_opencv_debug_dll}"
                "-DTARGET_FILE_DIR=$<TARGET_FILE_DIR:${target_name}>"
                "-DCONFIG=$<CONFIG>"
                -P "${CMAKE_CURRENT_LIST_DIR}/cmake/CopyOpenCVRuntime.cmake"
            VERBATIM
        )
    endif()
endfunction()

copy_opencv_runtime(Vision3DInspector)
copy_opencv_runtime(Vision3DMainWindowSmoke)

if(VISION3DINSPECTOR_BUILD_GEOMETRY_WORKER
   AND VISION3D_OPEN3D_TARGET
   AND EXISTS "${VISION3D_MESHOPTIMIZER_ROOT}/CMakeLists.txt")
    set(MESHOPT_BUILD_DEMO OFF CACHE BOOL "" FORCE)
    set(MESHOPT_BUILD_GLTFPACK OFF CACHE BOOL "" FORCE)
    set(MESHOPT_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(MESHOPT_INSTALL OFF CACHE BOOL "" FORCE)
    add_subdirectory("${VISION3D_MESHOPTIMIZER_ROOT}"
                     "${CMAKE_BINARY_DIR}/meshoptimizer-build"
                     EXCLUDE_FROM_ALL)

    if(TARGET meshoptimizer)
        add_executable(Vision3DGeometryWorker
            src/geometry_worker/main.cpp
        )
        target_compile_features(Vision3DGeometryWorker PRIVATE cxx_std_17)
        target_link_libraries(Vision3DGeometryWorker PRIVATE
            ${VISION3D_OPEN3D_TARGET}
            meshoptimizer
        )
        if(MSVC)
            target_compile_options(Vision3DGeometryWorker PRIVATE /W4 /permissive- /FS)
        endif()

        file(GLOB VISION3D_OPEN3D_RUNTIME_DLLS
             CONFIGURE_DEPENDS "${VISION3D_OPEN3D_ROOT}/bin/*.dll")
        foreach(VISION3D_OPEN3D_DLL IN LISTS VISION3D_OPEN3D_RUNTIME_DLLS)
            add_custom_command(TARGET Vision3DGeometryWorker POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${VISION3D_OPEN3D_DLL}"
                    "$<TARGET_FILE_DIR:Vision3DGeometryWorker>"
                VERBATIM
            )
        endforeach()
    endif()
elseif(VISION3DINSPECTOR_BUILD_GEOMETRY_WORKER)
    message(WARNING
        "Geometry worker disabled because the local Open3D 0.20.0 package or meshoptimizer source was not found.")
endif()
