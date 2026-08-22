# Windows build driver for lhatove.
#
# LOVE's MSVC build requires the megasource super-project as the CMake source
# directory, with this repository reachable at megasource/libs/love. This
# script clones megasource next to the repo if missing, creates the
# libs/love junction, then configures and builds.
#
# Usage:
#   .\scripts\build.ps1                 # Release
#   .\scripts\build.ps1 -Config Debug
param(
    [ValidateSet("Debug", "Release", "RelWithDebInfo")]
    [string]$Config = "Release",
    [string]$MegasourceDir,
    [string]$LhatDir,
    [string]$BuildDir,
    [ValidateSet("x64", "ARM64")]
    [string]$Platform = "x64",
    [switch]$ConfigureOnly
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$reposParent = Split-Path $repoRoot -Parent
if (-not $MegasourceDir) { $MegasourceDir = Join-Path $reposParent "megasource" }
if (-not $LhatDir) { $LhatDir = Join-Path $reposParent "lhat" }
if (-not $BuildDir) { $BuildDir = Join-Path $repoRoot "build" }

if (-not (Test-Path (Join-Path $LhatDir "include/lhat.h"))) {
    throw "lhat not found at '$LhatDir' (expected include/lhat.h). Pass -LhatDir."
}

if (-not (Test-Path (Join-Path $MegasourceDir "CMakeLists.txt"))) {
    Write-Host "Cloning megasource into $MegasourceDir"
    git clone https://github.com/love2d/megasource.git $MegasourceDir
    if ($LASTEXITCODE -ne 0) { throw "megasource clone failed" }
}

# megasource expects this repo at libs/love; a junction avoids copying and
# needs no admin rights.
$loveLink = Join-Path $MegasourceDir "libs/love"
if (Test-Path $loveLink) {
    $item = Get-Item $loveLink -Force
    if ($item.LinkType -ne "Junction") {
        throw "'$loveLink' exists and is not a junction. Remove it manually."
    }
    if ((Resolve-Path $item.Target).Path -ne $repoRoot) {
        Write-Host "Re-pointing junction $loveLink -> $repoRoot"
        [System.IO.Directory]::Delete($loveLink)
        New-Item -ItemType Junction -Path $loveLink -Target $repoRoot | Out-Null
    }
} else {
    New-Item -ItemType Junction -Path $loveLink -Target $repoRoot | Out-Null
    Write-Host "Created junction $loveLink -> $repoRoot"
}

cmake -S $MegasourceDir -B $BuildDir -A $Platform "-DLHATOVE_LHAT_DIR=$LhatDir"
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed" }

if ($ConfigureOnly) { return }

cmake --build $BuildDir --config $Config --target love lovec
if ($LASTEXITCODE -ne 0) { throw "Build failed" }

Write-Host ""
Write-Host "Build finished. Executables under: $BuildDir\love\$Config\"
