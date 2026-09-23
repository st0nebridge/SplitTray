# =============================================================================
# Module:  tools/redeploy.ps1
# Purpose: One iteration of the live loop: build, install, restart Explorer, and
#          capture the mod's log. Each iteration costs a UAC prompt and takes the
#          taskbar down for a few seconds, so it does the whole cycle at once
#          rather than making the user approve several steps.
#
# Usage:   .\tools\redeploy.ps1 [-NoBuild] [-Seconds 45] [-DumpXamlTree]
#                               [-Setting @{defaultTray='primary'}]
#
#          -Setting also takes 'name=value' strings, which is the only form that
#          survives `powershell -File redeploy.ps1 ...`.
#
#          dumpXamlTree is switched off on every run without -DumpXamlTree.
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
    [switch]$DumpXamlTree,
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
# Set on every run, not only when asked for. The setting persists, and a dump
# left on from one diagnostic run held the taskbar's thread for seconds on every
# Explorer start afterwards, long enough for Desk Tray to lose two icons
# (DECISIONS 51).
if (-not $overrides.ContainsKey('dumpXamlTree')) {
    $overrides['dumpXamlTree'] = [int]$DumpXamlTree.IsPresent
}

# The storage value names, read from the mod rather than listed here: a copy in
# this script drifted from the source once already ('iconOrder' for what the mod
# calls 'embeddedIconOrder'), and a clear that misses a name fails silently.
$modSource = Get-Content (Join-Path $repoRoot 'src\split-tray.wh.cpp') -Raw
$storageNames = [regex]::Matches($modSource,
    'constexpr\s+PCWSTR\s+k\w+Value\s*=\s*L"(\w+)"') |
    ForEach-Object { $_.Groups[1].Value }
if ($ClearPlacements -and -not $storageNames) {
    throw 'Found no storage value names in the mod source; refusing to guess.'
}
$storageLiteral = ($storageNames | ForEach-Object { "'$_'" }) -join ', '

$transcript = Join-Path $buildDir 'redeploy-elevated.log'
$overrideLiteral = ($overrides.GetEnumerator() | ForEach-Object {
    $v = if ($_.Value -is [int]) { "$($_.Value)" } else { "'$($_.Value)'" }
    "'$($_.Key)'=$v"
}) -join '; '

$elevatedScript = Join-Path $buildDir 'redeploy-elevated.ps1'
@"
Start-Transcript -Path '$transcript' -Force | Out-Null
try {
    Set-Location '$repoRoot'
    .\tools\install.ps1 -Install -Enable
    if (`$$($ClearPlacements.IsPresent)) {
        `$storage = 'HKLM:\SOFTWARE\Windhawk\Engine\ModsWritable\local@split-tray\LocalStorage'
        if (Test-Path `$storage) {
            foreach (`$name in @($storageLiteral)) {
                Remove-ItemProperty `$storage -Name `$name -ErrorAction SilentlyContinue
            }
            Write-Host 'setting       : cleared $($storageNames -join ', ')'
        }
    }
    `$overrides = @{ $overrideLiteral }
    foreach (`$e in `$overrides.GetEnumerator()) {
        `$type = if (`$e.Value -is [int]) { 'DWord' } else { 'String' }
        Set-ItemProperty '$settingsKey' -Name `$e.Key -Value `$e.Value -Type `$type
        Write-Host "setting       : `$(`$e.Key) = `$(`$e.Value)"
    }
} catch {
    Write-Host "FAILED: `$(`$_.Exception.Message)"
}
Stop-Transcript | Out-Null
"@ | Set-Content -Path $elevatedScript -Encoding utf8

Remove-Item $transcript -ErrorAction SilentlyContinue

# Run the install inline when this shell is already elevated, so there is no
# second UAC prompt and no extra window; otherwise ask for elevation.
$identity = [Security.Principal.WindowsIdentity]::GetCurrent()
$isElevated = (New-Object Security.Principal.WindowsPrincipal $identity).IsInRole(
    [Security.Principal.WindowsBuiltInRole]::Administrator)

if ($isElevated) {
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $elevatedScript
} else {
    try {
        Start-Process -FilePath powershell.exe -Verb RunAs -Wait -WindowStyle Hidden `
            -ArgumentList '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', $elevatedScript
    } catch {
        Write-Host ''
        Write-Host 'The elevation prompt was dismissed, so nothing was installed.' -ForegroundColor Yellow
        Write-Host 'Re-run this script, or run it from an elevated shell to avoid the prompt:' -ForegroundColor Yellow
        Write-Host "    $PSCommandPath $($MyInvocation.Line -replace '^.*redeploy\.ps1', '')" -ForegroundColor Yellow
        exit 1
    }
}

if (Test-Path $transcript) {
    Get-Content $transcript |
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
