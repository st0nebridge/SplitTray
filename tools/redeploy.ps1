# =============================================================================
# Module:  tools/redeploy.ps1
# Purpose: One iteration of the live loop: build, install, restart Explorer, and
#          capture the mod's log. Each iteration costs a UAC prompt and takes the
#          taskbar down for a few seconds, so it does the whole cycle at once
#          rather than making the user approve several steps.
#
# Usage:   .\tools\redeploy.ps1 [-NoBuild] [-Seconds 45]
#                               [-Setting @{defaultTray='primary'}]
#
#          -Setting also takes 'name=value' strings, which is the only form that
#          survives `powershell -File redeploy.ps1 ...`.
#
#          The XAML tree dump is a switch in the source, g_dumpXamlTree, not a
#          setting: set it to true for a run, and back afterwards.
#
# Output:  the captured log is written to build\live-log.txt and the
#          split-tray lines are printed.
#
# Notes:   Only one debug-output listener can exist at a time - close DebugView
#          first. The elevated part runs in its own window via Start-Process
#          -Verb RunAs, so its output comes back through a transcript file.
# =============================================================================

[CmdletBinding()]
param(
    [switch]$NoBuild,
    [int]$Seconds = 45,
    # Forget every hand-moved icon, hidden icon and the icon order, so the
    # per-process rules decide again. The mod's own storage lives under HKLM,
    # so it takes the elevated run.
    [switch]$ClearPlacements,
    # Typed as [object] on purpose. Launched as `powershell -File redeploy.ps1`,
    # every argument arrives as a string, so a [hashtable] parameter fails the
    # transformation before the script ever runs - which is exactly what
    # `-Setting @{defaultTray='primary'}` did. Both spellings are accepted now.
    [object]$Setting
)

$ErrorActionPreference = 'Stop'

function ConvertTo-SettingTable($value) {
    if ($null -eq $value) { return @{} }
    if ($value -is [hashtable]) { return $value.Clone() }

    $table = @{}
    foreach ($item in @($value)) {
        # '@{a=1; b=2}' is what a hashtable literal collapses to under -File.
        $text = "$item".Trim() -replace '^@?\{', '' -replace '\}$', ''
        foreach ($pair in $text -split ';') {
            $pair = $pair.Trim()
            if (-not $pair) { continue }
            $split = $pair.IndexOf('=')
            if ($split -lt 1) {
                throw "Cannot read the setting '$pair'. Expected name=value."
            }
            $name = $pair.Substring(0, $split).Trim().Trim("'", '"')
            $raw = $pair.Substring($split + 1).Trim().Trim("'", '"')
            $number = 0
            $table[$name] = if ([int]::TryParse($raw, [ref]$number)) {
                $number
            } else {
                $raw
            }
        }
    }
    return $table
}

$repoRoot = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $repoRoot 'build'
$logPath = Join-Path $buildDir 'live-log.txt'
$settingsKey = 'HKLM:\SOFTWARE\Windhawk\Engine\Mods\local@split-tray\Settings'

function Write-Step($text) {
    Write-Host ''
    Write-Host "==> $text" -ForegroundColor Cyan
}

if (-not $NoBuild) {
    Write-Step 'Build'
    & (Join-Path $PSScriptRoot 'build.ps1') -SkipTests | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'Build failed' }
    Write-Host '    ok'
}

# Everything that needs elevation happens in one elevated run, so there is one
# UAC prompt per iteration.
Write-Step 'Install (elevated - approve the UAC prompt)'
$overrides = ConvertTo-SettingTable $Setting

# Settings cross the elevation boundary as JSON data, never as script text.
$transcript = Join-Path $buildDir 'redeploy-elevated.log'
$overridesPath = Join-Path $buildDir 'redeploy-settings.json'
New-Item -ItemType Directory -Force -Path $buildDir | Out-Null
$overrides | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $overridesPath -Encoding UTF8
$elevatedScript = Join-Path $buildDir 'redeploy-elevated.ps1'
@'
param([string]$RepoRoot, [string]$Transcript, [string]$OverridesPath,
      [int]$ClearPlacements = 0)
