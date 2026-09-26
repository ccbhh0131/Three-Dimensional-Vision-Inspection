# Third Party Notices

This file records the final V0.2.0 source-repository boundary. The v0.1.0 tag
remains unchanged as the historical source baseline. The project
Apache-2.0 license does not relicense third-party software, research code,
model weights or data. Third-party terms apply independently.

## Qt

- **Purpose:** GUI framework and Qt Test support.
- **Usage:** Build dependency; the application requests Qt Core, Gui and
  Widgets, and tests request Qt Test.
- **Included:** No Qt source, binary, SDK archive or runtime is bundled in
  this repository.
- **Distribution:** No Qt binaries bundled.
- **License status:** Refer to the applicable Qt licensing terms for the
  exact version, modules and distribution method. See
  <https://doc.qt.io/qt-6/licensing.html>.

## COLMAP

- **Purpose:** External reconstruction backend.
- **Usage:** Runtime external tool; the application validates a configured
  `COLMAP.bat` and `bin/colmap.exe` installation.
- **Included:** No COLMAP source, executable or dependency bundle is
  included.
- **Distribution:** Not bundled.
- **License status:** See the exact COLMAP and transitive dependency terms
  before distributing a runtime. Upstream reference:
  <https://github.com/colmap/colmap/blob/main/COPYING.txt>.

## OpenCV

- **Purpose:** Research image-processing reference.
- **Usage:** Not a formal Qt application runtime dependency in this preview.
- **Included:** No OpenCV source or binary included.
- **Distribution:** Not bundled.
- **License status:** No OpenCV component is distributed by this preview.
  Review the exact upstream version and modules if a future release adds an
  import, link or bundled runtime.

## Ultralytics / YOLO

- **Purpose:** Research reference only for possible gauge-model work.
- **Usage:** No inference or training runtime is part of this release
  candidate.
- **Included:** No source, weights, exported models or datasets included.
- **Distribution:** Not bundled.
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

These references are documentation-only and no source is copied into the
repository.

| Name | Purpose | Included | License status |
|---|---|---|---|
| `mc260/meter-vision` | YOLO pose and pointer-gauge geometry research | No | Fixed local README states MIT, but the fixed checkout had no `LICENSE`; review required |
| `SiddharthB7/vlm-analog-gauge-reader` | Detection, pose and geometry research | No | Fixed local checkout contains an MIT `LICENSE`; no source copied |
| `Bashithaperera/Analog-Gauge-Reader-YOLO26-Pose-Geometry-Engine-` | Isolated Gauge baseline research | No | Fixed checkout had no `LICENSE`; status unknown |
| Roboflow Gauge Analog | Dataset-source candidate in research notes | No | Dataset and redistribution terms remain unverified |

## Excluded materials

- Private Gauge images and annotations.
- Dataset archives and training results.
- `.pt`, `.pth`, `.onnx` and other model weights.
- Virtual environments, cache, logs and build outputs.
- External third-party repository checkouts and copied source.
