# setup.ps1 — One-shot setup: installs vcpkg, configures CMake, builds RVSim
# Run this from the project root: .\setup.ps1
# Requires: Visual Studio 2022 with "Desktop development with C++" workload

$ErrorActionPreference = "Stop"
$ProjectRoot = $PSScriptRoot

Write-Host ""
Write-Host "  =============================" -ForegroundColor Cyan
Write-Host "  RVSim — Build Setup" -ForegroundColor Cyan
Write-Host "  =============================" -ForegroundColor Cyan
Write-Host ""

# -----------------------------------------------------------------------
# Step 1: Clone vcpkg if not already present
# -----------------------------------------------------------------------
$VcpkgRoot = "C:\vcpkg"

if (-Not (Test-Path "$VcpkgRoot\vcpkg.exe")) {
    Write-Host "[1/5] Cloning vcpkg..." -ForegroundColor Yellow
    if (-Not (Test-Path $VcpkgRoot)) {
        git clone https://github.com/microsoft/vcpkg.git $VcpkgRoot
    }
    Write-Host "[1/5] Bootstrapping vcpkg..." -ForegroundColor Yellow
    & "$VcpkgRoot\bootstrap-vcpkg.bat" -disableMetrics
} else {
    Write-Host "[1/5] vcpkg already installed at $VcpkgRoot" -ForegroundColor Green
}

$env:VCPKG_ROOT = $VcpkgRoot

# -----------------------------------------------------------------------
# Step 2: Find CMake (VS installs it under Common7\IDE\CommonExtensions)
# -----------------------------------------------------------------------
Write-Host "[2/5] Locating CMake..." -ForegroundColor Yellow

$CMake = "cmake"  # assume it's in PATH (VS adds it)
try {
    $v = & cmake --version 2>&1
    Write-Host "      Found: $($v[0])" -ForegroundColor Green
} catch {
    # Try VS-bundled CMake (VS 2022 Community installs to VS\18\...)
    $vsCMake = "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
    if (-Not (Test-Path $vsCMake)) {
        # Fallback: older VS 2022 path numbering
        $vsCMake = "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
    }
    if (Test-Path $vsCMake) {
        $CMake = $vsCMake
        Write-Host "      Found VS-bundled CMake" -ForegroundColor Green
    } else {
        Write-Host "ERROR: CMake not found. Open VS Installer → Modify → C++ → tick 'CMake tools'" -ForegroundColor Red
        exit 1
    }
}

# -----------------------------------------------------------------------
# Step 3: Find Python3 (needed for embed_frontend.py)
# -----------------------------------------------------------------------
Write-Host "[3/5] Locating Python 3..." -ForegroundColor Yellow
$Python = "python"
try {
    $pv = & python --version 2>&1
    Write-Host "      Found: $pv" -ForegroundColor Green
} catch {
    Write-Host "ERROR: Python 3 not found. Download from python.org" -ForegroundColor Red
    exit 1
}

# -----------------------------------------------------------------------
# Step 4: CMake configure  
# -----------------------------------------------------------------------
Write-Host "[4/5] Configuring CMake (this installs vcpkg packages — may take a few minutes)..." -ForegroundColor Yellow

$BuildDir = Join-Path $ProjectRoot "build"

& $CMake -B "$BuildDir" -S "$ProjectRoot" `
    "-DCMAKE_TOOLCHAIN_FILE=$VcpkgRoot\scripts\buildsystems\vcpkg.cmake" `
    "-DCMAKE_BUILD_TYPE=Release" `
    "-DVCPKG_TARGET_TRIPLET=x64-windows-static"

if ($LASTEXITCODE -ne 0) {
    Write-Host "CMake configure FAILED. Check errors above." -ForegroundColor Red
    exit 1
}

Write-Host "[4/5] CMake configure succeeded!" -ForegroundColor Green

# -----------------------------------------------------------------------
# Step 5: Build
# -----------------------------------------------------------------------
Write-Host "[5/5] Building RVSim (Release)..." -ForegroundColor Yellow

& $CMake --build "$BuildDir" --config Release --parallel

if ($LASTEXITCODE -ne 0) {
    Write-Host "Build FAILED. Check errors above." -ForegroundColor Red
    exit 1
}

Write-Host ""
Write-Host "  =============================" -ForegroundColor Green
Write-Host "  BUILD SUCCEEDED!" -ForegroundColor Green
Write-Host "  =============================" -ForegroundColor Green
Write-Host ""
Write-Host "  Executable: $ProjectRoot\bin\Release\RVSim.exe" -ForegroundColor Cyan
Write-Host ""
Write-Host "  To run: .\bin\Release\RVSim.exe" -ForegroundColor Cyan
Write-Host ""

# Offer to launch
$launch = Read-Host "Launch RVSim now? (y/n)"
if ($launch -eq 'y' -or $launch -eq 'Y') {
    Start-Process "$ProjectRoot\bin\Release\RVSim.exe"
}