$ErrorActionPreference = 'Stop'
Start-Transcript -LiteralPath $Transcript -Force | Out-Null
try {
    Set-Location -LiteralPath $RepoRoot
    & (Join-Path $RepoRoot 'tools/install.ps1') -Install -Enable
    if ($ClearPlacements) {
        $source = Get-Content -LiteralPath (Join-Path $RepoRoot 'src/split-tray.wh.cpp') -Raw
        $names = [regex]::Matches($source, 'constexpr\s+PCWSTR\s+k\w+Value\s*=\s*L"(\w+)"') |
            ForEach-Object { $_.Groups[1].Value }
        if (-not $names) { throw 'Found no storage value names; refusing to guess.' }
        $storage = 'HKLM:\SOFTWARE\Windhawk\Engine\ModsWritable\local@split-tray\LocalStorage'
        if (Test-Path -LiteralPath $storage) {
            foreach ($name in $names) {
                Remove-ItemProperty -LiteralPath $storage -Name $name -ErrorAction SilentlyContinue
            }
        }
    }
    $settingsKey = 'HKLM:\SOFTWARE\Windhawk\Engine\Mods\local@split-tray\Settings'
    $settings = Get-Content -LiteralPath $OverridesPath -Raw | ConvertFrom-Json
    foreach ($entry in $settings.PSObject.Properties) {
        $type = if ($entry.Value -is [int]) { 'DWord' } else { 'String' }
        Set-ItemProperty -LiteralPath $settingsKey -Name $entry.Name -Value $entry.Value -Type $type
        Write-Host "setting       : $($entry.Name) = $($entry.Value)"
    }
} finally {
    Stop-Transcript | Out-Null
}
'@ | Set-Content -LiteralPath $elevatedScript -Encoding UTF8

# Validate the generated script before allowing any elevated execution.
$tokens = $null
$parseErrors = $null
[void][System.Management.Automation.Language.Parser]::ParseFile($elevatedScript, [ref]$tokens, [ref]$parseErrors)
if ($parseErrors.Count) { throw "Invalid installer script: $parseErrors" }
Remove-Item -LiteralPath $transcript -ErrorAction SilentlyContinue

# Run the install inline when this shell is already elevated, so there is no
# second UAC prompt and no extra window; otherwise ask for elevation.
$identity = [Security.Principal.WindowsIdentity]::GetCurrent()
$isElevated = (New-Object Security.Principal.WindowsPrincipal $identity).IsInRole(
    [Security.Principal.WindowsBuiltInRole]::Administrator)
$installArgs = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', $elevatedScript,
    '-RepoRoot', $repoRoot, '-Transcript', $transcript, '-OverridesPath', $overridesPath,
    '-ClearPlacements', [string][int]$ClearPlacements.IsPresent)
if ($isElevated) {
    & powershell.exe @installArgs
    if ($LASTEXITCODE -ne 0) { throw 'Elevated installation failed; Explorer was not restarted.' }
} else {
    # Windows paths cannot contain double quotes. Quote each argument for
    # Start-Process, which otherwise splits paths containing spaces.
    $quotedArgs = ($installArgs | ForEach-Object { '"' + $_ + '"' }) -join ' '
    $process = Start-Process -FilePath powershell.exe -Verb RunAs -Wait -PassThru -WindowStyle Hidden -ArgumentList $quotedArgs
    if ($process.ExitCode -ne 0) { throw 'Elevated installation failed; Explorer was not restarted.' }
}
if (Test-Path -LiteralPath $transcript) {
    Get-Content -LiteralPath $transcript |
        Select-String -Pattern 'installed DLL|mod is now|setting  |FAILED' |
        ForEach-Object { Write-Host "    $_" }
}

# The capture has to be listening before the new Explorer loads the mod.
Write-Step "Capture ($Seconds seconds) and restart Explorer"
$capture = Start-Process -FilePath (Join-Path $buildDir 'dbgcapture.exe') `
    -ArgumentList '--filter', 'split-tray', '--seconds', $Seconds `
    -RedirectStandardOutput $logPath -NoNewWindow -PassThru
Start-Sleep -Seconds 1

Stop-Process -Name explorer -Force -ErrorAction SilentlyContinue
$sw = [Diagnostics.Stopwatch]::StartNew()
while ($sw.Elapsed.TotalSeconds -lt 25 -and
       -not (Get-Process explorer -ErrorAction SilentlyContinue)) {
    Start-Sleep -Milliseconds 400
}
if (-not (Get-Process explorer -ErrorAction SilentlyContinue)) {
    Start-Process explorer.exe
}
Write-Host '    Explorer restarted; waiting for the capture to finish'
$capture.WaitForExit()

Write-Step 'Mod log'
if (Test-Path $logPath) {
    Get-Content $logPath | ForEach-Object { Write-Host $_ }
} else {
    Write-Host '    no log captured'
}
