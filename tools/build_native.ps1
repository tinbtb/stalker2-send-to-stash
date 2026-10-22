param([string]$Zig = $env:STASH_ZIG)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
if (-not $Zig) {
    $command = Get-Command zig -ErrorAction SilentlyContinue
    if ($command) { $Zig = $command.Source }
}
if (-not $Zig) {
    $existing = Join-Path (Split-Path $root -Parent) 'rea_test\tools'
    $Zig = Get-ChildItem $existing -Directory -Filter 'zig-x86_64-windows-*' -ErrorAction SilentlyContinue |
        ForEach-Object { Join-Path $_.FullName 'zig.exe' } | Where-Object { Test-Path $_ } | Select-Object -First 1
}
if (-not $Zig) { throw 'Provide -Zig <path to zig.exe> or set STASH_ZIG.' }
$out = Join-Path $root 'build'
New-Item -ItemType Directory -Force $out | Out-Null
$env:ZIG_GLOBAL_CACHE_DIR = Join-Path $out 'zig-cache'
$env:ZIG_LOCAL_CACHE_DIR = Join-Path $out 'zig-local-cache'
Push-Location $root
try {
    $sources = @('src/send_to_stash.c','src/address_resolver.c','third_party/minhook/src/buffer.c','third_party/minhook/src/hook.c','third_party/minhook/src/trampoline.c','third_party/minhook/src/hde/hde64.c')
    & $Zig cc -target x86_64-windows-gnu -std=c11 -shared -O2 -g -Wall -Wextra -Wno-cast-function-type $sources -Isrc -Ithird_party/minhook/include -lbcrypt -lkernel32 -luser32 -o build/SendToStash.asi
    if ($LASTEXITCODE -ne 0) { throw 'ASI compilation failed.' }
    & $Zig cc -target x86_64-windows-gnu -std=c11 -O2 tools/guard_probe.c -municode -o build/guard_probe.exe
    if ($LASTEXITCODE -ne 0) { throw 'Guard probe compilation failed.' }
    Get-FileHash build/SendToStash.asi -Algorithm SHA256 | ForEach-Object { Write-Host "ASI SHA256: $($_.Hash)" }
} finally { Pop-Location }
