<#
.SYNOPSIS
    Builds lvdExplorer in Release and stages the exe plus its Qt
    dependencies (via windeployqt) into a clean directory. Shared by
    make-portable-zip.ps1 and build-installer.ps1 so the build+deploy
    steps exist in exactly one place.

.OUTPUTS
    A [PSCustomObject] with StagingDir and Version properties (Version is
    parsed straight out of the just-built app/generated/Version.h, so it's
    always exactly what this specific build actually compiled in -- see
    app/Version.h.in / project(lvdExplorer VERSION ...) in the root
    CMakeLists.txt for where that number ultimately comes from).
#>
param(
    [string]$QtBinDir = "C:\Qt\6.11.1\msvc2022_64\bin",
    # vcpkg install prefix providing libarchive (`vcpkg install libarchive`);
    # its runtime DLLs are copied next to the exe below.
    [string]$VcpkgInstalledDir = "D:\tools\vcpkg\installed\x64-windows",
    [string]$BuildDir = "$PSScriptRoot\..\..\build\release-package",
    [string]$StagingDir = "$BuildDir\staging"
)

$repoRoot = Resolve-Path "$PSScriptRoot\..\.."

Write-Host "==> Configuring (Release)..."
# Piped to Out-Host (not just left to flow through): stdout from a native
# exe joins the *success* output stream, and this script's return value
# is exactly that stream's leftover content -- letting cmake/windeployqt
# output flow through un-caught corrupts the caller's `$stagingDir = &
# stage-release.ps1 ...` capture into a multi-line blob of build log text.
& cmake -S $repoRoot -B $BuildDir -G "Visual Studio 17 2022" -A x64 `
    -DCMAKE_PREFIX_PATH="$(Split-Path $QtBinDir -Parent);$VcpkgInstalledDir" | Out-Host
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed" }

Write-Host "==> Building (Release)..."
& cmake --build $BuildDir --config Release --target lvdExplorer | Out-Host
if ($LASTEXITCODE -ne 0) { throw "Build failed" }

$exePath = Join-Path $BuildDir "Release\lvdExplorer.exe"
if (-not (Test-Path $exePath)) { throw "Built executable not found at $exePath" }

$versionHeaderPath = Join-Path $BuildDir "app\generated\Version.h"
if (-not (Test-Path $versionHeaderPath)) { throw "Generated Version.h not found at $versionHeaderPath" }
$versionMatch = Select-String -Path $versionHeaderPath -Pattern 'LVDEXPLORER_VERSION_STRING\s+"([^"]+)"'
if (-not $versionMatch) { throw "Could not parse LVDEXPLORER_VERSION_STRING out of $versionHeaderPath" }
$version = $versionMatch.Matches[0].Groups[1].Value
Write-Host "==> Version: $version"

if (Test-Path $StagingDir) { Remove-Item -Recurse -Force $StagingDir }
New-Item -ItemType Directory -Path $StagingDir | Out-Null
Copy-Item $exePath -Destination $StagingDir

Write-Host "==> Running windeployqt..."
$windeployqt = Join-Path $QtBinDir "windeployqt.exe"
& $windeployqt --release (Join-Path $StagingDir "lvdExplorer.exe") | Out-Host
if ($LASTEXITCODE -ne 0) { throw "windeployqt failed" }

# libarchive and the compression libraries it links against (archive.dll
# needs no OpenSSL with the feature set libarchive is installed with here).
foreach ($dll in @("archive.dll", "z.dll", "bz2.dll", "liblzma.dll", "lz4.dll", "zstd.dll")) {
    $source = Join-Path $VcpkgInstalledDir "bin\$dll"
    if (-not (Test-Path $source)) { throw "Missing libarchive runtime DLL: $source" }
    Copy-Item $source -Destination $StagingDir
}

Write-Host "==> Staged at $StagingDir"
return [PSCustomObject]@{ StagingDir = $StagingDir; Version = $version }
