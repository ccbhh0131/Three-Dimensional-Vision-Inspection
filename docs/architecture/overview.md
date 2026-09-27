# Architecture Overview

## Product boundary

The project is a Windows desktop foundation for three-dimensional vision
inspection workflows. The formal application is written in C++17 with Qt and
is organized around project state, image assets, reconstruction orchestration,
geometry validation, device markers and gauge runtime state.

The `v0.3.0` boundary includes GaugeAsset and GaugeProfile domain objects,
manual-ROI OpenCV visual reading, InspectionRecord history, status evaluation,
Mock Sensor monitoring and a generic read-only Modbus TCP adapter. It does not
claim a finished industrial monitoring product, real PLC validation, PLC
control/write capability, MQTT/S7 integration or automatic AI gauge reading.

## Application areas

| Area | Public responsibility |
|---|---|
| Project and settings | Manage project metadata and user-configurable external-tool settings. |
| Image assets | Register source images, expose asset information and support preview-oriented workflows. |
| Reconstruction | Coordinate an external reconstruction process and its workspace/artifact lifecycle. |
| 3D viewer | Render the controlled mesh, support camera interaction, picking and DeviceMarker placement. |
| Gauge domain | Bind GaugeAsset to a marker, store GaugeProfile and create InspectionRecord history. |
| Visual reading | Run a manual-ROI OpenCV pointer/circle geometry workflow with human confirmation. |
| Realtime runtime | Separate GaugeDataSource, GaugeLiveState, status evaluation and explicit snapshots. |
| Modbus TCP | Provide a read-only Qt SerialBus adapter for Holding/Input register polling and decoding. |

## Dependency boundary

Qt is the application framework. The public CMake requests Qt Core, Gui,
Widgets, OpenGL, OpenGLWidgets, Network and SerialBus; the selected Qt
distribution must provide its matching SerialPort dependency and Qt Test.
OpenCV 4.x supplies the `core`, `imgproc` and `imgcodecs` modules. COLMAP is an
external backend used by the managed reconstruction workflow; its binary,
checkout and transitive third-party materials are not included.

## Runtime and persistence boundary

Project JSON persists project metadata, image assets, DeviceMarkers, GaugeAssets,
status rules, data-source bindings and explicit InspectionRecord snapshots.
Live samples and connection state remain runtime data until the user records a
snapshot. The Modbus adapter issues read requests only. Modbus behavior was
validated with a local Qt `QModbusTcpServer` loopback environment, not against
a physical PLC.

## Evidence boundary

The public notes summarize completed engineering checks and retain their
limits. They do not imply metric-scale accuracy, production throughput,
hardware integration, general gauge-reading accuracy or industrial acceptance
without separate evidence for those claims.
