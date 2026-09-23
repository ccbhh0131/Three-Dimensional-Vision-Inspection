# Architecture Overview

## Product boundary

The project is a Windows desktop foundation for three-dimensional vision
inspection workflows. The formal application is written in C++ with Qt and is
organized around project state, image assets, reconstruction orchestration,
artifact validation and geometry validation.

It is not presented as a finished industrial monitoring product. Device
drivers, a production data loop, a self-contained reconstruction runtime and a
formal Gauge runtime remain outside the V0.1.0 public boundary.

## Application areas

| Area | Public responsibility |
|---|---|
| Project and settings | Manage project metadata and user-configurable external-tool settings. |
| Image assets | Register source images, expose asset information and support preview-oriented workflows. |
| Reconstruction | Coordinate an external reconstruction process and its workspace/artifact lifecycle. |
| Validation | Check expected reconstruction artifacts and geometry-contract assumptions. |
| UI shell | Provide the Qt desktop windows, actions, status and testable application boundaries. |

## Dependency boundary

Qt is the application framework. CMake and MSVC are build-tool requirements.
COLMAP is an external backend used by the managed reconstruction workflow; its
binary, checkout and transitive third-party materials are not included in this
preview.

## Evidence boundary

The public notes summarize completed engineering checks and retain their
limits. They do not imply metric-scale accuracy, production throughput,
hardware integration, or industrial acceptance unless a future release adds
separate evidence for those claims.
