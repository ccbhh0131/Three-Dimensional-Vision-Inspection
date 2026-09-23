# Three-Dimensional Vision Inspection Platform

> V0.1.0 public repository preview. This directory is assembled for review; GitHub publication remains a separate owner-approved step.

## Project Overview

Three-Dimensional Vision Inspection Platform is a C++ / Qt desktop vision inspection framework for image asset management, managed 3D reconstruction workflows and geometry validation.

The project is currently an engineering foundation with a separated research boundary. It is not yet a complete industrial monitoring or automatic measurement product.

## Features and Implemented Foundation

- Qt desktop application architecture.
- Image asset management and preview workflow.
- Managed 3D reconstruction workflow.
- COLMAP integration workflow through an external reconstruction backend.
- Reconstruction artifact and geometry validation foundations.
- Qt Test coverage for project, asset, backend and reconstruction orchestration behavior.

## Architecture

The application is organized as a Qt desktop shell over project and image
asset services, reconstruction orchestration, an external-backend adapter and
focused Qt Test targets. The reconstruction layer validates artifacts and
geometry while keeping the COLMAP runtime outside the source repository. See
`docs/architecture/overview.md` and `docs/reconstruction/` for the public
technical boundary.

## Current Public Preview

The preview contains the filtered application source and public technical notes:

```text
Three-Dimensional-Vision-Inspection/
├─ app/
│  ├─ CMakeLists.txt
│  ├─ CMakePresets.json
│  ├─ src/                    # C++ / Qt application source
│  └─ tests/                  # kept with app for CMake path stability
├─ docs/
│  ├─ architecture/
│  ├─ reconstruction/
│  └─ gauge/
├─ modules/gauge_research/    # research-only, filtered content
└─ examples/                  # documentation only; no image assets
```

The target structure is adjusted for build stability: tests remain under
`app/tests` because the current public CMake entry point includes that
directory. A root-level `tests/` directory is not used by this preview.

## Build Requirements

- Windows 10 or newer.
- Visual Studio 2022 / MSVC and a compatible CMake 3.x release.
- Qt 6.5.x with Core, Gui, Widgets and Test components.
- COLMAP 3.11.x may be configured as an external reconstruction backend for
  the opt-in integration workflow; its binary is not distributed here.

The preview supports the normal CMake flow from the `app` directory:

```text
cmake -S app -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

The commands above describe the public workflow; this assembly stage did not
rebuild the project or rerun CTest. Tests that probe an external COLMAP
installation are opt-in through `VISION3D_COLMAP_ROOT`.

## Research Module

Gauge inspection is maintained as a research prototype. The research boundary may contain algorithm notes, profile schema examples, evaluation documentation and carefully reviewed demonstration code. Private images, annotations, weights, training runs, environments and third-party source are not part of the public release.

## Not Yet

- Real-time industrial monitoring.
- A complete automatic measurement product.
- Embedded deployment.
- A self-contained COLMAP installer or bundled reconstruction runtime.
- A formally integrated Gauge runtime, 3D viewer and device-data loop.

## Public Data Policy

Datasets, private images, annotations, model weights, training outputs, cache, build outputs, virtual environments and third-party checkouts are excluded. See `THIRD_PARTY_NOTICES.md` for the public distribution boundary; internal release-audit records remain outside this repository.

## License

Licensed under the Apache License, Version 2.0.

You may obtain a copy of the License at [LICENSE](LICENSE). Third-party
software and research references retain their own licenses and review
requirements; see `THIRD_PARTY_NOTICES.md`.

## Third Party Dependencies

- **Qt** is the GUI and test build dependency. No Qt binaries are bundled in
  this source repository; applicable Qt licensing terms still apply.
- **COLMAP** is an external reconstruction backend used by the opt-in runtime
  workflow. Its binary is not bundled; applicable COLMAP and dependency terms
  still apply if a runtime is distributed separately.
- **OpenCV** is a research image-processing reference only in this preview.
  No OpenCV source or binary is included.
- **Ultralytics / YOLO** references are research-only. No framework source,
  model weights or datasets are included.

See `THIRD_PARTY_NOTICES.md` for the included/not-included boundary and the
license status of each third-party reference. Third-party terms apply
independently of this project's Apache-2.0 license.
