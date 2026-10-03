param(
    [Parameter(Mandatory=$true)][string]$Executable,
    [Parameter(Mandatory=$true)][string]$Model,
    [Parameter(Mandatory=$true)][string]$Workspace
)
$ErrorActionPreference = 'Stop'
$workspacePath = [IO.Path]::GetFullPath($Workspace)
if (!$workspacePath.StartsWith('D:\',[StringComparison]::OrdinalIgnoreCase)) { throw 'Smoke workspace must be on D:.' }
if (Test-Path -LiteralPath $workspacePath) { throw 'Use a fresh smoke workspace; existing evidence is preserved.' }
$Executable = (Resolve-Path -LiteralPath $Executable).Path
$Model = (Resolve-Path -LiteralPath $Model).Path
$inbox = "$workspacePath\client_to_ai"
$outbox = "$workspacePath\ai_to_client"
foreach ($dir in @($inbox,$outbox,"$workspacePath\temp")) { New-Item -ItemType Directory -Force $dir | Out-Null }
$env:TEMP = "$workspacePath\temp"; $env:TMP = $env:TEMP
$env:PSModuleAnalysisCachePath = "$workspacePath\temp\ps-module-cache"
$originalHash = (Get-FileHash -LiteralPath $Model).Hash
$size = (Get-Item -LiteralPath $Model).Length
$utf8 = New-Object Text.UTF8Encoding($false)
function Assert([bool]$condition,[string]$message) { if (!$condition) { throw $message } }
function Write-Request([string]$id) {
    New-Item -ItemType Directory "$inbox\$id" | Out-Null
    $request = @{protocolVersion=1;jobId=$id;type='mesh-copy';meshFile='final-mesh.ply';meshSize=$size;sha256=$originalHash}
    [IO.File]::WriteAllText("$inbox\$id\request.json",($request | ConvertTo-Json),$utf8)
}
function Get-Status([string]$id) {
    if (Test-Path "$outbox\$id\response.json") {
        return (Get-Content "$outbox\$id\response.json" -Raw -Encoding UTF8 | ConvertFrom-Json).status
    }
    return ''
}
function Run-Once([string]$name) {
    & $Executable --inbox $inbox --outbox $outbox --once 2>&1 | Out-File "$workspacePath\$name.log" -Encoding utf8
    return $LASTEXITCODE
}
Write-Request 'AIR-0001'
$worker = $null
try {
    Assert ((Run-Once 'before-ready') -eq 2) 'No-ready --once must exit 2.'
    Assert ((Get-Status 'AIR-0001') -eq 'queued') 'Request should be queued.'
    Assert (!(Test-Path "$outbox\AIR-0001\refined-mesh.ply")) 'Processed before READY.'
    $arguments = @('--inbox',('"'+$inbox+'"'),'--outbox',('"'+$outbox+'"'),'--serve')
    $worker = Start-Process -FilePath $Executable -ArgumentList $arguments -WorkingDirectory $workspacePath -WindowStyle Hidden -PassThru -RedirectStandardOutput "$workspacePath\serve.log" -RedirectStandardError "$workspacePath\serve-error.log"
    [IO.File]::WriteAllText("$inbox\AIR-0001\READY",'',$utf8)
    Start-Sleep -Milliseconds 2200
    Assert (!$worker.HasExited) 'Serve process did not stay running.'
    Assert ((Get-Status 'AIR-0001') -eq 'queued') 'READY without model must wait.'
    Assert (!(Test-Path "$outbox\AIR-0001\refined-mesh.ply")) 'Result appeared without model.'
    $input = [IO.File]::OpenRead($Model)
    try {
        $buffer = New-Object byte[] 4096
        [void]$input.Read($buffer,0,$buffer.Length)
        [IO.File]::WriteAllBytes("$inbox\AIR-0001\final-mesh.ply",$buffer)
    } finally { $input.Dispose() }
    Start-Sleep -Milliseconds 2200
    Assert ((Get-Status 'AIR-0001') -eq 'queued') 'Partial model must keep waiting.'
    Copy-Item -LiteralPath $Model -Destination "$workspacePath\temp\full-model.ply"
    Move-Item -LiteralPath "$workspacePath\temp\full-model.ply" -Destination "$inbox\AIR-0001\final-mesh.ply" -Force
    for ($i=0; $i -lt 60 -and (Get-Status 'AIR-0001') -ne 'completed'; $i++) { Start-Sleep -Milliseconds 250 }
    Assert ((Get-Status 'AIR-0001') -eq 'completed') 'Complete model was not processed.'
    $outputHash = (Get-FileHash "$outbox\AIR-0001\refined-mesh.ply").Hash
    Assert ($outputHash -eq $originalHash) 'Returned hash differs from original.'
    $response = Get-Content "$outbox\AIR-0001\response.json" -Raw -Encoding UTF8 | ConvertFrom-Json
    Assert ($response.sha256 -eq $originalHash -and $response.resultFile -eq 'refined-mesh.ply') 'Response metadata mismatch.'
    $before = (Get-Item "$outbox\AIR-0001\response.json").LastWriteTimeUtc.Ticks
    $modelBefore = (Get-Item "$outbox\AIR-0001\refined-mesh.ply").LastWriteTimeUtc.Ticks
    Start-Sleep -Milliseconds 2500
    Assert ((Get-Item "$outbox\AIR-0001\response.json").LastWriteTimeUtc.Ticks -eq $before) 'Repeated scan rewrote completed response.'
    Assert ((Get-Item "$outbox\AIR-0001\refined-mesh.ply").LastWriteTimeUtc.Ticks -eq $modelBefore) 'Repeated scan rewrote model.'
    Stop-Process -Id $worker.Id
    $worker.WaitForExit()
    Assert ((Run-Once 'restart-completed') -eq 2) 'Restart reprocessed completed job.'
    Assert ((Get-Item "$outbox\AIR-0001\response.json").LastWriteTimeUtc.Ticks -eq $before) 'Restart rewrote completed state.'
    $processingCount = @(Select-String -Path "$workspacePath\serve.log" -Pattern ' AIR-0001 processing$').Count
    Assert ($processingCount -eq 1) 'Expected exactly one processing event.'

    # A second task in the same smoke exercises ready-task --once completion.
    Write-Request 'AIR-0002'
    Copy-Item -LiteralPath $Model -Destination "$inbox\AIR-0002\final-mesh.ply"
    [IO.File]::WriteAllText("$inbox\AIR-0002\READY",'',$utf8)
    Assert ((Run-Once 'ready-once') -eq 0) 'Ready --once must exit 0.'
    Assert ((Get-Status 'AIR-0002') -eq 'completed') 'Ready --once did not complete.'
    Assert ((Get-FileHash "$outbox\AIR-0002\refined-mesh.ply").Hash -eq $originalHash) 'Once output hash mismatch.'
    Assert ((Get-FileHash "$inbox\AIR-0001\final-mesh.ply").Hash -eq $originalHash) 'Inbox model changed.'
    Assert ((Get-FileHash -LiteralPath $Model).Hash -eq $originalHash) 'Original model changed.'
    @{result='PASS';inputBytes=$size;inputSha256=$originalHash;outputSha256=$outputHash;readyGate=$true;earlyReadyWait=$true;partialModelWait=$true;completed=$true;repeatScanSkipped=$true;restartSkipped=$true;onceReadyCompleted=$true;processingCount=$processingCount;realNetworkTest=$false} | ConvertTo-Json | Tee-Object -FilePath "$workspacePath\smoke-result.json"
} finally {
    if ($worker -and !$worker.HasExited) { Stop-Process -Id $worker.Id; $worker.WaitForExit() }
}
