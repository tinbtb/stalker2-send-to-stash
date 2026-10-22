param(
    [Parameter(Mandatory)][string]$Baseline,
    [Parameter(Mandatory)][string]$DevelopmentFixture,
    [string]$Zig = $env:STASH_ZIG
)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$Baseline = (Resolve-Path -LiteralPath $Baseline).Path
$DevelopmentFixture = (Resolve-Path -LiteralPath $DevelopmentFixture).Path
if ((Get-FileHash -LiteralPath $Baseline -Algorithm SHA256).Hash.ToLowerInvariant() -ne '61bc1e030740cebc30cf1dad0c86cf65e39e12ff0500225821d684181e08d56b') { throw 'Unexpected baseline hash' }
if ((Get-FileHash -LiteralPath $DevelopmentFixture -Algorithm SHA256).Hash.ToLowerInvariant() -ne '2ecc5d19fe37f97e3f7f2467d652b299b5a47f010fa49fd803a49a4a6930a409') { throw 'Unexpected development-only fixture hash' }
if (-not $Zig) {
    $command = Get-Command zig -ErrorAction SilentlyContinue
    if ($command) { $Zig = $command.Source }
}
if (-not $Zig) { throw 'Provide -Zig <zig.exe> or STASH_ZIG (Zig 0.15.2)' }
if ((& $Zig version).Trim() -ne '0.15.2') { throw 'Requires Zig 0.15.2' }
New-Item -ItemType Directory -Force (Join-Path $root 'build') | Out-Null
Push-Location $root
try {
    & $Zig cc -target x86_64-windows-gnu -std=c11 -O2 -Wall -Wextra -Werror tools/address_probe.c src/address_resolver.c -Isrc -municode -lbcrypt -o build/address_probe.exe
    if ($LASTEXITCODE -ne 0) { throw 'Offline resolver probe compilation failed' }
    $baselineOutput = & ./build/address_probe.exe --image $Baseline
    if ($LASTEXITCODE -ne 0) { throw "Baseline/relocation/mutation tests failed: $baselineOutput" }
    $fixtureOutput = & ./build/address_probe.exe --inspect $DevelopmentFixture
    if ($LASTEXITCODE -ne 10) { throw "Development fixture must be rejected because its layout changed: $fixtureOutput" }
    @{
        baseline_relocation_mutation_tests = 'passed'
        development_fixture = @{ sha256 = '2ecc5d19fe37f97e3f7f2467d652b299b5a47f010fa49fd803a49a4a6930a409'; development_only = $true; executed = $false; result = 'rejected: changed layout/instructions' }
        baseline_output = $baselineOutput
        fixture_output = $fixtureOutput
        gameplay_test = 'not run; offline data-only validation'
    } | ConvertTo-Json -Depth 5 | Set-Content build/address-resolver-tests.json -Encoding utf8
    Write-Output 'Startup resolver tests passed: identity, relocation, mutation, ambiguity, call graph, manager agreement; development fixture correctly rejected. No game code executed.'
} finally { Pop-Location }
# The fixture's expected rejection uses probe exit 10; the test suite succeeded.
exit 0
