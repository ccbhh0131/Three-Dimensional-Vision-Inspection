find_package(Qt6 6.5 REQUIRED COMPONENTS Test)

add_executable(test_project_manifest
    test_project_manifest.cpp
    ../src/core/assets/AssetRecord.cpp
    ../src/core/assets/AssetRecord.h
    ../src/core/device/GaugeAsset.cpp
    ../src/core/device/GaugeAsset.h
    ../src/core/device/GaugeStatus.h
    ../src/core/device/GaugeProfile.cpp
    ../src/core/device/GaugeProfile.h
    ../src/core/inspection/InspectionRecord.cpp
    ../src/core/inspection/InspectionRecord.h
    ../src/core/project/ProjectManifest.cpp
    ../src/core/project/ProjectManifest.h
    ../src/core/reconstruction/ReconstructionTask.cpp
    ../src/core/reconstruction/ReconstructionTask.h
)
target_compile_features(test_project_manifest PRIVATE cxx_std_17)
if(MSVC)
    target_compile_options(test_project_manifest PRIVATE /FS)
endif()
target_include_directories(test_project_manifest PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../src")
target_link_libraries(test_project_manifest PRIVATE Qt6::Core Qt6::Gui Qt6::Test)
add_test(NAME project_manifest COMMAND test_project_manifest)

add_executable(test_colmap_backend
    test_colmap_backend.cpp
    ../src/backend/ColmapBackend.cpp
    ../src/backend/ColmapBackend.h
    ../src/backend/BackendProbeResult.h
    ../src/backend/ReconstructionBackend.h
    ../src/core/process/ProcessRunner.cpp
    ../src/core/process/ProcessRunner.h
)
target_compile_features(test_colmap_backend PRIVATE cxx_std_17)
if(MSVC)
    target_compile_options(test_colmap_backend PRIVATE /FS)
endif()
target_include_directories(test_colmap_backend PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../src")
target_link_libraries(test_colmap_backend PRIVATE Qt6::Core Qt6::Test)
add_test(NAME colmap_backend COMMAND test_colmap_backend)

add_executable(test_engine_locator
    test_engine_locator.cpp
    ../src/backend/ReconstructionEngineLocator.cpp
    ../src/backend/ReconstructionEngineLocator.h
)
target_compile_features(test_engine_locator PRIVATE cxx_std_17)
if(MSVC)
    target_compile_options(test_engine_locator PRIVATE /FS)
endif()
target_include_directories(test_engine_locator PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../src")
target_link_libraries(test_engine_locator PRIVATE Qt6::Core Qt6::Test)
add_test(NAME engine_locator COMMAND test_engine_locator)

add_executable(test_project_manager
    test_project_manager.cpp
    ../src/core/device/DeviceMarker.cpp
    ../src/core/device/DeviceMarker.h
    ../src/core/device/GaugeAsset.cpp
    ../src/core/device/GaugeAsset.h
    ../src/core/device/GaugeStatus.h
    ../src/core/device/GaugeProfile.cpp
    ../src/core/device/GaugeProfile.h
    ../src/core/inspection/InspectionRecord.cpp
    ../src/core/inspection/InspectionRecord.h
    ../src/core/assets/AssetRecord.cpp
    ../src/core/assets/AssetRecord.h
    ../src/core/project/ProjectManager.cpp
    ../src/core/project/ProjectManager.h
    ../src/core/project/ProjectManifest.cpp
    ../src/core/project/ProjectManifest.h
    ../src/core/reconstruction/ReconstructionTask.cpp
    ../src/core/reconstruction/ReconstructionTask.h
)
target_compile_features(test_project_manager PRIVATE cxx_std_17)
if(MSVC)
    target_compile_options(test_project_manager PRIVATE /FS)
endif()
target_include_directories(test_project_manager PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../src")
target_link_libraries(test_project_manager PRIVATE Qt6::Core Qt6::Gui Qt6::Test)
add_test(NAME project_manager COMMAND test_project_manager)

add_executable(test_main_window
    test_main_window.cpp
    ../src/app/MainWindow.cpp
    ../src/app/MainWindow.h
    ../src/app/AppPreferences.cpp
    ../src/app/AppPreferences.h
    ../src/app/AppShellViewModel.cpp
    ../src/app/AppShellViewModel.h
    ../resources/branding.qrc
    ../src/qml/qml.qrc
    ../src/core/project/ProjectManager.cpp
    ../src/core/project/ProjectManager.h
    ../src/core/project/ProjectManifest.cpp
    ../src/core/project/ProjectManifest.h
    ../src/core/assets/AssetRecord.cpp
    ../src/core/assets/AssetRecord.h
    ../src/core/assets/ThumbnailCache.cpp
    ../src/core/assets/ThumbnailCache.h
    ../src/core/process/ProcessRunner.cpp
    ../src/core/process/ProcessRunner.h
    ../src/core/device/DeviceMarker.cpp
    ../src/core/device/DeviceMarker.h
    ../src/core/device/GaugeAsset.cpp
    ../src/core/device/GaugeAsset.h
    ../src/core/device/GaugeStatus.h
    ../src/core/realtime/GaugeDataSource.cpp
    ../src/core/realtime/GaugeDataSource.h
    ../src/core/realtime/ModbusTcpGaugeDataSource.cpp
    ../src/core/realtime/ModbusTcpGaugeDataSource.h
    ../src/core/realtime/GaugeLiveState.cpp
    ../src/core/realtime/GaugeLiveState.h
    ../src/core/realtime/RealtimeMonitoringController.cpp
    ../src/core/realtime/RealtimeMonitoringController.h
    ../src/core/device/GaugeProfile.cpp
    ../src/core/device/GaugeProfile.h
    ../src/core/inspection/InspectionRecord.cpp
    ../src/core/inspection/InspectionRecord.h
    ../src/core/reconstruction/ReconstructionArtifactValidator.cpp
    ../src/core/reconstruction/ReconstructionArtifactValidator.h
    ../src/core/reconstruction/ReconstructionController.cpp
    ../src/core/reconstruction/ReconstructionController.h
    ../src/core/reconstruction/ReconstructionTask.cpp
    ../src/core/reconstruction/ReconstructionTask.h
    ../src/core/reconstruction/ReconstructionWorkspace.cpp
    ../src/core/reconstruction/ReconstructionWorkspace.h
    ../src/backend/BackendProbeResult.h
    ../src/backend/ReconstructionBackend.h
    ../src/backend/ColmapBackend.cpp
    ../src/backend/ColmapBackend.h
    ../src/backend/ReconstructionEngineLocator.cpp
    ../src/backend/ReconstructionEngineLocator.h
    ../src/widgets/BackendPanel.cpp
    ../src/widgets/BackendPanel.h
    ../src/widgets/DeveloperSettingsDialog.cpp
    ../src/widgets/DeveloperSettingsDialog.h
    ../src/widgets/GaugeAssetDialog.cpp
    ../src/widgets/GaugeAssetDialog.h
    ../src/widgets/GaugeStatusRuleDialog.cpp
    ../src/widgets/GaugeStatusRuleDialog.h
    ../src/widgets/ImageAssetModel.cpp
    ../src/widgets/ImageAssetModel.h
    ../src/widgets/ImageBrowserPanel.cpp
    ../src/widgets/ImageBrowserPanel.h
    ../src/widgets/ImagePreviewWidget.cpp
    ../src/widgets/ImagePreviewWidget.h
    ../src/widgets/LogPanel.cpp
    ../src/widgets/LogPanel.h
    ../src/widgets/ProjectPanel.cpp
    ../src/widgets/ProjectPanel.h
    ../src/widgets/ProductDialogStyle.cpp
    ../src/widgets/ProductDialogStyle.h
    ../src/widgets/ReconstructionPanel.cpp
    ../src/widgets/ReconstructionPanel.h
    ../src/core/geometry/BoundingBox.cpp
    ../src/core/geometry/BoundingBox.h
    ../src/core/geometry/MarkerHitTesting.cpp
    ../src/core/geometry/MarkerHitTesting.h
    ../src/core/geometry/Ray.cpp
    ../src/core/geometry/Ray.h
    ../src/core/geometry/SurfaceHit.h
    ../src/core/geometry/MeshPicking.cpp
    ../src/core/geometry/MeshPicking.h
    ../src/core/mesh/MeshData.cpp
    ../src/core/mesh/MeshData.h
    ../src/core/mesh/PlyMeshLoader.cpp
    ../src/core/mesh/PlyMeshLoader.h
    ../src/core/viewer/CameraController.cpp
    ../src/core/viewer/CameraController.h
    ../src/render/MeshRenderer.cpp
    ../src/render/MeshRenderer.h
    ../src/render/MarkerRenderer.cpp
    ../src/render/MarkerRenderer.h
    ../src/widgets/ModelViewerWidget.cpp
    ../src/widgets/ModelViewerWidget.h
    ../src/core/vision/VisualGaugeReader.cpp
    ../src/core/vision/VisualGaugeReader.h
    ../src/widgets/VisualGaugeReadingDialog.cpp
    ../src/widgets/VisualGaugeReadingDialog.h
)
target_compile_features(test_main_window PRIVATE cxx_std_17)
if(MSVC)
    target_compile_options(test_main_window PRIVATE /FS)
endif()
target_include_directories(test_main_window PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../src"
    ${OpenCV_INCLUDE_DIRS}
)
target_link_libraries(test_main_window PRIVATE
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
    Qt6::Test
    ${OpenCV_LIBS}
)
add_test(NAME main_window_probe COMMAND test_main_window)
set_tests_properties(main_window_probe PROPERTIES
    ENVIRONMENT "VISION3DINSPECTOR_DISABLE_QML_UI=1"
)

add_executable(test_gauge_status
    test_gauge_status.cpp
    ../src/core/device/GaugeAsset.cpp
    ../src/core/device/GaugeAsset.h
    ../src/core/device/GaugeStatus.h
)
target_compile_features(test_gauge_status PRIVATE cxx_std_17)
if(MSVC)
    target_compile_options(test_gauge_status PRIVATE /FS)
endif()
target_include_directories(test_gauge_status PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../src")
target_link_libraries(test_gauge_status PRIVATE Qt6::Core Qt6::Test)
add_test(NAME gauge_status COMMAND test_gauge_status)

add_executable(test_realtime_monitoring
    test_realtime_monitoring.cpp
    ../src/core/realtime/GaugeDataSource.cpp
    ../src/core/realtime/GaugeDataSource.h
    ../src/core/realtime/ModbusTcpGaugeDataSource.cpp
    ../src/core/realtime/ModbusTcpGaugeDataSource.h
    ../src/core/realtime/GaugeLiveState.cpp
    ../src/core/realtime/GaugeLiveState.h
    ../src/core/realtime/RealtimeMonitoringController.cpp
    ../src/core/realtime/RealtimeMonitoringController.h
    ../src/core/device/DeviceMarker.cpp
    ../src/core/device/DeviceMarker.h
    ../src/core/device/GaugeAsset.cpp
    ../src/core/device/GaugeAsset.h
    ../src/core/device/GaugeStatus.h
    ../src/core/device/GaugeProfile.cpp
    ../src/core/device/GaugeProfile.h
    ../src/core/inspection/InspectionRecord.cpp
    ../src/core/inspection/InspectionRecord.h
    ../src/core/assets/AssetRecord.cpp
    ../src/core/assets/AssetRecord.h
    ../src/core/project/ProjectManager.cpp
    ../src/core/project/ProjectManager.h
    ../src/core/project/ProjectManifest.cpp
    ../src/core/project/ProjectManifest.h
    ../src/core/reconstruction/ReconstructionTask.cpp
    ../src/core/reconstruction/ReconstructionTask.h
)
target_compile_features(test_realtime_monitoring PRIVATE cxx_std_17)
if(MSVC)
    target_compile_options(test_realtime_monitoring PRIVATE /FS)
endif()
target_include_directories(test_realtime_monitoring PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../src"
)
target_link_libraries(test_realtime_monitoring PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Network
    Qt6::SerialBus
    Qt6::Test
)
add_test(NAME realtime_monitoring COMMAND test_realtime_monitoring)

add_executable(test_modbus_tcp_data_source
    test_modbus_tcp_data_source.cpp
    ../src/core/realtime/GaugeDataSource.cpp
    ../src/core/realtime/GaugeDataSource.h
    ../src/core/realtime/GaugeLiveState.cpp
    ../src/core/realtime/GaugeLiveState.h
    ../src/core/realtime/ModbusTcpGaugeDataSource.cpp
    ../src/core/realtime/ModbusTcpGaugeDataSource.h
    ../src/core/realtime/RealtimeMonitoringController.cpp
    ../src/core/realtime/RealtimeMonitoringController.h
    ../src/core/device/DeviceMarker.cpp
    ../src/core/device/DeviceMarker.h
    ../src/core/device/GaugeAsset.cpp
    ../src/core/device/GaugeAsset.h
    ../src/core/device/GaugeStatus.h
    ../src/core/device/ModbusTcpBinding.h
    ../src/core/device/GaugeProfile.cpp
    ../src/core/device/GaugeProfile.h
    ../src/core/inspection/InspectionRecord.cpp
    ../src/core/inspection/InspectionRecord.h
    ../src/core/assets/AssetRecord.cpp
    ../src/core/assets/AssetRecord.h
    ../src/core/project/ProjectManager.cpp
    ../src/core/project/ProjectManager.h
    ../src/core/project/ProjectManifest.cpp
    ../src/core/project/ProjectManifest.h
    ../src/core/reconstruction/ReconstructionTask.cpp
    ../src/core/reconstruction/ReconstructionTask.h
)
target_compile_features(test_modbus_tcp_data_source PRIVATE cxx_std_17)
if(MSVC)
    target_compile_options(test_modbus_tcp_data_source PRIVATE /FS)
endif()
target_include_directories(test_modbus_tcp_data_source PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../src"
)
target_link_libraries(test_modbus_tcp_data_source PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Network
    Qt6::SerialBus
    Qt6::Test
)
add_test(NAME modbus_tcp_data_source COMMAND test_modbus_tcp_data_source)

add_executable(test_image_assets
    test_image_assets.cpp
    ../src/core/device/DeviceMarker.cpp
    ../src/core/device/DeviceMarker.h
    ../src/core/device/GaugeAsset.cpp
    ../src/core/device/GaugeAsset.h
    ../src/core/device/GaugeProfile.cpp
    ../src/core/device/GaugeProfile.h
    ../src/core/inspection/InspectionRecord.cpp
    ../src/core/inspection/InspectionRecord.h
    ../src/backend/BackendProbeResult.h
    ../src/backend/ReconstructionBackend.h
    ../src/backend/ColmapBackend.cpp
    ../src/backend/ColmapBackend.h
    ../src/core/assets/AssetRecord.cpp
    ../src/core/assets/AssetRecord.h
    ../src/core/assets/ThumbnailCache.cpp
    ../src/core/assets/ThumbnailCache.h
    ../src/core/project/ProjectManifest.cpp
    ../src/core/project/ProjectManifest.h
    ../src/core/project/ProjectManager.cpp
    ../src/core/project/ProjectManager.h
    ../src/core/process/ProcessRunner.cpp
    ../src/core/process/ProcessRunner.h
    ../src/core/reconstruction/ReconstructionArtifactValidator.cpp
    ../src/core/reconstruction/ReconstructionArtifactValidator.h
    ../src/core/reconstruction/ReconstructionController.cpp
    ../src/core/reconstruction/ReconstructionController.h
    ../src/core/reconstruction/ReconstructionTask.cpp
    ../src/core/reconstruction/ReconstructionTask.h
    ../src/core/reconstruction/ReconstructionWorkspace.cpp
    ../src/core/reconstruction/ReconstructionWorkspace.h
    ../src/widgets/ImageAssetModel.cpp
    ../src/widgets/ImageAssetModel.h
    ../src/widgets/ImageBrowserPanel.cpp
    ../src/widgets/ImageBrowserPanel.h
    ../src/widgets/ImagePreviewWidget.cpp
    ../src/widgets/ImagePreviewWidget.h
)
target_compile_features(test_image_assets PRIVATE cxx_std_17)
if(MSVC)
    target_compile_options(test_image_assets PRIVATE /FS)
endif()
target_include_directories(test_image_assets PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../src")
target_link_libraries(test_image_assets PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Test)
add_test(NAME image_assets COMMAND test_image_assets)

add_executable(test_reconstruction
    test_reconstruction.cpp
    ../src/core/device/DeviceMarker.cpp
    ../src/core/device/DeviceMarker.h
    ../src/core/device/GaugeAsset.cpp
    ../src/core/device/GaugeAsset.h
    ../src/core/device/GaugeProfile.cpp
    ../src/core/device/GaugeProfile.h
    ../src/core/inspection/InspectionRecord.cpp
    ../src/core/inspection/InspectionRecord.h
    ../src/backend/BackendProbeResult.h
    ../src/backend/ReconstructionBackend.h
    ../src/backend/ColmapBackend.cpp
    ../src/backend/ColmapBackend.h
    ../src/core/assets/AssetRecord.cpp
    ../src/core/assets/AssetRecord.h
    ../src/core/process/ProcessRunner.cpp
    ../src/core/process/ProcessRunner.h
    ../src/core/project/ProjectManager.cpp
    ../src/core/project/ProjectManager.h
    ../src/core/project/ProjectManifest.cpp
    ../src/core/project/ProjectManifest.h
    ../src/core/reconstruction/ReconstructionArtifactValidator.cpp
    ../src/core/reconstruction/ReconstructionArtifactValidator.h
    ../src/core/reconstruction/ReconstructionController.cpp
    ../src/core/reconstruction/ReconstructionController.h
    ../src/core/reconstruction/ReconstructionTask.cpp
    ../src/core/reconstruction/ReconstructionTask.h
    ../src/core/reconstruction/ReconstructionWorkspace.cpp
    ../src/core/reconstruction/ReconstructionWorkspace.h
)
target_compile_features(test_reconstruction PRIVATE cxx_std_17)
if(MSVC)
    target_compile_options(test_reconstruction PRIVATE /FS)
endif()
target_include_directories(test_reconstruction PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../src")
target_link_libraries(test_reconstruction PRIVATE Qt6::Core Qt6::Gui Qt6::Test)
add_test(NAME reconstruction COMMAND test_reconstruction)

add_executable(test_mesh_loader
    test_mesh_loader.cpp
    ../src/core/geometry/BoundingBox.cpp
    ../src/core/geometry/BoundingBox.h
    ../src/core/mesh/MeshData.cpp
    ../src/core/mesh/MeshData.h
    ../src/core/mesh/PlyMeshLoader.cpp
    ../src/core/mesh/PlyMeshLoader.h
)
target_compile_features(test_mesh_loader PRIVATE cxx_std_17)
if(MSVC)
    target_compile_options(test_mesh_loader PRIVATE /FS)
endif()
target_include_directories(test_mesh_loader PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../src")
target_link_libraries(test_mesh_loader PRIVATE Qt6::Core Qt6::Gui Qt6::Test)
add_test(NAME mesh_loader COMMAND test_mesh_loader)

add_executable(test_camera_controller
    test_camera_controller.cpp
    ../src/core/geometry/BoundingBox.cpp
    ../src/core/geometry/BoundingBox.h
    ../src/core/viewer/CameraController.cpp
    ../src/core/viewer/CameraController.h
)
target_compile_features(test_camera_controller PRIVATE cxx_std_17)
if(MSVC)
    target_compile_options(test_camera_controller PRIVATE /FS)
endif()
target_include_directories(test_camera_controller PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../src")
target_link_libraries(test_camera_controller PRIVATE Qt6::Core Qt6::Gui Qt6::Test)
add_test(NAME camera_controller COMMAND test_camera_controller)

add_executable(test_mesh_picking
    test_mesh_picking.cpp
    ../src/core/geometry/BoundingBox.cpp
    ../src/core/geometry/BoundingBox.h
    ../src/core/geometry/Ray.cpp
    ../src/core/geometry/Ray.h
    ../src/core/geometry/SurfaceHit.h
    ../src/core/geometry/MeshPicking.cpp
    ../src/core/geometry/MeshPicking.h
    ../src/core/mesh/MeshData.cpp
    ../src/core/mesh/MeshData.h
    ../src/core/viewer/CameraController.cpp
    ../src/core/viewer/CameraController.h
)
target_compile_features(test_mesh_picking PRIVATE cxx_std_17)
if(MSVC)
    target_compile_options(test_mesh_picking PRIVATE /FS)
endif()
target_include_directories(test_mesh_picking PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../src")
target_link_libraries(test_mesh_picking PRIVATE Qt6::Core Qt6::Gui Qt6::Test)
add_test(NAME mesh_picking COMMAND test_mesh_picking)

add_executable(test_model_viewer_picking
    test_model_viewer_picking.cpp
    ../src/core/device/DeviceMarker.cpp
    ../src/core/device/DeviceMarker.h
    ../src/core/geometry/BoundingBox.cpp
    ../src/core/geometry/BoundingBox.h
    ../src/core/geometry/MarkerHitTesting.cpp
    ../src/core/geometry/MarkerHitTesting.h
    ../src/core/geometry/Ray.cpp
    ../src/core/geometry/Ray.h
    ../src/core/geometry/SurfaceHit.h
    ../src/core/geometry/MeshPicking.cpp
    ../src/core/geometry/MeshPicking.h
    ../src/core/mesh/MeshData.cpp
    ../src/core/mesh/MeshData.h
    ../src/core/viewer/CameraController.cpp
    ../src/core/viewer/CameraController.h
    ../src/render/MeshRenderer.cpp
    ../src/render/MeshRenderer.h
    ../src/render/MarkerRenderer.cpp
    ../src/render/MarkerRenderer.h
    ../src/widgets/ModelViewerWidget.cpp
    ../src/widgets/ModelViewerWidget.h
)
target_compile_features(test_model_viewer_picking PRIVATE cxx_std_17)
if(MSVC)
    target_compile_options(test_model_viewer_picking PRIVATE /FS)
endif()
target_include_directories(test_model_viewer_picking PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../src")
target_link_libraries(test_model_viewer_picking PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::OpenGL
    Qt6::OpenGLWidgets
    Qt6::Test
)
add_test(NAME model_viewer_picking COMMAND test_model_viewer_picking)

add_executable(test_device_marker
    test_device_marker.cpp
    ../src/core/device/DeviceMarker.cpp
    ../src/core/device/DeviceMarker.h
    ../src/core/device/GaugeAsset.cpp
    ../src/core/device/GaugeAsset.h
    ../src/core/device/GaugeProfile.cpp
    ../src/core/device/GaugeProfile.h
    ../src/core/inspection/InspectionRecord.cpp
    ../src/core/inspection/InspectionRecord.h
    ../src/core/assets/AssetRecord.cpp
    ../src/core/assets/AssetRecord.h
    ../src/core/project/ProjectManager.cpp
    ../src/core/project/ProjectManager.h
    ../src/core/project/ProjectManifest.cpp
    ../src/core/project/ProjectManifest.h
    ../src/core/reconstruction/ReconstructionTask.cpp
    ../src/core/reconstruction/ReconstructionTask.h
)
target_compile_features(test_device_marker PRIVATE cxx_std_17)
if(MSVC)
    target_compile_options(test_device_marker PRIVATE /FS)
endif()
target_include_directories(test_device_marker PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../src")
target_link_libraries(test_device_marker PRIVATE Qt6::Core Qt6::Gui Qt6::Test)
add_test(NAME device_marker COMMAND test_device_marker)

add_executable(test_marker_hit_testing
    test_marker_hit_testing.cpp
    ../src/core/device/DeviceMarker.h
    ../src/core/geometry/BoundingBox.cpp
    ../src/core/geometry/BoundingBox.h
    ../src/core/geometry/MarkerHitTesting.cpp
    ../src/core/geometry/MarkerHitTesting.h
    ../src/core/geometry/MeshPicking.cpp
    ../src/core/geometry/MeshPicking.h
    ../src/core/geometry/Ray.cpp
    ../src/core/geometry/Ray.h
    ../src/core/mesh/MeshData.cpp
    ../src/core/mesh/MeshData.h
    ../src/core/viewer/CameraController.cpp
    ../src/core/viewer/CameraController.h
)
target_compile_features(test_marker_hit_testing PRIVATE cxx_std_17)
if(MSVC)
    target_compile_options(test_marker_hit_testing PRIVATE /FS)
endif()
target_include_directories(test_marker_hit_testing PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../src")
target_link_libraries(test_marker_hit_testing PRIVATE Qt6::Core Qt6::Gui Qt6::Test)
add_test(NAME marker_hit_testing COMMAND test_marker_hit_testing)

add_executable(test_gauge_asset
    test_gauge_asset.cpp
    ../src/core/device/DeviceMarker.cpp
    ../src/core/device/DeviceMarker.h
    ../src/core/device/GaugeAsset.cpp
    ../src/core/device/GaugeAsset.h
    ../src/core/device/GaugeProfile.cpp
    ../src/core/device/GaugeProfile.h
    ../src/core/inspection/InspectionRecord.cpp
    ../src/core/inspection/InspectionRecord.h
    ../src/core/assets/AssetRecord.cpp
    ../src/core/assets/AssetRecord.h
    ../src/core/project/ProjectManager.cpp
    ../src/core/project/ProjectManager.h
    ../src/core/project/ProjectManifest.cpp
    ../src/core/project/ProjectManifest.h
    ../src/core/reconstruction/ReconstructionTask.cpp
    ../src/core/reconstruction/ReconstructionTask.h
)
target_compile_features(test_gauge_asset PRIVATE cxx_std_17)
if(MSVC)
    target_compile_options(test_gauge_asset PRIVATE /FS)
endif()
target_include_directories(test_gauge_asset PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../src")
target_link_libraries(test_gauge_asset PRIVATE Qt6::Core Qt6::Gui Qt6::Test)
add_test(NAME gauge_asset COMMAND test_gauge_asset)

add_executable(test_inspection_record
    test_inspection_record.cpp
    ../src/core/inspection/InspectionRecord.cpp
    ../src/core/inspection/InspectionRecord.h
    ../src/core/device/DeviceMarker.cpp
    ../src/core/device/DeviceMarker.h
    ../src/core/device/GaugeAsset.cpp
    ../src/core/device/GaugeAsset.h
    ../src/core/device/GaugeProfile.cpp
    ../src/core/device/GaugeProfile.h
    ../src/core/assets/AssetRecord.cpp
    ../src/core/assets/AssetRecord.h
    ../src/core/project/ProjectManager.cpp
    ../src/core/project/ProjectManager.h
    ../src/core/project/ProjectManifest.cpp
    ../src/core/project/ProjectManifest.h
    ../src/core/reconstruction/ReconstructionTask.cpp
    ../src/core/reconstruction/ReconstructionTask.h
)
target_compile_features(test_inspection_record PRIVATE cxx_std_17)
if(MSVC)
    target_compile_options(test_inspection_record PRIVATE /FS)
endif()
target_include_directories(test_inspection_record PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../src"
)
target_link_libraries(test_inspection_record PRIVATE Qt6::Core Qt6::Gui Qt6::Test)
add_test(NAME inspection_record COMMAND test_inspection_record)

add_executable(test_gauge_profile
    test_gauge_profile.cpp
    ../src/core/device/GaugeProfile.cpp
    ../src/core/device/GaugeProfile.h
)
target_compile_features(test_gauge_profile PRIVATE cxx_std_17)
if(MSVC)
    target_compile_options(test_gauge_profile PRIVATE /FS)
endif()
target_include_directories(test_gauge_profile PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../src")
target_link_libraries(test_gauge_profile PRIVATE Qt6::Core Qt6::Test)
add_test(NAME gauge_profile COMMAND test_gauge_profile)

add_executable(test_visual_gauge_reader
    test_visual_gauge_reader.cpp
    ../src/core/device/GaugeProfile.cpp
    ../src/core/device/GaugeProfile.h
    ../src/core/vision/VisualGaugeReader.cpp
    ../src/core/vision/VisualGaugeReader.h
)
target_compile_features(test_visual_gauge_reader PRIVATE cxx_std_17)
if(MSVC)
    target_compile_options(test_visual_gauge_reader PRIVATE /FS)
endif()
target_include_directories(test_visual_gauge_reader PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../src"
    ${OpenCV_INCLUDE_DIRS}
)
target_link_libraries(test_visual_gauge_reader PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    ${OpenCV_LIBS}
)
add_test(NAME visual_gauge_reader COMMAND test_visual_gauge_reader)

if(TARGET opencv_world)
    get_target_property(_opencv_release_dll opencv_world IMPORTED_LOCATION_RELEASE)
    get_target_property(_opencv_debug_dll opencv_world IMPORTED_LOCATION_DEBUG)
elseif(TARGET opencv_core)
    get_target_property(_opencv_release_dll opencv_core IMPORTED_LOCATION_RELEASE)
    get_target_property(_opencv_debug_dll opencv_core IMPORTED_LOCATION_DEBUG)
endif()
if(_opencv_release_dll AND NOT _opencv_release_dll MATCHES "-NOTFOUND$")
    add_custom_command(TARGET test_visual_gauge_reader POST_BUILD
        COMMAND ${CMAKE_COMMAND}
            "-DOPENCV_RELEASE_DLL=${_opencv_release_dll}"
            "-DOPENCV_DEBUG_DLL=${_opencv_debug_dll}"
            "-DTARGET_FILE_DIR=$<TARGET_FILE_DIR:test_visual_gauge_reader>"
            "-DCONFIG=$<CONFIG>"
            -P "${CMAKE_CURRENT_LIST_DIR}/../cmake/CopyOpenCVRuntime.cmake"
        VERBATIM
    )
    add_custom_command(TARGET test_main_window POST_BUILD
        COMMAND ${CMAKE_COMMAND}
            "-DOPENCV_RELEASE_DLL=${_opencv_release_dll}"
            "-DOPENCV_DEBUG_DLL=${_opencv_debug_dll}"
            "-DTARGET_FILE_DIR=$<TARGET_FILE_DIR:test_main_window>"
            "-DCONFIG=$<CONFIG>"
            -P "${CMAKE_CURRENT_LIST_DIR}/../cmake/CopyOpenCVRuntime.cmake"
        VERBATIM
    )
endif()
