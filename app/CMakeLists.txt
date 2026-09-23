cmake_minimum_required(VERSION 3.21)

project(Vision3DInspector
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

option(VISION3DINSPECTOR_BUILD_TESTS "Build Qt Test targets" ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets)

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
)

add_executable(Vision3DInspector WIN32 ${VISION3DINSPECTOR_SOURCES})

target_compile_features(Vision3DInspector PRIVATE cxx_std_17)
target_include_directories(Vision3DInspector PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(Vision3DInspector PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
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
