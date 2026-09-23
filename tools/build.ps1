# =============================================================================
# Module:  tools/build.ps1
# Purpose: Everything that can be verified without touching the running shell:
#          the same compile the Windhawk editor performs, the regression suite,
#          and the mod DLL itself.
# Usage:   .\tools\build.ps1 [-SkipTests] [-SkipDll] [-Mutate]
# Deps:    Windhawk's bundled clang (C:\Program Files\Windhawk\Compiler),
#          LLVM's llvm-dlltool for the windhawk.dll import library.
# =============================================================================

[CmdletBinding()]
param(
    [switch]$SkipTests,
    [switch]$SkipDll,
    [switch]$Mutate
)

$ErrorActionPreference = 'Stop'
# PowerShell 7 turns a native command's stderr into a terminating error under
# ErrorActionPreference=Stop, so a single clang warning would fail the build.
# Exit codes are checked explicitly after every invocation instead.
$PSNativeCommandUseErrorActionPreference = $false

$repoRoot = Split-Path -Parent $PSScriptRoot
$modSource = Join-Path $repoRoot 'src\split-tray.wh.cpp'
$testSource = Join-Path $repoRoot 'tests\regression\split_tray_tests.cpp'
$harness = Join-Path $repoRoot 'tests\harness'
$buildDir = Join-Path $repoRoot 'build'

$windhawkRoot = 'C:\Program Files\Windhawk'
$clang = Join-Path $windhawkRoot 'Compiler\bin\clang++.exe'
$dllTool = 'C:\Program Files\LLVM\bin\llvm-dlltool.exe'
$internalApiHeader = Join-Path $windhawkRoot 'Compiler\include\windhawk_api_internal.h'

function Assert-Tool($path, $what) {
    if (-not (Test-Path $path)) {
        throw "$what not found at $path"
    }
}

function Write-Step($text) {
    Write-Host ''
    Write-Host "==> $text" -ForegroundColor Cyan
}

Assert-Tool $clang 'Windhawk bundled clang'
Assert-Tool $modSource 'mod source'
New-Item -ItemType Directory -Force -Path $buildDir | Out-Null

# The flags Windhawk's editor uses, read from its own compile_flags.txt so this
# cannot drift from what Windhawk will do when the mod is compiled for real.
$editorFlags = @(
    '-x', 'c++'
    '-std=c++23'
    '-target', 'x86_64-w64-mingw32'
    '-DUNICODE', '-D_UNICODE'
    '-DWINVER=0x0A00', '-D_WIN32_WINNT=0x0A00', '-D_WIN32_IE=0x0A00'
    '-DNTDDI_VERSION=0x0A000008'
    '-D__USE_MINGW_ANSI_STDIO=0'
    '-DWH_MOD', '-DWH_EDITING'
    '-include', 'windhawk_api.h'
    '-Wall', '-Wextra'
    '-Wno-unused-parameter', '-Wno-missing-field-initializers'
    '-Wno-cast-function-type-mismatch'
)

Write-Step 'Source hygiene'
# Stray control bytes from shell heredocs collapsing backslash escapes. A NUL
# inside a character literal even has the right value, so the code works and the
# file is quietly corrupt; cheap to check, so it is checked.
& python (Join-Path $PSScriptRoot 'check-sources.py')
if ($LASTEXITCODE -ne 0) { throw 'Stray control characters in tracked sources' }

Write-Step 'Settings block lint'
# A setting the block does not declare reads back as 0 with no error at all, so
# this mismatch has to be caught before the mod ships.
& python (Join-Path $PSScriptRoot 'check-settings.py')
if ($LASTEXITCODE -ne 0) { throw 'Settings block does not match the code' }

Write-Step 'Symbol check'
# The embedding hooks private symbols in Explorer's own DLLs. They resolve at
# runtime, so a Windows update that renames one leaves the mod a no-op with only
# a log line to say so. --offline keeps a network-less build working.
& python (Join-Path $PSScriptRoot 'check-symbols.py')
if ($LASTEXITCODE -ne 0) { throw 'A symbol the mod hooks is missing from the live binaries' }

Write-Step 'Compile check (identical to the Windhawk editor)'
& $clang @editorFlags '-fsyntax-only' $modSource
if ($LASTEXITCODE -ne 0) { throw "Compile check failed with exit code $LASTEXITCODE" }
Write-Host '    clean' -ForegroundColor Green

