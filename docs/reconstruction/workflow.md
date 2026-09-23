# Reconstruction Workflow

## Managed flow

The application treats reconstruction as a managed external process:

1. A project supplies a workspace and registered image assets.
2. The reconstruction controller creates a task with explicit inputs and
   status transitions.
3. The task invokes an externally configured COLMAP installation.
4. The workspace records the expected output artifacts and process information.
5. Artifact validation checks the result before the workflow reports success.

The public application does not bundle COLMAP or claim that the reconstruction
backend is self-contained. A local COLMAP installation is an operator
configuration, not a repository asset.

## Public implementation boundary

The C++ / Qt source includes the controller, task, workspace and artifact
validation boundaries. The accompanying Qt tests cover project state, image
assets, backend discovery and reconstruction orchestration. External-backend
smoke checks are opt-in and use `VISION3D_COLMAP_ROOT` when an installation is
available.

## Reproducibility limits

The workflow depends on the selected COLMAP build, Qt/MSVC toolchain, input
images and machine resources. A public source checkout alone is therefore not
equivalent to a complete reconstruction runtime or a fixed benchmark.
