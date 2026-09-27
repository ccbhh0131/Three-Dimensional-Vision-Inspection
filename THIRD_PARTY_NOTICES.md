# Third Party Notices

This file records the `v0.3.0` source-repository boundary. The `v0.1.0` and
`v0.2.0` tags remain available as historical source baselines. The Apache-2.0
license for this repository does not relicense third-party software, research
code, model weights or data. Third-party terms apply independently.

## Qt

- **Purpose:** Desktop UI, OpenGL widgets, networking, Qt SerialBus and Qt Test.
- **Usage:** The application requests Qt Core, Gui, Widgets, OpenGL,
  OpenGLWidgets, Network and SerialBus; tests request Qt Test.
- **Dependency note:** Qt SerialBus requires the matching Qt SerialPort
  dependency provided by the selected Qt distribution.
- **Included:** No Qt source, binary, SDK archive or runtime is bundled in this
  repository.
- **License status:** Refer to the applicable Qt licensing terms for the exact
  Qt version, modules and distribution method. See
  <https://doc.qt.io/qt-6/licensing.html>.

## OpenCV

- **Purpose:** Formal visual gauge reading runtime.
- **Usage:** The C++ application uses the `core`, `imgproc` and `imgcodecs`
  modules for manual-ROI pointer/circle geometry and image handling.
- **Included:** No OpenCV source, binary or runtime DLL is included.
- **Distribution:** The user supplies a compatible OpenCV 4.x installation
  through CMake discovery.
- **License status:** Review the exact upstream OpenCV version and module terms
  when distributing a runtime. See
  <https://opencv.org/license/>.

## COLMAP

- **Purpose:** External reconstruction backend.
- **Usage:** Runtime external tool for the managed reconstruction workflow.
- **Included:** No COLMAP source, executable or dependency bundle is included.
- **Distribution:** Not bundled. Review the exact COLMAP and transitive
  dependency terms before distributing a runtime. Upstream reference:
  <https://github.com/colmap/colmap/blob/main/COPYING.txt>.

## Ultralytics / YOLO

- **Purpose:** Historical research reference only.
- **Usage:** No YOLO inference or training runtime is part of this release.
- **Included:** No source, weights, exported models or datasets are included.
- **License status:** Future use requires review of the exact framework,
  checkpoint, model, dataset and commercial-use terms. Upstream reference:
  <https://github.com/ultralytics/ultralytics/blob/main/LICENSE>.

## Build tools

| Name | Purpose | Included | License status |
|---|---|---|---|
| CMake | Build configuration | No toolchain files bundled | Use the tool's own terms |
| MSVC / Visual Studio | C++ build toolchain | No toolchain files bundled | Use the selected Microsoft terms |
| Windows SDK | Platform build headers/tools | No SDK files bundled | Use the selected Microsoft terms |

## Research references

These references are documentation-only; no source, model or dataset is copied
into this repository.

| Name | Purpose | Included | License status |
|---|---|---|---|
| `mc260/meter-vision` | YOLO pose and pointer-gauge geometry research | No | Fixed local README states MIT, but the fixed checkout had no `LICENSE`; review required |
| `SiddharthB7/vlm-analog-gauge-reader` | Detection, pose and geometry research | No | Fixed local checkout contains an MIT `LICENSE`; no source copied |
| `Bashithaperera/Analog-Gauge-Reader-YOLO26-Pose-Geometry-Engine-` | Isolated gauge baseline research | No | Fixed checkout had no `LICENSE`; status unknown |
| Roboflow Gauge Analog | Dataset-source candidate in research notes | No | Dataset and redistribution terms remain unverified |

## Excluded materials

- Private gauge images and annotations.
- Dataset archives and training results.
- `.pt`, `.pth`, `.onnx` and other model weights.
- Virtual environments, cache, logs and build outputs.
- External third-party repository checkouts and copied source.
- Qt, OpenCV, COLMAP and SerialBus/SerialPort binary packages.
