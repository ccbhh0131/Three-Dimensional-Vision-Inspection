# Vision3D Local AI worker

Independent Windows x64 / C++17 / Qt 6 Core executable. No Inspector classes,
QML, geometry libraries, AI models, web server, or database. Build this directory
directly; the client CMake project is unchanged.

## Build and run

Use the existing MSVC x64 toolchain and Qt 6.5+ MSVC kit. Keep source, build,
temporary files, logs and runtime data under the approved D: workspace.
Set TEMP/TMP for the build process only; do not change system settings.

```powershell
# Set these variables to absolute locations under your approved D: workspace.
$env:TEMP = $tempDir
$env:TMP = $tempDir
& $cmake -S $sourceDir -B $buildDir -G 'Visual Studio 17 2022' -A x64 "-DCMAKE_PREFIX_PATH=$qtKit"
& $cmake --build $buildDir --config Release --target Vision3DLocalAI -- /m:1 /nodeReuse:false
# For local execution, place the existing kit's Qt6Core.dll next to the EXE.
& $exe --inbox $inbox --outbox $outbox --once
& $exe --inbox $inbox --outbox $outbox --serve
```

Both absolute directory arguments are mandatory; there are **no implicit data
directories**. Inbox must exist. Outbox may be created; the two trees must be
disjoint, including after resolving junctions. Use D: paths for this project.
The worker reads inbox only and writes to outbox only. Logs go to stdout/stderr;
redirect them to D: as needed. `--serve` polls once per second, processing at most
one task per scan. Stop with Ctrl+C. A small `.worker.lock` in outbox prevents
two worker instances from using the same output directory.

`--once` scans once, skipping waiting and terminal tasks, processes at most one
eligible task, and exits: 0 completed; 1 processing/configuration error; 2 no
ready task. A persistent `failed` task requires a new job ID to retry. Completed
job IDs are immutable and must not be reused, even with a different request.

## Transport compatibility and v1 task envelope

Stage 7-1A actually uses root-level `final-mesh.ply`, `refined-mesh.ply`, a
`model-manifest.json` with `bytes`/`sha256`, and transfer receipts. It has **no**
Job directory, READY, protocolVersion or request/response format to reuse.
This worker adds a minimal v1 task envelope while retaining the directory
directions, PLY naming and SHA-256 semantics. Per the Stage 7-1B requirement,
`meshSize` is the request equivalent of the old manifest's `bytes`.

The old root-level single-file PoC is not a worker Job and is ignored. No old
script, Syncthing setting, ZIP or in-progress transfer is changed. The existing
copy script is not automatically upgraded to this protocol. A future sender
must create the task envelope below; this stage does not implement that client.

```text
client_to_ai/                  # client sends; host reads only
  AIR-0001/
    request.json
    final-mesh.ply
    READY
ai_to_client/                  # host writes; client receives
  AIR-0001/
    response.json
    refined-mesh.ply           # published only after copy verification
```

Example request.json (meshSize and hash must describe the actual model):

```json
{
  "protocolVersion": 1,
  "jobId": "AIR-0001",
  "type": "mesh-copy",
  "meshFile": "final-mesh.ply",
  "meshSize": 10074461,
  "sha256": "ba6ff2a3ce50f82178750523f8c9ce8f924c877e2ab126bf02b4b5178b64d1d8"
}
```

Use a unique job ID matching `[A-Za-z0-9][A-Za-z0-9_-]{0,63}`, identical to the
directory name. meshFile must be one ASCII filename matching
`[A-Za-z0-9][A-Za-z0-9_-]*\.ply`, not an absolute path or nested path. Request,
input and output path checks reject escaping symlinks/junctions. Write request
JSON atomically and READY last on the sender, then keep that task immutable.
READY can be empty. Syncthing delivery order is not assumed.

Response fields: protocolVersion, jobId, status, resultFile, sha256, error,
updatedAt (UTC). For queued/processing/failed, resultFile and sha256 are empty.
`failed` has a short error. `completed` has `resultFile: "refined-mesh.ply"`
and the verified lowercase output hash. All response paths are relative to
that outbox job directory.

Flow:

1. Discover task. Missing/unparseable JSON waits for synchronization.
2. Publish queued and wait for READY. Once READY exists, reject unsupported
   or invalid request fields as failed.
3. Missing/unreadable/wrong-size/wrong-hash input stays queued and is retried.
   There is no arbitrary timeout: a permanently wrong input continues waiting
   until corrected. No processing is started based on READY alone.
4. Publish processing. `processModel(inputMesh, outputMesh)` currently only
   streams a byte-for-byte copy to a staging file via QSaveFile.
5. Re-read and verify staging size/hash, then use a same-directory Windows
   rename without copy fallback or overwrite to publish refined-mesh.ply.
6. Publish completed via QSaveFile, after the complete model exists. Repeated
   scans and process restarts skip terminal responses. An interrupted processing
   task can reuse an already published result only if its hash and size match.

**Client rule:** Syncthing can deliver completed before the result file even
though the host published them in order. A client must wait for completed AND
the result file, then verify its SHA-256 before offering the result to the user.
Ignore `.part`, temporary files and `.worker.lock`. No remote ordering guarantee
is claimed by this implementation.

## Bounded local smoke

`smoke.ps1` is one small standalone smoke, not a test framework. Supply the built
EXE, an existing model and a fresh D: workspace. It checks early READY, partial
arrival, completion/hash equality, repeated scans, restart idempotency, and both
run modes. It uses two tasks in one run so `--serve` and ready `--once` each
execute a task. Evidence is retained in that workspace; original input is kept.

```powershell
& .\smoke.ps1 -Executable $exe -Model $existingModel -Workspace $freshSmokeDir
```

This is local filesystem simulation only. Stage 7-1A remains WAITING FOR HOST;
real cross-network transfer and host deployment are NOT YET TESTED.

## Future processing and deployment

Replace `JobProcessor::processModel()` with the Qwen/Blender workflow when that
stage is authorized. Keep inbox read-only and staged output publication. When
refinement starts changing bytes, replace the current copy-equality check with
actual output validation and hash the refined output for the response.

A later host bundle needs this Release EXE, matching Qt6Core.dll, the required
MSVC runtime and licenses, explicit D: launch paths, and the approved Syncthing
runtime/configuration workflow. Validate it on a clean host without development
tools. This stage builds the executable only; it does not make an installer,
install services, or change the Stage 7-1A bundle.