if (-not $SkipTests) {
    Write-Step 'Regression tests'
    $testExe = Join-Path $buildDir 'split_tray_tests.exe'
    $testFlags = @(
        '-x', 'c++', '-std=c++23', '-target', 'x86_64-w64-mingw32'
        '-DUNICODE', '-D_UNICODE'
        '-DWINVER=0x0A00', '-D_WIN32_WINNT=0x0A00', '-DNTDDI_VERSION=0x0A000008'
        '-D__USE_MINGW_ANSI_STDIO=0'
        '-O0', '-g'
        '-DSPLITTRAY_NO_XAML'
        '-I', $harness
        '-o', $testExe
        $testSource
        '-lcomctl32', '-lgdi32', '-luser32', '-lshell32'
        '-lole32', '-loleaut32', '-lruntimeobject', '-lshlwapi', '-static'
    )
    & $clang @testFlags
    if ($LASTEXITCODE -ne 0) { throw "Test build failed with exit code $LASTEXITCODE" }
    & $testExe
    if ($LASTEXITCODE -ne 0) { throw 'Regression tests FAILED' }

    Write-Step 'Integration test (private desktop, real shell32)'
    $integrationExe = Join-Path $buildDir 'mod_integration_test.exe'
    $integrationFlags = @(
        '-x', 'c++', '-std=c++23', '-target', 'x86_64-w64-mingw32'
        '-DUNICODE', '-D_UNICODE'
        '-DWINVER=0x0A00', '-D_WIN32_WINNT=0x0A00', '-DNTDDI_VERSION=0x0A000008'
        '-D__USE_MINGW_ANSI_STDIO=0'
        '-O0', '-g'
        '-DSPLITTRAY_NO_XAML'
        '-I', $harness
        '-o', $integrationExe
        (Join-Path $repoRoot 'tests\integration\mod_integration_test.cpp')
        '-lcomctl32', '-lgdi32', '-luser32', '-lshell32'
        '-lole32', '-loleaut32', '-lruntimeobject', '-lshlwapi', '-static'
        '-Wno-cast-function-type-mismatch'
    )
    & $clang @integrationFlags
    if ($LASTEXITCODE -ne 0) { throw "Integration build failed with exit code $LASTEXITCODE" }
    & $integrationExe
    if ($LASTEXITCODE -ne 0) { throw 'Integration test FAILED' }
}

if ($Mutate) {
    Write-Step 'Mutation check'
    & python (Join-Path $PSScriptRoot 'mutate.py')
    if ($LASTEXITCODE -ne 0) { throw 'Mutation check failed' }
}

