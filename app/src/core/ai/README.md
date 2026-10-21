# Client AI Job component (Stage 7-1C)

`AiJobClient` is a Qt Core static library linked by the existing Inspector
target. It has no UI, database, network layer or processing implementation.
The caller supplies two existing absolute directories, owns polling and decides
when to adopt a verified result. No application data directory is guessed.

```cpp
AiJobClient jobs(clientToAiDirectory, aiToClientDirectory);
QString error;
auto submitted = jobs.submit(existingFinalMesh, error);
if (submitted.jobId.isEmpty()) {
    // Submission failed. No READY was published; retain files for diagnosis.
    return;
}
// Store the job ID and poll later. Local publication is not remote delivery.
auto result = jobs.checkResult(submitted.jobId);
if (result.state == AiJobClient::State::Completed) {
    // result.filePath is now complete and verified against the response.
    // Return it to the calling workflow; do not overwrite the original model.
}
```

## One existing protocol

The request is exactly Stage 7-1B v1: protocolVersion=1, a unique
`AIR-<UUID>` jobId, type=`mesh-copy`, meshFile=`final-mesh.ply`, meshSize and
sha256. Each Job has its own directory under client_to_ai. Copy and hash the
model, verify the staged file, atomically publish request.json, then atomically
publish READY last. The original file is read only. Partial local submissions
remain unready and are not treated as completed submissions.

Responses come from `ai_to_client/<jobId>/response.json`. Missing/incomplete
JSON waits. queued, processing and failed are exposed to the caller; failed
includes the Worker's short error. Completed is accepted only after validating
version, job ID, a safe relative result filename, local file size and SHA-256.
Only an accepted result contains filePath. Unsafe paths and malformed complete
response fields report Failed; missing/partial files or hash mismatch report
Waiting so Syncthing can finish delivery. There is no arbitrary sync timeout.

**Size compatibility:** the unchanged Stage 7-1B Worker response has no result
size field. For its mesh-copy tasks, use request.meshSize as the expected output
size. If a v1 response includes `resultSize`, that value takes precedence.
A future non-copy response must supply resultSize; its size is not inferred from
the input. This optional field is an additive extension, not a second protocol.
The current Worker and JobProcessor are not changed in this stage.

**Hash compatibility:** always validate against response.sha256. The component
does not require output SHA-256 to equal input SHA-256. The end-to-end smoke
asserts equality only because the current Worker performs an unchanged copy.
Future mesh-refinement submission/processing is not implemented here.

Root directories must be disjoint. Job and result paths use the same single
component restrictions as the Worker; canonical checks reject escaping links
and junctions. The receive tree is read only. A fresh AiJobClient can check an
existing job using its retained request.json; no database is required.

## Bounded end-to-end smoke

`tests/ai_job_smoke` is a minimal test entry, not another business application.
It builds the same component library as Inspector, invokes the **existing**
Vision3DLocalAI.exe via --once, and verifies a real TempleRing model locally.
The host outbox is isolated from the client's receive directory to simulate
response-before-model delivery without touching Syncthing or its existing PoC.
It verifies that publication is not completion, completed-without-model waits,
a partial model waits, and the full model is accepted only after verification.

Build only this test entry with the existing MSVC x64 / Qt 6.5+ kit:

```powershell
# All selected source/build/temp/test paths must be in the approved D: workspace.
$env:TEMP = $tempDir
$env:TMP = $tempDir
& $cmake -S "$sourceRoot/tests/ai_job_smoke" -B $buildDir -G 'Visual Studio 17 2022' -A x64 "-DCMAKE_PREFIX_PATH=$qtKit"
& $cmake --build $buildDir --config Release --target Vision3DAiJobSmoke -- /m:1 /nodeReuse:false
# Place the existing matching Qt6Core.dll beside the smoke EXE for local running.
& $smokeExe $existingWorkerExe $existingTempleRingMesh $freshSmokeWorkspace
```

The worker has its own Qt runtime beside its existing executable. Neither it
nor the client GUI needs to be rebuilt by this smoke. The main client CMake
adds/links this component, but no formal QML page or client action is connected
in this stage. A full Inspector GUI build/regression is outside this small
component smoke.

Stage 7-1A's package, configuration, identity, database and directories are
unchanged. Real network transfer remains WAITING FOR HOST. Local directory
delivery cannot establish public-network connectivity or host deployment.
