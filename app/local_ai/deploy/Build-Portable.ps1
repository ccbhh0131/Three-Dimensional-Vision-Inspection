param(
    [Parameter(Mandatory=$true)][string]$WorkerExe,
    [Parameter(Mandatory=$true)][string]$QtKit,
    [Parameter(Mandatory=$true)][string]$QtBaseSource,
    [Parameter(Mandatory=$true)][string]$CrtDirectory,
    [Parameter(Mandatory=$true)][string]$SyncthingDirectory,
    [Parameter(Mandatory=$true)][string]$SyncthingOfficialZip,
    [Parameter(Mandatory=$true)][string]$ProjectLicense,
    [Parameter(Mandatory=$true)][string]$MicrosoftLicense,
    [Parameter(Mandatory=$true)][string]$OutputDirectory
)
$ErrorActionPreference='Stop'
$out=[IO.Path]::GetFullPath($OutputDirectory)
if ($out -notmatch '^[dD]:\\') { throw 'Use a D: output directory.' }
$package=Join-Path $out 'Vision3DLocalAI-Host'
if(Test-Path $package){throw 'Package already exists; use a new output directory to preserve it.'}
New-Item -ItemType Directory -Force "$out\temp" | Out-Null
$env:TEMP="$out\temp"; $env:TMP=$env:TEMP
$env:PSModuleAnalysisCachePath="$out\temp\ps-module-cache"
$expected='39571E4D0900C2A2CAB14C0B170F49751340A869E49734CCC8079D9B98A7974B'
if((Get-FileHash $SyncthingOfficialZip).Hash -ne $expected){throw 'Official Syncthing archive digest mismatch.'}
if((Get-FileHash "$SyncthingDirectory\syncthing.exe").Hash -ne '36A0F7BC372F64FA7CC4F5654FA324C0DD9F7FEF2E07565E00C6E1CF73F50344'){throw 'Syncthing runtime differs from verified v2.1.5.'}
foreach($dir in @('app','runtime/syncthing','licenses/qt','licenses/qt-third-party','licenses/microsoft','licenses/syncthing','data/config','data/index','data/client_to_ai','data/ai_to_client','data/logs','data/temp')) {
    New-Item -ItemType Directory -Force (Join-Path $package $dir) | Out-Null
}
Copy-Item $WorkerExe "$package\app\Vision3DLocalAI.exe"
Copy-Item "$QtKit\bin\Qt6Core.dll" "$package\app\Qt6Core.dll"
foreach($name in @('msvcp140.dll','msvcp140_1.dll','vcruntime140.dll','vcruntime140_1.dll')) { Copy-Item (Join-Path $CrtDirectory $name) "$package\app\$name" }
Copy-Item "$SyncthingDirectory\syncthing.exe" "$package\runtime\syncthing\syncthing.exe"
Copy-Item "$SyncthingDirectory\LICENSE.txt","$SyncthingDirectory\AUTHORS.txt" "$package\licenses\syncthing\"
Copy-Item $ProjectLicense "$package\licenses\Vision3D-Apache-2.0.txt"
Copy-Item $MicrosoftLicense "$package\licenses\microsoft\Visual-Studio-2022-Community-License-EN.docx"
Copy-Item "$QtBaseSource\LICENSES\*.txt" "$package\licenses\qt\"
$attributions=@()
foreach($file in (Get-ChildItem "$QtBaseSource\src" -Filter qt_attribution.json -Recurse)) {
    foreach($entry in @(Get-Content $file.FullName -Raw -Encoding UTF8 | ConvertFrom-Json)) {
        if($entry.QDocModule -ne 'qtcore'){continue}
        $attributions+=$entry
        if($entry.LicenseFile) {
            $license=Join-Path $file.DirectoryName $entry.LicenseFile
            if(!(Test-Path $license)){throw "Missing third-party license: $($entry.Id)"}
            Copy-Item $license "$package\licenses\qt-third-party\$($entry.Id)-LICENSE.txt"
        }
    }
}
$attributions | ConvertTo-Json -Depth 20 | Set-Content "$package\licenses\qt-third-party\attributions.json" -Encoding UTF8
Copy-Item "$PSScriptRoot\Host.ps1","$PSScriptRoot\sync-ignore.txt","$PSScriptRoot\README.md","$PSScriptRoot\THIRD_PARTY_NOTICES.md" $package
$template=@'
@echo off
setlocal
cd /d "%~dp0"
if /i not "%CD:~0,3%"=="D:\" (
  echo ERROR: Extract the complete package to D:.
  pause
  exit /b 1
)
if not exist "data\temp\" (
  echo ERROR: Missing data\temp directory. Fully extract the package first.
  pause
  exit /b 1
)
set "TEMP=%CD%\data\temp"
set "TMP=%TEMP%"
set "PSModuleAnalysisCachePath=%TEMP%\ps-module-cache"
"%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -ExecutionPolicy Bypass -File "%CD%\Host.ps1" -Action @ACTION@
set "HOST_EXIT=%ERRORLEVEL%"
if not "%HOST_EXIT%"=="0" echo ERROR: @ACTION@ failed. Review the message above and data\logs.
if /i not "%~1"=="--no-pause" pause
exit /b %HOST_EXIT%
'@
foreach($item in @(@('Start-Host','Start'),@('Stop-Host','Stop'),@('Status-Host','Status'),@('Pair-Client','Pair'))) {
    [IO.File]::WriteAllText("$package\$($item[0]).cmd",($template.Replace('@ACTION@',$item[1]) -replace '\r?\n',"`r`n"),[Text.Encoding]::ASCII)
}
$manifest=@(Get-ChildItem $package -Recurse -File | ForEach-Object {
    @{path=$_.FullName.Substring($package.Length+1).Replace('\','/');bytes=$_.Length;sha256=(Get-FileHash -LiteralPath $_.FullName).Hash}
})
@{package='Vision3DLocalAI-Host';workerVersion='0.1.0';qtVersion='6.5.3';syncthingVersion='2.1.5';syncthingOfficialZipSha256=$expected;files=$manifest} | ConvertTo-Json -Depth 5 | Set-Content "$package\package-manifest.json" -Encoding UTF8
Add-Type -AssemblyName System.IO.Compression.FileSystem
$zip=Join-Path $out 'Vision3DLocalAI-Host-Portable-x64.zip'
[IO.Compression.ZipFile]::CreateFromDirectory($package,$zip,[IO.Compression.CompressionLevel]::Optimal,$true)
$hash=(Get-FileHash $zip).Hash
"$hash  $([IO.Path]::GetFileName($zip))" | Set-Content "$zip.sha256" -Encoding ASCII
@{packagePath=$package;portableZip=$zip;zipBytes=(Get-Item $zip).Length;sha256=$hash;installer='PENDING'} | ConvertTo-Json | Tee-Object -FilePath "$out\package-summary.json"
