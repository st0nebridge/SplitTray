# =============================================================================
# Module:  tools/install.ps1
# Purpose: Install, enable, disable or remove the built mod in the local
#          Windhawk installation without going through the Windhawk UI.
#
#          A Windhawk mod is three things: the compiled DLL under
#          %ProgramData%\Windhawk\Engine\Mods\64, a registry key under
#          HKLM\SOFTWARE\Windhawk\Engine\Mods that tells the engine which
#          processes to inject it into, and a copy of the source under
#          %ProgramData%\Windhawk\ModsSource so the Windhawk UI can show and
#          rebuild it. This script maintains all three.
#
# Usage:   .\tools\install.ps1 -Install            # stage it, left disabled
#          .\tools\install.ps1 -Install -Enable    # stage and enable
#          .\tools\install.ps1 -Enable [-RestartExplorer]
#          .\tools\install.ps1 -Disable
#          .\tools\install.ps1 -Uninstall
#          .\tools\install.ps1 -Status
#          .\tools\install.ps1 -Logs [-Follow]
#
# Notes:   Requires elevation for everything except -Status and -Logs.
#          The engine injects mods when a process starts, so an already running
#          Explorer only picks the mod up after -RestartExplorer (or a reboot).
# =============================================================================

[CmdletBinding(DefaultParameterSetName = 'Status')]
param(
    # Mandatory in its own set, or `-Enable` alone matches both 'Install' and
    # 'Enable' and PowerShell cannot choose.
    [Parameter(ParameterSetName = 'Install', Mandatory)][switch]$Install,
    [Parameter(ParameterSetName = 'Install')]
    [Parameter(ParameterSetName = 'Enable')][switch]$Enable,
    [Parameter(ParameterSetName = 'Disable')][switch]$Disable,
    [Parameter(ParameterSetName = 'Uninstall')][switch]$Uninstall,
    [Parameter(ParameterSetName = 'Status')][switch]$Status,
    [Parameter(ParameterSetName = 'Logs')][switch]$Logs,
    [Parameter(ParameterSetName = 'Logs')][switch]$Follow,
    [Parameter(ParameterSetName = 'Install')]
    [Parameter(ParameterSetName = 'Enable')][switch]$RestartExplorer
)

$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$modSource = Join-Path $repoRoot 'src\split-tray.wh.cpp'
$builtDll = Join-Path $repoRoot 'build\split-tray.dll'

$modId = 'local@split-tray'
$windhawkData = 'C:\ProgramData\Windhawk'
$modsDir64 = Join-Path $windhawkData 'Engine\Mods\64'
$modsSourceDir = Join-Path $windhawkData 'ModsSource'
$regKey = "HKLM:\SOFTWARE\Windhawk\Engine\Mods\$modId"

function Test-Elevated {
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    (New-Object Security.Principal.WindowsPrincipal $identity).IsInRole(
        [Security.Principal.WindowsBuiltInRole]::Administrator)
}

function Assert-Elevated {
    if (-not (Test-Elevated)) {
        throw 'This action writes to HKLM and %ProgramData%; re-run in an elevated shell.'
    }
}

function Get-ModVersion {
    (Select-String -Path $modSource -Pattern '^// @version\s+(\S+)').Matches[0].Groups[1].Value
}

# The mod's default settings, parsed out of its own ==WindhawkModSettings==
# block.
#
# The engine reads settings from the registry, and a value that is not there
# reads back as 0 or an empty string with no error at all - so a hand install has
# to seed them, the way Windhawk's UI does. This used to be a hardcoded table
# here, which silently went stale the moment a setting was added to the mod:
# embedInTaskbar defaulted to on in the block, was never written to the registry,
# and so read back as off, which switched the whole XAML attachment off without a
# single line in the log. Parsing the block means the two cannot drift.
#
# Only top-level scalars are seeded. A list setting (perProcessRouting) is
# legitimately absent until the user adds a rule.
function Get-DefaultSettings {
    $lines = Get-Content $modSource
    $start = ($lines | Select-String -Pattern '^// ==WindhawkModSettings==$').LineNumber
    $end = ($lines | Select-String -Pattern '^// ==/WindhawkModSettings==$').LineNumber
    if (-not $start -or -not $end) {
        throw "Could not find the settings block in $modSource"
    }
    $block = $lines[$start..($end - 2)]

    $defaults = @{}
    foreach ($line in $block) {
        # Top level only: nested entries are indented, so anchor on column 0.
        if ($line -notmatch '^- (\w+):\s*(.*)$') { continue }
        $name = $Matches[1]
        $value = $Matches[2].Trim()

        if ($value -eq '') { continue }                 # a list, not a scalar
        if ($value -match '^[>|]') { continue }         # folded block scalar

        if ($value -eq 'true') { $defaults[$name] = 1 }
        elseif ($value -eq 'false') { $defaults[$name] = 0 }
        elseif ($value -match '^-?\d+$') { $defaults[$name] = [int]$value }
        else { $defaults[$name] = $value.Trim('"', "'") }
    }
    if ($defaults.Count -lt 5) {
        throw "Only parsed $($defaults.Count) settings from the block; that is not right"
    }
    $defaults
}

