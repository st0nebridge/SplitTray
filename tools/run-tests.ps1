# =============================================================================
# Module:  tools/run-tests.ps1
# Purpose: Build and run the regression suite only. Shorthand for
#          tools\build.ps1 -SkipDll.
# Usage:   .\tools\run-tests.ps1 [-Mutate]
# =============================================================================

[CmdletBinding()]
param([switch]$Mutate)

$ErrorActionPreference = 'Stop'
& (Join-Path $PSScriptRoot 'build.ps1') -SkipDll -Mutate:$Mutate
