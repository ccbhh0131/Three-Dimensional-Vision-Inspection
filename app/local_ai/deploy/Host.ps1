param([ValidateSet('Start','Stop','Status','Pair')][string]$Action='Start')
$ErrorActionPreference='Stop'
$root=$PSScriptRoot
if ($root -notmatch '^[dD]:\\') { throw 'Extract the complete package to D: before running.' }
if (![Environment]::Is64BitProcess) { throw 'Use 64-bit Windows PowerShell.' }
# Refuse redirected package/data roots before creating any files.
function Check-Directory([string]$path) {
    $item=Get-Item -LiteralPath $path -ErrorAction SilentlyContinue
    while ($item) {
        if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Use ordinary D: directories, not junctions or symbolic links.' }
        $item=$item.Parent
    }
}
Check-Directory $root
Check-Directory "$root\data"
foreach ($dir in @('data','data/config','data/index','data/client_to_ai','data/ai_to_client','data/logs','data/temp')) {
    $path=Join-Path $root $dir
    Check-Directory $path
    New-Item -ItemType Directory -Force -Path $path | Out-Null
}
$env:TEMP="$root\data\temp"; $env:TMP=$env:TEMP
$env:PSModuleAnalysisCachePath="$root\data\temp\ps-module-cache"
Set-Location -LiteralPath $root
$config="$root\data\config"
$syncthing="$root\runtime\syncthing\syncthing.exe"
$worker="$root\app\Vision3DLocalAI.exe"
$api='http://127.0.0.1:18484/rest'
$statePath="$root\data\processes.json"
$headers=$null
$selfId=''
function Load-Identity {
    [xml]$identityConfig=Get-Content "$config\config.xml" -Raw -Encoding UTF8
    $script:headers=@{'X-API-Key'=[string]$identityConfig.configuration.gui.apikey}
    $script:selfId=(Get-Content "$config\device-id.txt" -Raw).Trim()
}
function Api([string]$path) { Invoke-RestMethod "$api/$path" -Headers $headers -TimeoutSec 5 }
function Owned-Process($record) {
    if (!$record) { return $null }
    $p=Get-Process -Id $record.id -ErrorAction SilentlyContinue
    if ($p -and $p.Path -eq $record.path -and $p.StartTime.ToUniversalTime().Ticks -eq [long]$record.started) { return $p }
    return $null
}
function Save-State($st,$wk) {
    @{syncthing=@{id=$st.Id;path=$syncthing;started=$st.StartTime.ToUniversalTime().Ticks};worker=@{id=$wk.Id;path=$worker;started=$wk.StartTime.ToUniversalTime().Ticks};deviceId=$selfId} | ConvertTo-Json -Depth 4 | Set-Content $statePath -Encoding UTF8
}
function Stop-Owned {
    if (!(Test-Path $statePath)) { Write-Host 'No recorded processes to stop.'; return }
    $state=Get-Content $statePath -Raw -Encoding UTF8 | ConvertFrom-Json
    $wk=Owned-Process $state.worker
    if ($wk) { Stop-Process -Id $wk.Id; [void]$wk.WaitForExit(5000) }
    if (Test-Path "$config\device-id.txt") {
        Load-Identity
        try {
            $status=Api 'system/status'
            if ($status.myID -ne $selfId) { throw 'API device identity mismatch.' }
            Invoke-RestMethod -Method Post "$api/system/shutdown" -Headers $headers -TimeoutSec 5 | Out-Null
        } catch {
            if (Owned-Process $state.syncthing) { throw 'Syncthing shutdown failed; inspect this package logs. Other instances were not touched.' }
        }
    }
    $st=Owned-Process $state.syncthing
    if ($st -and !$st.WaitForExit(5000)) { throw 'Syncthing has not exited yet; retry Stop after reviewing logs.' }
    Write-Host 'Host processes stopped. Configuration, identity and tasks are retained.'
}
if ($Action -eq 'Stop') { Stop-Owned; return }
if ($Action -eq 'Status' -or $Action -eq 'Pair') {
    Load-Identity
    $status=Api 'system/status'
    if ($status.myID -ne $selfId) { throw 'API device identity mismatch.' }
    Write-Host "HOST DEVICE ID: $selfId"
    if ($Action -eq 'Status') {
        $state=Get-Content $statePath -Raw -Encoding UTF8 | ConvertFrom-Json
        if (!(Owned-Process $state.worker)) { throw 'Syncthing is reachable but Worker is not running.' }
        Write-Host 'Syncthing READY; Worker RUNNING. This does not prove remote synchronization.'
        return
    }
    $remoteId=(Read-Host 'Paste the CLIENT device ID obtained directly from its operator').Trim().ToUpper()
    if ($remoteId -notmatch '^[A-Z2-7]{7}(-[A-Z2-7]{7}){7}$' -or $remoteId -eq $selfId) { throw 'Invalid remote device ID.' }
    Write-Host "HOST:   $selfId"
    Write-Host "CLIENT: $remoteId"
    if ((Read-Host 'After comparing both IDs with the client operator, type PAIR to approve') -cne 'PAIR') { throw 'Pairing was not approved; no device added.' }
    $device=@{deviceID=$remoteId;name='Vision3D-approved-client';addresses=@('dynamic');autoAcceptFolders=$false;introducer=$false}
    Invoke-RestMethod -Method Post "$api/config/devices" -Headers $headers -ContentType 'application/json' -Body ($device | ConvertTo-Json) | Out-Null
    foreach ($folderId in @('vision3d-poc-client_to_ai','vision3d-poc-ai_to_client')) {
        $folder=Api "config/folders/$folderId"
        $devices=@($folder.devices)
        if ($devices.deviceID -notcontains $remoteId) { $devices+=@{deviceID=$remoteId} }
        Invoke-RestMethod -Method Patch "$api/config/folders/$folderId" -Headers $headers -ContentType 'application/json' -Body (@{devices=$devices} | ConvertTo-Json -Depth 5) | Out-Null
    }
    Write-Host 'Approved client added. The client must also approve this host and share both folders.'
    return
}

