# Automated Toolchain Setup for Gravity Rush 2 PC Port (gr2port)
$ErrorActionPreference = "Continue"

Write-Host "==========================================" -ForegroundColor Cyan
Write-Host " Phase 1: Toolchain & Dependency Provisioning" -ForegroundColor Cyan
Write-Host "==========================================" -ForegroundColor Cyan

function Install-WingetPackage($id, $name) {
    Write-Host "[Toolchain] Checking $name ($id)..." -ForegroundColor Yellow
    $check = winget list --id $id
    if ($LASTEXITCODE -eq 0 -and $check -match $id) {
        Write-Host "[Toolchain] $name is already installed." -ForegroundColor Green
    } else {
        Write-Host "[Toolchain] Installing $name via winget..." -ForegroundColor Yellow
        winget install --id $id --silent --accept-package-agreements --accept-source-agreements
    }
}

Install-WingetPackage "Kitware.CMake" "CMake"
Install-WingetPackage "Ninja-build.Ninja" "Ninja"
Install-WingetPackage "LLVM.LLVM" "LLVM / Clang"
Install-WingetPackage "Microsoft.OpenJDK.21" "OpenJDK 21"

Write-Host "==========================================" -ForegroundColor Green
Write-Host " Toolchain Provisioning Finished." -ForegroundColor Green
Write-Host "==========================================" -ForegroundColor Green
