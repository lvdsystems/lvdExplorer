<#
.SYNOPSIS
    Builds lvdExplorer in Release (see stage-release.ps1) and compiles
    the Inno Setup installer (installer.iss) from the staged output.
    Requires Inno Setup's ISCC.exe compiler to be installed.
#>
param(
    [string]$QtBinDir = "C:\Qt\6.11.1\msvc2022_64\bin",
    [string]$IsccPath = "C:\Program Files\Inno Setup 7\ISCC.exe",
    [string]$OutputDir = "$PSScriptRoot\..\..\dist"
)

$release = & "$PSScriptRoot\stage-release.ps1" -QtBinDir $QtBinDir
$stagingDir = $release.StagingDir
$version = $release.Version

if (-not (Test-Path $IsccPath)) { throw "Inno Setup compiler not found at $IsccPath" }
if (-not (Test-Path $OutputDir)) { New-Item -ItemType Directory -Path $OutputDir | Out-Null }

Write-Host "==> Compiling installer (version $version)..."
& $IsccPath "/DStagingDir=$stagingDir" "/DOutputDir=$OutputDir" "/DMyAppVersion=$version" "$PSScriptRoot\installer.iss"
if ($LASTEXITCODE -ne 0) { throw "ISCC failed" }

Write-Host "==> Done."
