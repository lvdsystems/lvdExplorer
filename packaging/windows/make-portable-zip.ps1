<#
.SYNOPSIS
    Packages lvdExplorer as a self-contained portable .zip: the staged
    release build (see stage-release.ps1) plus the lvdExplorer.ini marker
    file that switches the app into portable storage mode (see
    SessionManager::isPortableMode() in core/session/sessionmanager.cpp)
    so sessions/settings live next to the exe instead of the per-user
    AppData location.

.PARAMETER OutputDir
    Where to place the final zip. Defaults to <repo>/dist.
#>
param(
    [string]$QtBinDir = "C:\Qt\6.11.1\msvc2022_64\bin",
    [string]$OutputDir = "$PSScriptRoot\..\..\dist"
)

$repoRoot = Resolve-Path "$PSScriptRoot\..\.."
$release = & "$PSScriptRoot\stage-release.ps1" -QtBinDir $QtBinDir
$stagingDir = $release.StagingDir
$version = $release.Version

# Portable-mode marker -- see the doc comment above.
New-Item -ItemType File -Path (Join-Path $stagingDir "lvdExplorer.ini") -Force | Out-Null

Copy-Item (Join-Path $repoRoot "README.md") -Destination $stagingDir
Copy-Item (Join-Path $repoRoot "LICENSE") -Destination $stagingDir

if (-not (Test-Path $OutputDir)) { New-Item -ItemType Directory -Path $OutputDir | Out-Null }
$zipPath = Join-Path $OutputDir "lvdExplorer-$version-portable-win64.zip"
if (Test-Path $zipPath) { Remove-Item $zipPath }

Write-Host "==> Compressing to $zipPath..."
Compress-Archive -Path (Join-Path $stagingDir "*") -DestinationPath $zipPath

Write-Host "==> Done: $zipPath"
