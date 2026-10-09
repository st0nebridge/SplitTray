# =============================================================================
# Module:  tools/coverage.ps1
# Purpose: Measures how much of the mod's source the three test suites run:
#          regions, branches, functions and lines, from clang's source-based
#          coverage over the regression suite, the integration test and the
#          XAML suite together. The first two compile the XAML section out
#          (SPLITTRAY_NO_XAML); the XAML suite compiles it in and runs it in a
#          hosted island, so the whole of the mod's source is counted.
# Usage:   .\tools\coverage.ps1 [-Uncovered] [-Html]
#            -Uncovered  also lists the mod's functions that no test runs
#            -Html       also writes an annotated copy of the source to
#                        build\coverage\html\index.html
# Deps:    Windhawk's bundled clang, which carries the profile runtime;
#          llvm-profdata and llvm-cov of the same major version, from
#          C:\Program Files\LLVM.
# =============================================================================

[CmdletBinding()]
param([switch]$Uncovered, [switch]$Html)

$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $false

$repoRoot = Split-Path -Parent $PSScriptRoot
$clang = 'C:\Program Files\Windhawk\Compiler\bin\clang++.exe'
$profdata = 'C:\Program Files\LLVM\bin\llvm-profdata.exe'
$llvmCov = 'C:\Program Files\LLVM\bin\llvm-cov.exe'
foreach ($tool in @($clang, $profdata, $llvmCov)) {
    if (-not (Test-Path $tool)) { throw "Not found: $tool" }
}

$outDir = Join-Path $repoRoot 'build\coverage'
if (Test-Path $outDir) { Remove-Item -Recurse -Force $outDir }
New-Item -ItemType Directory -Force -Path $outDir | Out-Null

# The same flags as tools/build.ps1's test builds, plus coverage.
$common = @(
    '-x', 'c++', '-std=c++23', '-target', 'x86_64-w64-mingw32'
    '-DUNICODE', '-D_UNICODE'
    '-DWINVER=0x0A00', '-D_WIN32_WINNT=0x0A00', '-DNTDDI_VERSION=0x0A000008'
    '-D__USE_MINGW_ANSI_STDIO=0'
    '-O0', '-g'
    '-fprofile-instr-generate', '-fcoverage-mapping'
    '-I', (Join-Path $repoRoot 'tests\harness')
)
$libs = @(
    '-lcomctl32', '-lgdi32', '-luser32', '-lshell32'
    '-lole32', '-loleaut32', '-lruntimeobject', '-lshcore', '-luiautomationcore', '-static'
    '-Wno-cast-function-type-mismatch'
)
$suites = [ordered]@{
    unit        = 'tests\regression\split_tray_tests.cpp'
    integration = 'tests\integration\mod_integration_test.cpp'
    xaml        = 'tests\xaml\xaml_tests.cpp'
}
# Each suite's own flags: the first two leave the XAML section out.
$suiteFlags = @{
    unit        = @('-DSPLITTRAY_NO_XAML')
    integration = @('-DSPLITTRAY_NO_XAML')
    xaml        = @('-D_WIN32_IE=0x0A00')
}

$binaries = @()
foreach ($name in $suites.Keys) {
    $exe = Join-Path $outDir "$name.exe"
    Write-Host "==> building $name with coverage"
    if ($name -eq 'xaml') {
        # Windows hosts XAML in a Win32 window only for a process whose
        # manifest names Windows 10.
        Copy-Item (Join-Path $repoRoot 'tests\xaml\xaml_tests.exe.manifest') "$exe.manifest" -Force
    }
    $flags = $suiteFlags[$name]
    & $clang @common @flags '-o' $exe (Join-Path $repoRoot $suites[$name]) @libs
    if ($LASTEXITCODE -ne 0) { throw "$name build failed ($LASTEXITCODE)" }

    # %p: the integration test runs a copy of itself on a private desktop, and
    # each process writes its own profile.
    $env:LLVM_PROFILE_FILE = Join-Path $outDir "$name-%p.profraw"
    Write-Host "==> running $name"
    $summary = & $exe 2>&1 | Select-String -Pattern '\d+ checks, \d+ failures?'
    $code = $LASTEXITCODE
    Write-Host "    $summary"
    if ($code -ne 0) { throw "$name suite failed ($code): coverage of a failing run means nothing" }
    $binaries += $exe
}
Remove-Item Env:LLVM_PROFILE_FILE

