# Module: SplitTray.Performance
# Purpose: Compile and repeat the isolated tray-update stress gate.
[CmdletBinding()]
param([ValidateRange(1,10)][int]$Runs=3,[switch]$ObserveOnly)
$ErrorActionPreference='Stop'
$repoRoot=Split-Path -Parent $PSScriptRoot
$compiler='C:\Program Files\Windhawk\Compiler\bin\clang++.exe'
$binary=Join-Path $repoRoot 'build\tray_stress.exe'
$flags=@('-std=c++23','-target','x86_64-w64-mingw32','-DUNICODE','-D_UNICODE',
 '-DWINVER=0x0A00','-D_WIN32_WINNT=0x0A00','-D_WIN32_IE=0x0A00','-DNTDDI_VERSION=0x0A000008',
 '-O1','-I',(Join-Path $repoRoot 'tests\harness'),(Join-Path $repoRoot 'tests\performance\tray_stress.cpp'),
 '-o',$binary,'-lcomctl32','-lgdi32','-luser32','-lshell32','-lole32','-loleaut32','-lruntimeobject','-lshcore','-luiautomationcore','-static')
& $compiler @flags
if ($LASTEXITCODE -ne 0) { throw 'Performance probe compile failed.' }
Copy-Item -LiteralPath (Join-Path $repoRoot 'tests\xaml\xaml_tests.exe.manifest') -Destination "$binary.manifest" -Force
for ($run=1;$run -le $Runs;$run++) {
 if ($ObserveOnly) { & $binary '--observe-only' } else { & $binary }
 if ($LASTEXITCODE -ne 0) { throw "Stress gate failed in run $run (exit $LASTEXITCODE)." }
}