foreach ($name in @('Vision3DLocalAI.exe','Qt6Core.dll','msvcp140.dll','msvcp140_1.dll','vcruntime140.dll','vcruntime140_1.dll')) {
    if (!(Test-Path "$root\app\$name" -PathType Leaf)) { throw "Missing runtime: app\$name" }
}
if (!(Test-Path $syncthing -PathType Leaf)) { throw 'Missing Syncthing runtime.' }
if (Test-Path $statePath) {
    $old=Get-Content $statePath -Raw -Encoding UTF8 | ConvertFrom-Json
    if ((Owned-Process $old.worker) -or (Owned-Process $old.syncthing)) {
        throw 'This package already has recorded running processes. Use Status-Host.cmd or Stop-Host.cmd first.'
    }
}
# Port checks never stop or reconfigure an existing Syncthing instance.
foreach ($port in @(18484,22300)) {
    $listener=New-Object Net.Sockets.TcpListener([Net.IPAddress]::Loopback,$port)
    try { $listener.Start() } catch { throw "Port $port is occupied. No existing process was changed." } finally { $listener.Stop() }
}
$wv=& $worker --version
if ($LASTEXITCODE -ne 0) { throw 'Worker runtime check failed.' }
$sv=& $syncthing --version
if ($LASTEXITCODE -ne 0 -or $sv -notlike 'syncthing v2.1.5 *') { throw 'Syncthing version check failed.' }
Write-Host $wv
Write-Host $sv
if (!(Test-Path "$config\initialized")) {
    $password=[guid]::NewGuid().ToString('N')
    $password | & $syncthing generate --config $config --data "$root\data\index" --no-port-probing --gui-user vision3d --gui-password -
    if ($LASTEXITCODE) { throw 'Syncthing identity generation failed.' }
    [xml]$xml=Get-Content "$config\config.xml" -Raw -Encoding UTF8
    foreach ($folder in @($xml.configuration.SelectNodes('folder'))) { [void]$xml.configuration.RemoveChild($folder) }
    $selfId=$xml.configuration.device.id
    $xml.configuration.device.name='Vision3DLocalAI-Host'
    $xml.configuration.gui.address='127.0.0.1:18484'
    $xml.configuration.options.startBrowser='false'
    $xml.configuration.options.autoUpgradeIntervalH='0'
    $xml.configuration.options.urAccepted='-1'
    $xml.configuration.options.crashReportingEnabled='false'
    $xml.configuration.options.localAnnounceEnabled='false'
    $xml.configuration.options.natEnabled='false'
    $xml.configuration.options.listenAddress='tcp://0.0.0.0:22300'
    foreach ($address in @('quic://0.0.0.0:22300','dynamic+https://relays.syncthing.net/endpoint')) {
        $node=$xml.CreateElement('listenAddress'); $node.InnerText=$address; [void]$xml.configuration.options.AppendChild($node)
    }
    foreach ($name in @('client_to_ai','ai_to_client')) {
        $folder=$xml.configuration.defaults.folder.CloneNode($true)
        $folder.SetAttribute('id',"vision3d-poc-$name")
        $folder.SetAttribute('label',$name)
        $folder.SetAttribute('path',"$root\data\$name")
        $folder.SetAttribute('type',$(if($name -eq 'client_to_ai'){'receiveonly'}else{'sendonly'}))
        $folder.SetAttribute('rescanIntervalS','15')
        $folder.SetAttribute('fsWatcherDelayS','1')
        [void]$xml.configuration.AppendChild($folder)
    }
    $xml.Save("$config\config.xml")
    $selfId | Set-Content "$config\device-id.txt" -Encoding ASCII
    "GUI: http://127.0.0.1:18484`r`nUser: vision3d`r`nPassword: $password" | Set-Content "$config\GUI-credentials.txt" -Encoding UTF8
    '1' | Set-Content "$config\initialized" -Encoding ASCII
}
Load-Identity
# Initial defaults only; do not overwrite an operator's ignore policy on restart.
foreach ($name in @('client_to_ai','ai_to_client')) {
    $ignore="$root\data\$name\.stignore"
    if (!(Test-Path $ignore)) { Copy-Item "$root\sync-ignore.txt" $ignore }
}
$st=$null; $wk=$null
try {
    $stamp=Get-Date -Format 'yyyyMMdd-HHmmss'
    $arguments=@('serve','--config',('"'+$config+'"'),'--data',('"'+$root+'\data\index"'),'--no-browser','--no-upgrade','--no-restart','--no-port-probing','--log-file',('"'+$root+'\data\logs\syncthing.log"'))
    $st=Start-Process $syncthing -ArgumentList $arguments -WorkingDirectory $root -WindowStyle Hidden -PassThru -RedirectStandardOutput "$root\data\logs\syncthing-$stamp.out.log" -RedirectStandardError "$root\data\logs\syncthing-$stamp.err.log"
    $ready=$false
    for($i=0;$i -lt 30;$i++) {
        Start-Sleep -Seconds 1
        if($st.HasExited){throw 'Syncthing exited; read data/logs.'}
        try { $status=Api 'system/status'; if($status.myID -eq $selfId){$ready=$true;break} } catch {}
    }
    if(!$ready){throw 'Syncthing API did not become ready with the expected device ID.'}
    foreach($id in @('vision3d-poc-client_to_ai','vision3d-poc-ai_to_client')) {
        $health=Api "db/status?folder=$id"
        if($health.errors -or $health.pullErrors){throw "Syncthing folder error: $id"}
    }
    $arguments=@('--inbox',('"'+$root+'\data\client_to_ai"'),'--outbox',('"'+$root+'\data\ai_to_client"'),'--serve')
    $wk=Start-Process $worker -ArgumentList $arguments -WorkingDirectory "$root\app" -WindowStyle Hidden -PassThru -RedirectStandardOutput "$root\data\logs\worker-$stamp.out.log" -RedirectStandardError "$root\data\logs\worker-$stamp.err.log"
    Start-Sleep -Seconds 1
    if($wk.HasExited){throw 'Worker exited; read data/logs/worker-*.err.log.'}
    Save-State $st $wk
    Write-Host 'READY: Syncthing and Worker started in the background.'
    Write-Host "HOST DEVICE ID: $selfId"
    Write-Host 'No remote device is automatically trusted. Use Pair-Client.cmd after verifying device IDs.'
    Write-Host 'Use Status-Host.cmd to inspect, Stop-Host.cmd to stop. Keep data/ when upgrading.'
} catch {
    if($wk -and !$wk.HasExited){Stop-Process -Id $wk.Id}
    if($st -and !$st.HasExited){
        try { $s=Api 'system/status'; if($s.myID -eq $selfId){Invoke-RestMethod -Method Post "$api/system/shutdown" -Headers $headers -TimeoutSec 5 | Out-Null} } catch {}
        if(!$st.WaitForExit(5000)){Stop-Process -Id $st.Id}
    }
    throw
}