$raw = @(Get-ChildItem $outDir -Filter *.profraw | ForEach-Object FullName)
if ($raw.Count -lt 3) { throw "Expected a profile from each suite, found $($raw.Count)" }
$merged = Join-Path $outDir 'merged.profdata'
& $profdata merge -sparse -o $merged @raw
if ($LASTEXITCODE -ne 0) { throw 'llvm-profdata merge failed' }

# Only the mod's own source: not the tests, the golden payloads, the harness or
# the compiler's headers. (LLVM's regular expressions have no lookahead, so the
# rest is named rather than the mod kept.)
$objects = @($binaries[0]) + @($binaries[1..($binaries.Count - 1)] | ForEach-Object { '-object', $_ })
$sourceFilter = @('-ignore-filename-regex', '_tests?\.cpp$|golden_payloads\.h$|[\\/]harness[\\/]|[\\/]Compiler[\\/]')

Write-Host ''
Write-Host "==> coverage of src\split-tray.wh.cpp ($($raw.Count) profiles)"
# llvm-cov warns on stderr about functions compiled differently in the suites
# (with and without the XAML section). Windows PowerShell 5.1 turns a native
# command's stderr into a terminating error under 'Stop', which the setting
# above only prevents in PowerShell 7, so the exit codes decide here.
$ErrorActionPreference = 'Continue'
$report = & $llvmCov report @objects "-instr-profile=$merged" '-show-branch-summary' @sourceFilter 2>&1 |
    ForEach-Object { "$_" }
if ($LASTEXITCODE -ne 0) { throw 'llvm-cov report failed' }
$report | Write-Host

# The four measures from the TOTAL row: regions stand in for statements.
$export = & $llvmCov export @objects "-instr-profile=$merged" '-summary-only' @sourceFilter 2>$null |
    ConvertFrom-Json
$totals = $export.data[0].totals
$dims = [ordered]@{
    'S (regions)'   = $totals.regions.percent
    'B (branches)'  = $totals.branches.percent
    'F (functions)' = $totals.functions.percent
    'L (lines)'     = $totals.lines.percent
}
$targets = @{ 'S (regions)' = 85; 'B (branches)' = 70; 'F (functions)' = 85; 'L (lines)' = 85 }
Write-Host ''
foreach ($key in $dims.Keys) {
    $mark = if ($dims[$key] -ge $targets[$key]) { 'ok' } else { 'BELOW' }
    Write-Host ('    {0,-14} {1,6:N1}%   target {2}%   {3}' -f $key, $dims[$key], $targets[$key], $mark)
}

if ($Uncovered) {
    Write-Host ''
    Write-Host '==> functions no test runs'
    $functions = & $llvmCov export @objects "-instr-profile=$merged" @sourceFilter 2>$null |
        ConvertFrom-Json
    $functions.data[0].functions |
        Where-Object { $_.count -eq 0 } |
        ForEach-Object { '{0,6}  {1}' -f $_.regions[0][0], $_.name } |
        Sort-Object { [int]($_.Trim() -split '\s+')[0] } |
        Write-Host
}

if ($Html) {
    & $llvmCov show @objects "-instr-profile=$merged" '-format=html' '-show-branches=count' `
        "-output-dir=$(Join-Path $outDir 'html')" @sourceFilter 2>$null
    if ($LASTEXITCODE -ne 0) { throw 'llvm-cov show failed' }
    Write-Host "    annotated source: $(Join-Path $outDir 'html\index.html')"
}