Write-Step 'Diagnostic tools'
$nativeFlags = @(
    '-x', 'c++', '-std=c++23', '-target', 'x86_64-w64-mingw32'
    '-DUNICODE', '-D_UNICODE', '-DWINVER=0x0A00', '-D_WIN32_WINNT=0x0A00'
    '-O1', '-static'
)
# Captures the mod's Wh_Log output, which the engine emits with OutputDebugStringW.
& $clang @nativeFlags '-o' (Join-Path $buildDir 'dbgcapture.exe') `
    (Join-Path $repoRoot 'tools\dbgcapture.cpp') '-lkernel32'
if ($LASTEXITCODE -ne 0) { throw 'dbgcapture build failed' }
Write-Host '    dbgcapture.exe'

# Re-captures the Shell_TrayWnd wire format from the live shell32 on this build of
# Windows, on a private desktop so the real shell is never involved.
& $clang @nativeFlags '-o' (Join-Path $buildDir 'wire_probe.exe') `
    (Join-Path $repoRoot 'tests\probe\shell32_wire_probe.cpp') `
    '-luser32' '-lshell32' '-ladvapi32' '-lstdc++'
if ($LASTEXITCODE -ne 0) { throw 'wire probe build failed' }
Write-Host '    wire_probe.exe'

if (-not $SkipDll) {
    Write-Step 'Mod DLL'
    Assert-Tool $dllTool 'llvm-dlltool'
    Assert-Tool $internalApiHeader 'windhawk_api_internal.h'

    # Windhawk mods resolve the InternalWh_* entry points from windhawk.dll, which
    # the engine has already loaded into the host process. Build an import library
    # for it from the names its own header declares.
    $names = Select-String -Path $internalApiHeader -Pattern '\b(InternalWh_\w+)\s*\(' -AllMatches |
        ForEach-Object { $_.Matches } |
        ForEach-Object { $_.Groups[1].Value } |
        Sort-Object -Unique
    if ($names.Count -lt 10) { throw "Only found $($names.Count) InternalWh_* exports; expected ~23" }

    $defPath = Join-Path $buildDir 'windhawk.def'
    $defLines = @('LIBRARY windhawk.dll', 'EXPORTS') + $names
    Set-Content -Path $defPath -Value $defLines -Encoding ascii
    $importLib = Join-Path $buildDir 'libwindhawk.a'
    & $dllTool -m i386:x86-64 -d $defPath -l $importLib
    if ($LASTEXITCODE -ne 0) { throw 'llvm-dlltool failed' }
    Write-Host "    import library for windhawk.dll ($($names.Count) exports)"

    # Take the link libraries from the mod's own @compilerOptions line, which is
    # what Windhawk itself uses, so the two builds cannot drift apart.
    $modCompilerOptions = @()
    $optionsLine = Select-String -Path $modSource -Pattern '^// @compilerOptions\s+(.+)$'
    if ($optionsLine) {
        $modCompilerOptions = $optionsLine.Matches[0].Groups[1].Value.Trim() -split '\s+'
        Write-Host "    mod compiler options: $($modCompilerOptions -join ' ')"
    }

    $version = (Select-String -Path $modSource -Pattern '^// @version\s+(\S+)').Matches[0].Groups[1].Value
    $modId = (Select-String -Path $modSource -Pattern '^// @id\s+(\S+)').Matches[0].Groups[1].Value
    $dllPath = Join-Path $buildDir 'split-tray.dll'

    # WH_MOD_ID and WH_MOD_VERSION have to reach the compiler as wide string
    # literals, quotes and all. Passing -DWH_MOD_ID=L"..." on the command line
    # does not survive: Windows PowerShell 5.1 and PowerShell 7 hand quotes to
    # native commands differently, and under 5.1 the macro arrived as the bare
    # token Llocal@split-tray, which only fails later where the macro is
    # concatenated with another literal. A generated header has no shell in the
    # path at all.
    $definesHeader = Join-Path $buildDir 'mod_defines.h'
    @(
        '// GENERATED by tools/build.ps1 - do not edit.'
        '#pragma once'
        "#define WH_MOD_ID L`"local@$modId`""
        "#define WH_MOD_VERSION L`"$version`""
    ) | Set-Content -Path $definesHeader -Encoding ascii

    $dllFlags = @(
        '-x', 'c++', '-std=c++23', '-target', 'x86_64-w64-mingw32', '-shared'
        '-DUNICODE', '-D_UNICODE'
        '-DWINVER=0x0A00', '-D_WIN32_WINNT=0x0A00', '-D_WIN32_IE=0x0A00'
        '-DNTDDI_VERSION=0x0A000008'
        '-D__USE_MINGW_ANSI_STDIO=0'
        '-DWH_MOD'
        '-include', $definesHeader
        '-include', 'windhawk_api.h'
        '-O2'
        '-o', $dllPath
        $modSource
        '-Wl,--export-all-symbols'
        "-L$buildDir", '-lwindhawk'
        $modCompilerOptions
        '-static'
        '-Wno-cast-function-type-mismatch'
    )
    & $clang @dllFlags
    if ($LASTEXITCODE -ne 0) { throw "DLL link failed with exit code $LASTEXITCODE" }

    # The engine calls the mod through these five mangled exports; if any is
    # missing the mod silently does nothing, so check rather than assume.
    $required = @(
        '_Z10Wh_ModInitv', '_Z15Wh_ModAfterInitv', '_Z21Wh_ModSettingsChangedv',
        '_Z18Wh_ModBeforeUninitv', '_Z12Wh_ModUninitv'
    )
    $readobj = 'C:\Program Files\LLVM\bin\llvm-readobj.exe'
    if (Test-Path $readobj) {
        $exports = & $readobj --coff-exports $dllPath
        foreach ($symbol in $required) {
            if (-not ($exports | Select-String -SimpleMatch $symbol -Quiet)) {
                throw "Built DLL is missing the required export $symbol"
            }
        }
        Write-Host "    all $($required.Count) lifecycle exports present"
    }

    # The mod id reaches the binary as a UTF-16 literal via the registered
    # message names. Checking for it catches a macro that compiled but expanded
    # to the wrong thing, which a successful link would otherwise hide.
    $expected = "local@$modId"
    $bytes = [System.IO.File]::ReadAllBytes($dllPath)
    $utf16 = [System.Text.Encoding]::Unicode.GetString($bytes)
    if ($utf16.Contains($expected)) {
        Write-Host "    mod id '$expected' present in the binary"
    } else {
        throw "Built DLL does not contain the mod id '$expected'; WH_MOD_ID did not expand correctly"
    }

    $size = [math]::Round((Get-Item $dllPath).Length / 1KB)
    Write-Host "    $dllPath ($size KB), version $version" -ForegroundColor Green
}

Write-Host ''
Write-Host 'Build OK' -ForegroundColor Green
