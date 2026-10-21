param([string]$GameRoot)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
if (-not $GameRoot) {
    $settings = Get-Content (Join-Path $env:LOCALAPPDATA 'stalker2-modding-harness\settings.json') -Raw | ConvertFrom-Json
    $GameRoot = $settings.game
}
if (-not $GameRoot) { throw 'Game root is not configured.' }
$bin = Join-Path $GameRoot 'Stalker2\Binaries\Win64'
if (-not (Test-Path (Join-Path $bin 'Stalker2-Win64-Shipping.exe'))) { throw "Game executable not found in $bin" }
if (Get-Process 'Stalker2-Win64-Shipping' -ErrorAction SilentlyContinue) { throw 'Close the game before installing the ASI.' }
$source = Join-Path $root 'build\SendToStash.asi'
$verification = Get-Content (Join-Path $root 'build\static-verification.json') -Raw | ConvertFrom-Json
$sourceHash = (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash
if ($sourceHash -ne $verification.asi_sha256) { throw 'Build changed after verification.' }
$exeHash = (Get-FileHash -LiteralPath (Join-Path $bin 'Stalker2-Win64-Shipping.exe') -Algorithm SHA256).Hash
if ($exeHash -ne $verification.target_sha256) { throw 'Installed executable is not the supported build.' }
$loader = @('winhttp.dll','dinput8.dll','dsound.dll','version.dll','winmm.dll') | Where-Object {
    $path = Join-Path $bin $_
    (Test-Path $path) -and ((Get-Item $path).VersionInfo.FileDescription -match 'Ultimate ASI Loader')
}
if (-not $loader) { throw 'No verified existing Ultimate ASI Loader was detected; no loader DLL has been replaced.' }
$target = Join-Path $bin 'SendToStash.asi'
$backup = $null
if (Test-Path -LiteralPath $target) {
    $backupDir = Join-Path $root ('build\backups\' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
    New-Item -ItemType Directory -Force $backupDir | Out-Null
    $backup = Join-Path $backupDir 'SendToStash.asi'
    Copy-Item -LiteralPath $target -Destination $backup
}
Copy-Item -LiteralPath $source -Destination $target -Force
if ((Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash -ne $sourceHash) { throw 'Installed ASI hash differs.' }
[ordered]@{ target=$target; sha256=$sourceHash; prior_copy_backup=$backup; loader=($loader -join ', '); installed_at=(Get-Date -Format o) } |
    ConvertTo-Json | Set-Content (Join-Path $root 'build\installation.json')
Write-Host "Installed test ASI: $target"
Write-Host "Existing loader retained: $($loader -join ', ')"
