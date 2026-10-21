param(
    [Parameter(Mandatory=$true)][string]$Zip,
    [Parameter(Mandatory=$true)][string]$Model,
    [Parameter(Mandatory=$true)][string]$Workspace
)
$ErrorActionPreference='Stop'
$base=[IO.Path]::GetFullPath($Workspace)
if($base -notmatch '^[dD]:\\' -or (Test-Path $base)){throw 'Use a fresh D: smoke workspace.'}
New-Item -ItemType Directory -Force "$base\temp" | Out-Null
$env:TEMP="$base\temp"; $env:TMP=$env:TEMP
$env:PSModuleAnalysisCachePath="$base\temp\ps-module-cache"
Add-Type -AssemblyName System.IO.Compression.FileSystem
$archive=[IO.Compression.ZipFile]::OpenRead($Zip)
try {
    $entries=@($archive.Entries | Select-Object FullName,Length)
    if(@($entries | Where-Object {$_.FullName -match '\.(pem|ply|db|log)$' -or ($_.Length -gt 0 -and $_.FullName -match '/data/')}).Count){throw 'Package contains user/runtime data.'}
    $entries | ConvertTo-Json | Set-Content "$base\zip-entries.json" -Encoding UTF8
} finally {$archive.Dispose()}
[IO.Compression.ZipFile]::ExtractToDirectory($Zip,$base)
$hostRoot="$base\Vision3DLocalAI-Host"
$manifest=Get-Content "$hostRoot\package-manifest.json" -Raw -Encoding UTF8 | ConvertFrom-Json
foreach($entry in $manifest.files){if((Get-FileHash -LiteralPath (Join-Path $hostRoot $entry.path)).Hash -ne $entry.sha256){throw 'Extracted file hash mismatch.'}}
function Check([bool]$ok,[string]$reason){if(!$ok){throw $reason}}
function Launch([string]$script,[string]$log){
    # Wait for the launcher only, not its deliberately persistent process tree.
    $arguments='/d /c ""'+$hostRoot+'\'+$script+'.cmd" --no-pause"'
    $launcher=Start-Process "$env:SystemRoot\System32\cmd.exe" -ArgumentList $arguments -WorkingDirectory $hostRoot -WindowStyle Hidden -PassThru -RedirectStandardOutput "$base\$log" -RedirectStandardError "$base\$log.err"
    if(!$launcher.WaitForExit(45000)){throw "$script launcher timed out; see $log"}
    if($launcher.ExitCode){throw "$script failed; see $log"}
}
$started=$false
try {
    Launch 'Start-Host' 'start.log'
    $started=$true
    Launch 'Status-Host' 'status.log'
    $state=Get-Content "$hostRoot\data\processes.json" -Raw -Encoding UTF8 | ConvertFrom-Json
    $workerProcess=Get-Process -Id $state.worker.id
    $modules=@($workerProcess.Modules | Select-Object ModuleName,FileName)
    $modules | ConvertTo-Json | Set-Content "$base\worker-modules.json" -Encoding UTF8
    foreach($name in @('Qt6Core.dll','msvcp140.dll','msvcp140_1.dll','vcruntime140.dll','vcruntime140_1.dll')) {
        $dll=@($modules | Where-Object ModuleName -eq $name)
        Check ($dll.Count -eq 1 -and $dll[0].FileName -eq "$hostRoot\app\$name") "Runtime not loaded from package: $name"
    }
    [xml]$config=Get-Content "$hostRoot\data\config\config.xml" -Raw -Encoding UTF8
    Check (@($config.configuration.device).Count -eq 1) 'Fresh host unexpectedly trusts another device.'
    Check ($config.configuration.gui.address -eq '127.0.0.1:18484') 'GUI is not loopback-only.'
    $headers=@{'X-API-Key'=[string]$config.configuration.gui.apikey}
    $api='http://127.0.0.1:18484/rest'
    $paths=Invoke-RestMethod "$api/system/paths" -Headers $headers
    $paths | ConvertTo-Json | Set-Content "$base\syncthing-paths.json" -Encoding UTF8
    $listeners=@(Get-NetTCPConnection -LocalPort 18484 -State Listen)
    Check ($listeners.Count -eq 1 -and $listeners[0].LocalAddress -eq '127.0.0.1') 'Unexpected API listener.'
    $http=0
    try{Invoke-WebRequest "$api/system/status" -UseBasicParsing | Out-Null}catch{$http=[int]$_.Exception.Response.StatusCode}
    Check ($http -eq 403) 'Unauthenticated API was not rejected.'
    $id=(Get-Content "$hostRoot\data\config\device-id.txt" -Raw).Trim()
    $keyHash=(Get-FileHash "$hostRoot\data\config\key.pem").Hash
    $hash=(Get-FileHash -LiteralPath $Model).Hash
    $size=(Get-Item -LiteralPath $Model).Length
    $job='AIR-PACKAGE-0001'
    $input="$hostRoot\data\client_to_ai\$job"
    $output="$hostRoot\data\ai_to_client\$job"
    New-Item -ItemType Directory $input | Out-Null
    Copy-Item -LiteralPath $Model "$input\final-mesh.ply"
    @{protocolVersion=1;jobId=$job;type='mesh-copy';meshFile='final-mesh.ply';meshSize=$size;sha256=$hash} | ConvertTo-Json | Set-Content "$input\request.json" -Encoding UTF8
    [IO.File]::WriteAllBytes("$input\READY",[byte[]]@())
    $completed=$false
    for($i=0;$i -lt 60;$i++) {
        Start-Sleep -Milliseconds 250
        if(Test-Path "$output\response.json"){$r=Get-Content "$output\response.json" -Raw -Encoding UTF8 | ConvertFrom-Json; if($r.status -eq 'completed'){$completed=$true;break}; if($r.status -eq 'failed'){throw "Worker failed: $($r.error)"}}
    }
    Check $completed 'Package Worker did not complete the mesh-copy task.'
    $outputHash=(Get-FileHash "$output\refined-mesh.ply").Hash
    Check ($hash -eq $outputHash -and $r.sha256 -eq $hash) 'Result SHA-256 mismatch.'
    $ticks=(Get-Item "$output\response.json").LastWriteTimeUtc.Ticks
    $ignore=Invoke-RestMethod "$api/db/ignores?folder=vision3d-poc-ai_to_client" -Headers $headers
    Check ($ignore.ignore -contains '/.worker.lock' -and $ignore.ignore -contains '*.part') 'Missing sync exclusions.'
    Launch 'Stop-Host' 'stop.log'; $started=$false
    Launch 'Start-Host' 'restart.log'; $started=$true
    Start-Sleep -Seconds 2
    Check ((Get-Content "$hostRoot\data\config\device-id.txt" -Raw).Trim() -eq $id) 'Device ID changed on restart.'
    Check ((Get-FileHash "$hostRoot\data\config\key.pem").Hash -eq $keyHash) 'Device key changed on restart.'
    Check ((Get-Item "$output\response.json").LastWriteTimeUtc.Ticks -eq $ticks) 'Completed task was rewritten on restart.'
    Check ((Get-FileHash -LiteralPath $Model).Hash -eq $hash) 'Original model changed.'
    Launch 'Stop-Host' 'final-stop.log'; $started=$false
    @{result='PASS';inputBytes=$size;inputSha256=$hash;outputSha256=$outputHash;deviceId=$id;packageLocalDlls=$true;loopbackApi=$true;unauthenticatedHttpStatus=$http;noPretrustedPeers=$true;ignoreRulesLoaded=$true;identityPreserved=$true;completedJobPreserved=$true;remoteHostTest=$false;realInternetTransfer='WAITING FOR HOST'} | ConvertTo-Json | Tee-Object -FilePath "$base\smoke-result.json"
} finally {if($started){Launch 'Stop-Host' 'cleanup-stop.log'}}
