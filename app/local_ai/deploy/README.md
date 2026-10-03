# Vision3DLocalAI-Host — Stage 7-2

Windows 10/11 x64 portable host. Requires only the Windows OS, built-in Windows
PowerShell 5.1 and this complete package. No Inspector, development tools, AI
model, Blender or processing plugin is included. Current task type: mesh-copy.

## Start on the host

1. Save and fully extract the ZIP on D:, for example `D:\Vision3DLocalAI\`.
   Keep all app/runtime/license/script files and empty data directories together.
   Ensure the downloader/extractor also uses D: for its temporary files. Do not
   run directly inside the ZIP or use a C:-targeting junction.
2. Double-click **Start-Host.cmd** as a normal user. It checks runtime versions,
   creates private configuration if needed, starts Syncthing and the Worker in
   the background, and prints READY plus HOST DEVICE ID. Any failure has a
   nonzero exit and an error; logs are in data/logs. No browser starts.
3. **Status-Host.cmd** checks both processes and shows the device ID.
   **Stop-Host.cmd** stops only this package's recorded processes. Closing the
   startup console does not stop the background processes. Do not launch this
   same package twice; stop before upgrading or moving it.

No Windows service, startup task, firewall rule, registry or system environment
change is made. TEMP/TMP and PowerShell cache are process-local under data/temp.
Windows may ask about network access; the scripts never approve that prompt.
If declined, outbound Relay may still work; report failures instead of turning
off the firewall. API port 18484 and transfer TCP/QUIC 22300 must be available.
These ports differ from Stage 7-1A's 18384/22200.

## First-run pairing: explicit approval on both sides

The package has **no device keys and no pretrusted laptop ID**. First run creates
a unique host identity under data/config, reused after stop/start.

1. Compare the HOST DEVICE ID shown by Status-Host.cmd with the client operator.
2. Obtain that client's device ID directly from its operator. On the host run
   **Pair-Client.cmd**, paste it, compare both printed IDs and type **PAIR**.
   Any other answer makes no pairing change. Auto-accept and introducer are off.
3. The client must explicitly add/approve the host device and share these two
   folder IDs using the opposite directions. Keep the exact IDs consistent:

| Directory | Folder ID | Host | Client |
|---|---|---|---|
| client_to_ai | vision3d-poc-client_to_ai | Receive Only | Send Only |
| ai_to_client | vision3d-poc-ai_to_client | Send Only | Receive Only |

4. On the client use its Syncthing local management or its approved pairing
   tooling. This host package does not reconfigure any laptop. Keep the existing
   Stage 7-1A test instance/package separate and unchanged during validation.
5. Confirm the device connection and folders before submitting a v1 Job through
   AiJobClient. The host processes request.json + READY + verified input. A
   root-level PLY alone is not a Job. Completed is not proof the result has
   reached the client; it must also wait for the result and verify its hash.

Optional local GUI: http://127.0.0.1:18484, random credentials saved in
data/config/GUI-credentials.txt. It is loopback-only and requires authentication.
Never publish this API on a public address. The normal host workflow requires
no browser; any manually chosen browser must use a D: data/cache profile.

## Persistence and upgrades

```text
app/                 Worker, Qt6Core, four app-local MSVC DLLs
runtime/syncthing/   official Syncthing executable
licenses/            project, Qt, Qt third-party, Microsoft, Syncthing notices
data/config/         device keys, config, private GUI credentials
data/index/          Syncthing's own index (not an application database)
data/client_to_ai/    received immutable Job inputs
data/ai_to_client/    Worker responses/results
data/logs/           process logs
data/temp/           process temporary files/cache
```

For upgrade: stop this package, back up data/, then replace app/, runtime/,
licenses/ and top-level scripts/docs from the new package. **Retain data/** and
never replace its keys/config/index/tasks with those from another machine.
No upgrade action is automatic. Keep the installed root fixed once initialized;
Syncthing saves absolute folder paths. Relocation requires a deliberate config
path update while stopped; do not move a running installation.

Only the two data transport directories are shared. Logs/config/index are
outside them. On first start, sync-ignore.txt becomes each folder's .stignore
and excludes Worker lock files, .part files, logs and atomic-write leftovers.
Existing ignore files are retained on later starts.

The Worker has serial processing and persistent completed-task deduplication.
It does not contain AI, UI, REST business APIs or a task database.

## Validation and installer boundary

The supplied package is for a real host to run without development tools.
Local package smoke is not remote-host installation or Internet validation.
Cross-network transport remains WAITING FOR HOST until both real devices pair
and a file round trip is verified.

An EXE installer is PENDING if no Inno Setup/NSIS/WiX compiler is already present.
No tool is installed automatically. A later approved D:-portable Inno Setup tool
can produce a selectable-D: installer with user data excluded from replacement.
This stage delivers the full portable ZIP and its SHA-256 first, without a
GitHub Release.

For maintainers: Build-Portable.ps1 takes explicit paths to an existing Release
Worker, matching Qt kit/source license tree, official VC Redist CRT directory,
verified Syncthing v2.1.5 runtime/archive, project/Microsoft licenses and a fresh
D: output directory. It copies only the runtime/license allowlist, creates empty
data directories, launch scripts, a manifest and ZIP. It does not compile or
install tools. The build script itself is kept in Git, not in the runtime ZIP.
