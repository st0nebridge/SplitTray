# Module: SplitTray.Configuration
# Purpose: Back up and restore Windhawk settings and remembered tray state.
# Dependencies: Windows registry, PowerShell 5.1; optional Windows Forms picker.
[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][ValidateSet('Export','Import')][string]$Action,
    [string]$Path,
    [ValidateSet('split-tray','local@split-tray')][string]$ModId = 'local@split-tray',
    [switch]$Interactive,
    [string]$RegistryBase = 'HKLM:\SOFTWARE\Windhawk\Engine'
)
$ErrorActionPreference = 'Stop'
if ($Interactive) {
    Add-Type -AssemblyName System.Windows.Forms
    if ($Action -eq 'Export') { $picker = New-Object System.Windows.Forms.SaveFileDialog }
    else { $picker = New-Object System.Windows.Forms.OpenFileDialog }
    try {
        $picker.Filter = 'Split Tray backup (*.json)|*.json'
        $picker.FileName = 'split-tray-backup.json'
        if ($picker.ShowDialog() -ne [System.Windows.Forms.DialogResult]::OK) { return }
        $Path = $picker.FileName
    } finally { $picker.Dispose() }
}
if (-not $Path) { throw 'Supply -Path or use -Interactive.' }
$settingsPath = Join-Path $RegistryBase "Mods\$ModId\Settings"
$statePath = Join-Path $RegistryBase "ModsWritable\$ModId\LocalStorage"

function Read-Values([string]$KeyPath) {
    $values = @()
    if (Test-Path -LiteralPath $KeyPath) {
        $key = Get-Item -LiteralPath $KeyPath
        try {
            foreach ($name in $key.GetValueNames()) {
                $kind = $key.GetValueKind($name).ToString()
                if ($kind -notin @('String','DWord')) { throw "Unsupported value type: $name ($kind)" }
                $values += [pscustomobject]@{ name=$name; kind=$kind; value=$key.GetValue($name) }
            }
        } finally { $key.Close() }
    }
    return $values
}

function Get-Backup {
    return [ordered]@{ format='SplitTray'; version=1; createdUtc=[DateTime]::UtcNow.ToString('o');
        settings=@(Read-Values $settingsPath); state=@(Read-Values $statePath) }
}

function Test-Backup($Backup) {
    if ($Backup.format -ne 'SplitTray' -or $Backup.version -ne 1) { throw 'Unrecognized Split Tray backup format.' }
    if ($null -eq $Backup.settings -or $null -eq $Backup.state) { throw 'Missing backup sections.' }
    $source = Get-Content -LiteralPath (Join-Path $PSScriptRoot '..\src\split-tray.wh.cpp') -Raw
    $settings = @([regex]::Matches($source, '(?m)^\s*- ([A-Za-z][A-Za-z0-9]*):') | ForEach-Object { $_.Groups[1].Value })
    # Older installations retain this inert diagnostic switch; preserve it in backups.
    $settings += 'dumpXamlTree'
    $stateNames = @([regex]::Matches($source, 'constexpr PCWSTR k\w+Value = L"([^"]+)";') | ForEach-Object { $_.Groups[1].Value })
    if (-not $settings.Count -or -not $stateNames.Count) { throw 'Could not derive allowed keys from the mod source.' }
    foreach ($section in @('settings','state')) {
        $seen = @{}
        foreach ($entry in @($Backup.$section)) {
            if (-not $entry -or $entry.name -isnot [string] -or $seen.ContainsKey($entry.name)) { throw 'Invalid or duplicate backup entry.' }
            $seen[$entry.name] = $true
            if ($section -eq 'state') {
                if ($entry.name -notin $stateNames -or $entry.kind -ne 'String') { throw "Unknown state entry: $($entry.name)" }
            } else {
                $baseName = ($entry.name -split '[.\[]')[0]
                if ($baseName -notin $settings -or $entry.name -notmatch '^[A-Za-z][A-Za-z0-9]*(\[\d{1,4}\])?(\.[A-Za-z][A-Za-z0-9]*)?$') { throw "Unknown setting: $($entry.name)" }
            }
            if ($entry.name -eq 'dumpXamlTree' -and ($entry.kind -ne 'DWord' -or $entry.value -notin @(0,1))) { throw 'Invalid retired diagnostic switch.' }
            if ($entry.kind -eq 'String') {
                if ($entry.value -isnot [string] -or $entry.value.Length -gt 4194304) { throw 'Invalid string value.' }
            } elseif ($entry.kind -eq 'DWord') {
                if ($entry.value -is [string] -or $entry.value -is [bool]) { throw 'Invalid DWORD value.' }
                $number = [long]$entry.value
                if ($number -lt [int]::MinValue -or $number -gt [uint32]::MaxValue -or [double]$entry.value -ne $number) { throw 'DWORD is out of range.' }
            } else { throw 'Unsupported registry value type.' }
        }
    }
}

function Write-Values([string]$KeyPath, $Values) {
    if (-not (Test-Path -LiteralPath $KeyPath)) { [void](New-Item -Path $KeyPath -Force) }
    $key = Get-Item -LiteralPath $KeyPath
    try {
        $names = @($key.GetValueNames())
    } finally { $key.Close() }
    foreach ($name in $names) { Remove-ItemProperty -LiteralPath $KeyPath -Name $name }
    foreach ($entry in @($Values)) {
        $value = $entry.value
        if ($entry.kind -eq 'DWord') { $value = [BitConverter]::ToInt32([BitConverter]::GetBytes([uint32]([long]$value -band 0xffffffffL)),0) }
        [void](New-ItemProperty -LiteralPath $KeyPath -Name $entry.name -Value $value -PropertyType $entry.kind -Force)
    }
}

if ($Action -eq 'Export') {
    $backup = Get-Backup
    Test-Backup $backup
    $backup | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $Path -Encoding UTF8
    Write-Output "Backup saved: $Path"
} else {
    $file = Get-Item -LiteralPath $Path
    if ($file.Length -gt 16777216) { throw 'Backup is larger than 16 MB.' }
    $incoming = Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json
    Test-Backup $incoming
    $modPath = Join-Path $RegistryBase "Mods\$ModId"
    if (Test-Path -LiteralPath $modPath) {
        $modKey = Get-Item -LiteralPath $modPath
        try { if ($modKey.GetValue('Disabled', 1) -eq 0) { throw 'Disable this mod in Windhawk before restoring its remembered state.' } }
        finally { $modKey.Close() }
    }
    # Validate everything before the first mutation; keep a complete recovery copy.
    $before = Get-Backup
    $recovery = "$Path.before-restore-$([DateTime]::UtcNow.ToString('yyyyMMddHHmmssfff')).json"
    $before | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $recovery -Encoding UTF8
    try {
        Write-Values $settingsPath $incoming.settings
        Write-Values $statePath $incoming.state
    } catch {
        Write-Values $settingsPath $before.settings
        Write-Values $statePath $before.state
        throw
    }
    Write-Output "Restored settings and state. Recovery backup: $recovery"
    Write-Output 'Disable and enable the development mod in Windhawk to load the restored state.'
}