function Show-Status {
    Write-Host "mod id        : $modId"
    if (Test-Path $regKey) {
        $props = Get-ItemProperty $regKey
        $state = if ($props.Disabled -eq 0) { 'ENABLED' } else { 'disabled' }
        Write-Host "registry      : present ($state)"
        Write-Host "version       : $($props.Version)"
        Write-Host "library       : $($props.LibraryFileName)"
        Write-Host "injected into : $($props.Include)"
        $dll = Join-Path $modsDir64 $props.LibraryFileName
        Write-Host "dll on disk   : $(if (Test-Path $dll) { 'yes' } else { 'MISSING' })"
        if (Test-Path "$regKey\Settings") {
            Write-Host 'settings      :'
            Get-ItemProperty "$regKey\Settings" |
                Select-Object -Property * -ExcludeProperty PS* |
                Format-List | Out-String | Write-Host
        }
    } else {
        Write-Host 'registry      : not installed'
    }
    $built = if (Test-Path $builtDll) {
        "yes ($(Get-Item $builtDll | ForEach-Object LastWriteTime))"
    } else { 'no - run tools\build.ps1' }
    Write-Host "local build   : $built"
    $explorer = Get-Process explorer -ErrorAction SilentlyContinue
    if ($explorer) {
        $loaded = $explorer.Modules | Where-Object { $_.ModuleName -like 'local@split-tray*' }
        Write-Host "loaded in explorer: $(if ($loaded) { $loaded.ModuleName -join ', ' } else { 'no' })"
    }
}

function Invoke-Install {
    Assert-Elevated
    if (-not (Test-Path $builtDll)) {
        throw "No built DLL at $builtDll - run tools\build.ps1 first."
    }
    if (-not (Test-Path $windhawkData)) {
        throw "Windhawk data directory not found at $windhawkData - is Windhawk installed?"
    }
    New-Item -ItemType Directory -Force -Path $modsDir64 | Out-Null
    New-Item -ItemType Directory -Force -Path $modsSourceDir | Out-Null

    $version = Get-ModVersion
    # Windhawk gives each build a unique file name so a new DLL can be dropped in
    # while the previous one is still mapped into a running process.
    $stamp = Get-Random -Minimum 100000 -Maximum 999999
    $libraryFileName = "${modId}_${version}_${stamp}.dll"
    Copy-Item $builtDll (Join-Path $modsDir64 $libraryFileName) -Force
    Write-Host "installed DLL : $libraryFileName"

    # Keep the source where the Windhawk UI can find it, so the mod can be edited
    # and recompiled from the UI afterwards.
    Copy-Item $modSource (Join-Path $modsSourceDir "$modId.wh.cpp") -Force
    Write-Host "installed src : $modsSourceDir\$modId.wh.cpp"

    $existing = if (Test-Path $regKey) { Get-ItemProperty $regKey } else { $null }
    $previousLibrary = $existing.LibraryFileName
    # Preserve whether the mod was enabled across a reinstall; a fresh install is
    # staged disabled so nothing is injected until it is asked for.
    $previousDisabled = if ($null -ne $existing -and $null -ne $existing.Disabled) {
        [int]$existing.Disabled
    } else { 1 }

    # NOT New-Item -Force: on an existing key that deletes every value and the
    # Settings subkey with it, which silently resets the user's configuration and
    # leaves Disabled unset.
    if (-not (Test-Path $regKey)) {
        New-Item -Path $regKey -Force | Out-Null
    }
    Set-ItemProperty $regKey -Name 'LibraryFileName' -Value $libraryFileName -Type String
    Set-ItemProperty $regKey -Name 'Include' -Value 'explorer.exe' -Type String
    Set-ItemProperty $regKey -Name 'Exclude' -Value '' -Type String
    Set-ItemProperty $regKey -Name 'Architecture' -Value 'x86-64' -Type String
    Set-ItemProperty $regKey -Name 'Version' -Value $version -Type String
    Set-ItemProperty $regKey -Name 'LoggingEnabled' -Value 1 -Type DWord
    Set-ItemProperty $regKey -Name 'SettingsChangeTime' `
        -Value ([int][double]::Parse((Get-Date -UFormat %s))) -Type DWord
    Set-ItemProperty $regKey -Name 'Disabled' -Value $previousDisabled -Type DWord

    # Only seed settings that are not already there, so an upgrade keeps the
    # user's choices. Again not -Force: that would wipe them.
    if (-not (Test-Path "$regKey\Settings")) {
        New-Item -Path "$regKey\Settings" -Force | Out-Null
    }
    $current = Get-ItemProperty "$regKey\Settings"
    $defaults = Get-DefaultSettings
    $seeded = @()
    foreach ($entry in $defaults.GetEnumerator()) {
        if ($null -ne $current.($entry.Key)) { continue }
        $type = if ($entry.Value -is [int]) { 'DWord' } else { 'String' }
        Set-ItemProperty "$regKey\Settings" -Name $entry.Key -Value $entry.Value -Type $type
        $seeded += $entry.Key
    }
    if ($seeded.Count) {
        Write-Host "seeded        : $($seeded -join ', ')"
    }

    # A starting position for a fresh install: SystemInformer publishes four
    # icons (CPU, I/O, memory, network) and they are the obvious candidates for
    # a second tray. Only written when no rule exists at all, so it never
    # overwrites rules the user has set.
    if ($null -eq $current.'perProcessRouting[0].exe') {
        Set-ItemProperty "$regKey\Settings" -Name 'perProcessRouting[0].exe' `
            -Value 'SystemInformer.exe' -Type String
        Set-ItemProperty "$regKey\Settings" -Name 'perProcessRouting[0].destination' `
            -Value 'secondary' -Type String
        Write-Host "seeded rule   : SystemInformer.exe -> secondary"
    }

    # Check the result rather than the intent. A setting missing from the
    # registry does not fail anything - it reads back as 0 and quietly turns a
    # feature off - so the only way to know the seeding worked is to read it back.
    $after = Get-ItemProperty "$regKey\Settings"
    $absent = $defaults.Keys | Where-Object { $null -eq $after.$_ }
    if ($absent) {
        throw "These settings are still missing from the registry after seeding: $($absent -join ', ')"
    }
    Write-Host "settings      : all $($defaults.Count) present"

    if ($previousLibrary -and $previousLibrary -ne $libraryFileName) {
        $stale = Join-Path $modsDir64 $previousLibrary
        if (Test-Path $stale) {
            # May still be mapped into a live Explorer; failing to delete is fine.
            Remove-Item $stale -Force -ErrorAction SilentlyContinue
        }
    }

    Write-Host 'registry      : written'
}

function Set-Enabled([bool]$enabled) {
    Assert-Elevated
    if (-not (Test-Path $regKey)) { throw "Not installed; run -Install first." }
    Set-ItemProperty $regKey -Name 'Disabled' -Value ([int](-not $enabled)) -Type DWord
    Set-ItemProperty $regKey -Name 'SettingsChangeTime' `
        -Value ([int][double]::Parse((Get-Date -UFormat %s))) -Type DWord
    Write-Host "mod is now    : $(if ($enabled) { 'ENABLED' } else { 'disabled' })"
}

function Invoke-Uninstall {
    Assert-Elevated
    if (Test-Path $regKey) {
        $library = (Get-ItemProperty $regKey).LibraryFileName
        Remove-Item $regKey -Recurse -Force
        Write-Host 'registry      : removed'
        if ($library) {
            $dll = Join-Path $modsDir64 $library
            if (Test-Path $dll) {
                Remove-Item $dll -Force -ErrorAction SilentlyContinue
            }
        }
    }
    $src = Join-Path $modsSourceDir "$modId.wh.cpp"
    if (Test-Path $src) {
        Remove-Item $src -Force
        Write-Host 'source        : removed'
    }
    Write-Host 'Restart Explorer to unload it from the running shell.'
}

function Restart-Explorer {
    Write-Host 'restarting Explorer...'
    Stop-Process -Name explorer -Force -ErrorAction SilentlyContinue
    # Windows restarts the shell itself; only start it if it stays down.
    for ($i = 0; $i -lt 20; $i++) {
        Start-Sleep -Milliseconds 500
        if (Get-Process explorer -ErrorAction SilentlyContinue) { break }
    }
    if (-not (Get-Process explorer -ErrorAction SilentlyContinue)) {
        Start-Process explorer.exe
    }
    Start-Sleep -Seconds 3
    Write-Host 'Explorer is back up.'
}

function Show-Logs {
    # Windhawk writes mod logs to the debug output; DebugView or the Windhawk UI
    # log window are the usual readers. The UI keeps a copy here when its log
    # window has been open.
    $candidates = @(
        (Join-Path $windhawkData 'Engine\Logs'),
        (Join-Path $windhawkData 'UIData\user-data\logs')
    )
    $found = $false
    foreach ($dir in $candidates) {
        if (Test-Path $dir) {
            Get-ChildItem $dir -Recurse -Filter *.log -ErrorAction SilentlyContinue |
                ForEach-Object { Write-Host $_.FullName; $found = $true }
        }
    }
    if (-not $found) {
        Write-Host 'Windhawk does not keep mod logs on disk by default.'
        Write-Host 'Read them live with one of:'
        Write-Host '  * The Windhawk UI: open the mod, then the log window'
        Write-Host '  * Sysinternals DebugView (run elevated, enable Capture Global Win32)'
        Write-Host '  * tools\watch-log.ps1 in this repo'
    }
}

switch ($PSCmdlet.ParameterSetName) {
    'Install' {
        Invoke-Install
        if ($Enable) { Set-Enabled $true }
        if ($RestartExplorer) { Restart-Explorer }
        Show-Status
    }
    'Enable' {
        Set-Enabled $true
        if ($RestartExplorer) { Restart-Explorer }
        Show-Status
    }
    'Disable' { Set-Enabled $false; Show-Status }
    'Uninstall' { Invoke-Uninstall }
    'Logs' { Show-Logs }
    default { Show-Status }
}
