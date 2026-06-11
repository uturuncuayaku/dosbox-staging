# DOSBox Staging - Windows 11 Development Environment Diagnostics
# ================================================================
# Run this script to diagnose common setup and build issues
# Usage: powershell -ExecutionPolicy Bypass -File diagnose-dev-env-windows.ps1

param(
    [switch]$Verbose = $false,
    [switch]$RepairVcpkg = $false,
    [switch]$ClearCache = $false
)

$ErrorActionPreference = "Continue"
$diagnostics = @()

# Color codes
$colors = @{
    Success = 'Green'
    Error = 'Red'
    Warning = 'Yellow'
    Info = 'Cyan'
    Highlight = 'Magenta'
}

function Write-Log {
    param(
        [string]$Message,
        [ValidateSet('Success', 'Error', 'Warning', 'Info', 'Highlight')]
        [string]$Type = 'Info'
    )
    
    $timestamp = Get-Date -Format "HH:mm:ss"
    $color = $colors[$Type]
    Write-Host "[$timestamp] " -NoNewline
    Write-Host $Message -ForegroundColor $color
}

function Test-CommandExists {
    param([string]$Command)
    return $null -ne (Get-Command $Command -ErrorAction SilentlyContinue)
}

function Get-CommandVersion {
    param([string]$Command)
    
    try {
        $version = & $Command --version 2>&1 | Select-Object -First 1
        return $version
    }
    catch {
        return "Unable to get version"
    }
}

function Add-Diagnostic {
    param(
        [string]$Name,
        [string]$Status,
        [string]$Details = ""
    )
    
    $diagnostics += @{
        Name = $Name
        Status = $Status
        Details = $Details
    }
}

# ============================================================
# Diagnostics
# ============================================================

Write-Log "========================================" -Type Highlight
Write-Log "DOSBox Staging - Development Environment Diagnostics" -Type Highlight
Write-Log "========================================" -Type Highlight
Write-Host ""

# System Info
Write-Log "Gathering system information..." -Type Info
$osInfo = Get-CimInstance Win32_OperatingSystem
$os = $osInfo.Caption
$buildNumber = [int]($osInfo.BuildNumber)

Write-Host "OS: $os (Build $buildNumber)"
$ram = [math]::Round((Get-CimInstance Win32_ComputerSystem).TotalPhysicalMemory / 1GB, 2)
Write-Host "RAM: $ram GB"

$diskFree = Get-Volume | Where-Object { $_.DriveLetter -eq 'C' } | Select-Object -ExpandProperty SizeRemaining
$diskFreeGb = [math]::Round($diskFree / 1GB, 2)
Write-Host "Disk Free (C:): $diskFreeGb GB"

Write-Host ""

# Check Windows 11 Build
Write-Log "Checking Windows 11 Build..." -Type Info
if ($buildNumber -ge 22000) {
    Add-Diagnostic "Windows 11 Build" "✓ Pass" "Build $buildNumber (required: 22000+)"
    Write-Log "✓ Windows 11 Build 22000+ detected" -Type Success
}
else {
    Add-Diagnostic "Windows 11 Build" "✗ Fail" "Build $buildNumber (required: 22000+)"
    Write-Log "✗ Windows 11 Build 22000+ NOT found" -Type Error
}
Write-Host ""

# Check Administrator Rights
Write-Log "Checking administrator privileges..." -Type Info
$currentUser = [Security.Principal.WindowsIdentity]::GetCurrent()
$principal = New-Object Security.Principal.WindowsPrincipal($currentUser)
$isAdmin = $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)

if ($isAdmin) {
    Add-Diagnostic "Administrator Privileges" "✓ Pass" "Running with admin rights"
    Write-Log "✓ Running with administrator privileges" -Type Success
}
else {
    Add-Diagnostic "Administrator Privileges" "⚠ Warning" "Not running as admin (some operations may fail)"
    Write-Log "⚠ Not running with administrator privileges (recommended for setup)" -Type Warning
}
Write-Host ""

# Check Disk Space
Write-Log "Checking disk space..." -Type Info
if ($diskFreeGb -ge 20) {
    Add-Diagnostic "Disk Space" "✓ Pass" "$diskFreeGb GB free (recommended: 20+ GB)"
    Write-Log "✓ Sufficient disk space available" -Type Success
}
elseif ($diskFreeGb -ge 10) {
    Add-Diagnostic "Disk Space" "⚠ Warning" "$diskFreeGb GB free (recommended: 20+ GB)"
    Write-Log "⚠ Limited disk space (builds may fail if < 15 GB free)" -Type Warning
}
else {
    Add-Diagnostic "Disk Space" "✗ Fail" "$diskFreeGb GB free (minimum: 10 GB)"
    Write-Log "✗ Insufficient disk space for builds" -Type Error
}
Write-Host ""

# Check Internet
Write-Log "Checking internet connectivity..." -Type Info
try {
    $test = Invoke-WebRequest -Uri "https://api.github.com" -UseBasicParsing -TimeoutSec 5 -ErrorAction Stop
    Add-Diagnostic "Internet Connection" "✓ Pass" "GitHub API accessible"
    Write-Log "✓ Internet connection verified" -Type Success
}
catch {
    Add-Diagnostic "Internet Connection" "✗ Fail" "Cannot reach GitHub (required for vcpkg)"
    Write-Log "✗ Internet connection issue detected" -Type Error
}
Write-Host ""

# Check Tools
Write-Log "========================================" -Type Highlight
Write-Log "Checking Required Tools" -Type Highlight
Write-Log "========================================" -Type Highlight
Write-Host ""

# Git
Write-Log "Checking Git..." -Type Info
if (Test-CommandExists "git") {
    $gitVersion = Get-CommandVersion "git"
    Add-Diagnostic "Git" "✓ Pass" $gitVersion
    Write-Host "✓ Git installed: $gitVersion"
}
else {
    Add-Diagnostic "Git" "✗ Fail" "Git not found on PATH"
    Write-Log "✗ Git not found. Install via: winget install --id Git.Git --exact" -Type Error
}
Write-Host ""

# C++ Compiler (MSVC)
Write-Log "Checking C++ Compiler (MSVC)..." -Type Info
if (Test-CommandExists "cl") {
    $clVersion = & cl 2>&1 | Select-Object -First 1
    Add-Diagnostic "C++ Compiler" "✓ Pass" $clVersion
    Write-Host "✓ MSVC Compiler available: $clVersion"
}
else {
    Add-Diagnostic "C++ Compiler" "✗ Fail" "cl.exe not found on PATH"
    Write-Log "✗ MSVC C++ Compiler not found. Install Visual Studio 2022 Community with C++ tools." -Type Error
}
Write-Host ""

# CMake
Write-Log "Checking CMake..." -Type Info
if (Test-CommandExists "cmake") {
    $cmakeVersion = Get-CommandVersion "cmake" | Select-Object -First 1
    # Check version number
    if ($cmakeVersion -match "3\.(\d+)") {
        $minorVersion = [int]$matches[1]
        if ($minorVersion -ge 25) {
            Add-Diagnostic "CMake" "✓ Pass" $cmakeVersion
            Write-Host "✓ CMake installed: $cmakeVersion"
        }
        else {
            Add-Diagnostic "CMake" "⚠ Warning" "$cmakeVersion (minimum: 3.25)"
            Write-Log "⚠ CMake version is older than recommended (3.25+)" -Type Warning
        }
    }
    else {
        Add-Diagnostic "CMake" "✓ Pass" $cmakeVersion
        Write-Host "✓ CMake installed: $cmakeVersion"
    }
}
else {
    Add-Diagnostic "CMake" "✗ Fail" "CMake not found on PATH"
    Write-Log "✗ CMake not found. Install via: winget install --id Kitware.CMake --exact" -Type Error
}
Write-Host ""

# Python
Write-Log "Checking Python..." -Type Info
if (Test-CommandExists "python") {
    $pythonVersion = Get-CommandVersion "python"
    if ($pythonVersion -match "3\.(\d+)") {
        $minorVersion = [int]$matches[1]
        if ($minorVersion -ge 10) {
            Add-Diagnostic "Python" "✓ Pass" $pythonVersion
            Write-Host "✓ Python installed: $pythonVersion"
        }
        else {
            Add-Diagnostic "Python" "⚠ Warning" "$pythonVersion (minimum: 3.10)"
            Write-Log "⚠ Python version is older than recommended (3.10+)" -Type Warning
        }
    }
    else {
        Add-Diagnostic "Python" "✓ Pass" $pythonVersion
        Write-Host "✓ Python installed: $pythonVersion"
    }
}
else {
    Add-Diagnostic "Python" "⚠ Warning" "Python not found (only needed for documentation builds)"
    Write-Log "⚠ Python not found (optional - only needed for documentation builds)" -Type Warning
}
Write-Host ""

# Optional Tools
Write-Log "========================================" -Type Highlight
Write-Log "Checking Optional Tools" -Type Highlight
Write-Log "========================================" -Type Highlight
Write-Host ""

# Ninja
Write-Log "Checking Ninja..." -Type Info
if (Test-CommandExists "ninja") {
    $ninjaVersion = Get-CommandVersion "ninja"
    Write-Host "✓ Ninja installed: $ninjaVersion (faster builds)"
}
else {
    Write-Log "ℹ Ninja not installed (optional - enables faster parallel builds)" -Type Info
}
Write-Host ""

# Git GUI Tools
Write-Log "Checking Git GUI tools..." -Type Info
$hasGit = Test-CommandExists "tortoisegitproc"
$hasGitHub = Test-CommandExists "gh"
if ($hasGit -or $hasGitHub) {
    if ($hasGit) { Write-Host "✓ TortoiseGit installed" }
    if ($hasGitHub) { Write-Host "✓ GitHub CLI installed" }
}
else {
    Write-Log "ℹ No Git GUI tools found (optional)" -Type Info
}
Write-Host ""

# Check Project Structure
Write-Log "========================================" -Type Highlight
Write-Log "Checking Project Structure" -Type Highlight
Write-Log "========================================" -Type Highlight
Write-Host ""

$projectFiles = @(
    'CMakeLists.txt',
    'vcpkg.json',
    'README.md',
    '.git'
)

$projectPath = Get-Location

$missingFiles = @()
foreach ($file in $projectFiles) {
    $filePath = Join-Path $projectPath $file
    if (Test-Path $filePath) {
        Write-Host "✓ Found: $file"
    }
    else {
        Write-Host "✗ Missing: $file"
        $missingFiles += $file
    }
}

if ($missingFiles.Count -gt 0) {
    Write-Log "⚠ Project files missing. Are you in the dosbox-staging directory?" -Type Warning
    Add-Diagnostic "Project Structure" "⚠ Warning" "Not all project files found"
}
else {
    Add-Diagnostic "Project Structure" "✓ Pass" "All project files found"
    Write-Log "✓ Project structure looks correct" -Type Success
}
Write-Host ""

# Check Build Directory
Write-Log "Checking build directories..." -Type Info
$buildDir = Join-Path $projectPath "build"
$debugBuildDir = Join-Path $buildDir "debug-windows"

if (Test-Path $buildDir) {
    $buildSize = (Get-ChildItem $buildDir -Recurse | Measure-Object -Property Length -Sum).Sum / 1MB
    Write-Host "✓ Build directory exists: $([math]::Round($buildSize, 2)) MB"
    
    if (Test-Path $debugBuildDir) {
        Write-Host "  └─ Debug build found"
        $exePath = Join-Path $debugBuildDir "dosbox.exe"
        if (Test-Path $exePath) {
            Write-Host "    └─ ✓ Executable present: dosbox.exe"
            Add-Diagnostic "Executable" "✓ Pass" "Build executable found"
        }
        else {
            Write-Host "    └─ ✗ Executable not found (build may be incomplete)"
        }
    }
}
else {
    Write-Log "ℹ No build directory found (this is normal before first build)" -Type Info
}
Write-Host ""

# Check PATH
Write-Log "Checking PATH environment variable..." -Type Info
$pathErrors = @()

$requiredPaths = @(
    "Git",
    "CMake",
    "Python"
)

$pathValue = $env:PATH
if ($pathValue -match "cmake|CMake" -eq $false) {
    $pathErrors += "CMake"
}
if ($pathValue -match "python|Python" -eq $false) {
    $pathErrors += "Python"
}
if ($pathValue -match "git|Git" -eq $false) {
    $pathErrors += "Git"
}

if ($pathErrors.Count -eq 0) {
    Write-Log "✓ All required tools appear to be on PATH" -Type Success
    Add-Diagnostic "PATH Configuration" "✓ Pass" "All tools on system PATH"
}
else {
    Write-Log "⚠ Missing from PATH: $($pathErrors -join ', ')" -Type Warning
    Add-Diagnostic "PATH Configuration" "⚠ Warning" "Some tools may not be on PATH"
}
Write-Host ""

# Repair Options
Write-Log "========================================" -Type Highlight
Write-Log "Repair Options" -Type Highlight
Write-Log "========================================" -Type Highlight
Write-Host ""

if ($ClearCache) {
    Write-Log "Clearing CMake cache..." -Type Highlight
    if (Test-Path "$buildDir/CMakeCache.txt") {
        Remove-Item "$buildDir/CMakeCache.txt" -Force -ErrorAction SilentlyContinue
        Write-Log "✓ CMake cache cleared" -Type Success
    }
    Write-Host ""
}

if ($RepairVcpkg) {
    Write-Log "Attempting vcpkg repair..." -Type Highlight
    if (Test-Path $buildDir) {
        Write-Log "Removing build directory for full rebuild..." -Type Warning
        Remove-Item $buildDir -Recurse -Force -ErrorAction SilentlyContinue
        Write-Log "✓ Build directory cleared" -Type Success
        Write-Log "Run: cmake --preset debug-windows" -Type Info
        Write-Log "Then: cmake --build --preset debug-windows" -Type Info
    }
    Write-Host ""
}

# Summary
Write-Log "========================================" -Type Highlight
Write-Log "Diagnostic Summary" -Type Highlight
Write-Log "========================================" -Type Highlight
Write-Host ""

$passed = ($diagnostics | Where-Object { $_.Status -match "✓ Pass" }).Count
$failed = ($diagnostics | Where-Object { $_.Status -match "✗ Fail" }).Count
$warnings = ($diagnostics | Where-Object { $_.Status -match "⚠" }).Count

Write-Host "Passed:  $passed"
Write-Host "Failed:  $failed"
Write-Host "Warnings: $warnings"
Write-Host ""

if ($failed -eq 0) {
    Write-Log "Environment looks good! Ready to build." -Type Success
}
else {
    Write-Log "Issues detected. Please fix failures above and run diagnostics again." -Type Error
}

Write-Host ""

# Detailed Report
if ($Verbose) {
    Write-Log "Detailed Diagnostic Report:" -Type Highlight
    Write-Host ""
    
    foreach ($diag in $diagnostics) {
        $statusColor = switch ($diag.Status) {
            { $_ -match "✓" } { 'Green' }
            { $_ -match "✗" } { 'Red' }
            { $_ -match "⚠" } { 'Yellow' }
            default { 'White' }
        }
        
        Write-Host "$($diag.Name): " -NoNewline
        Write-Host $diag.Status -ForegroundColor $statusColor
        if ($diag.Details) {
            Write-Host "  └─ $($diag.Details)" -ForegroundColor Gray
        }
    }
}

Write-Host ""
Write-Log "Diagnostics complete." -Type Info

# Helpful Commands
Write-Host ""
Write-Log "Helpful Commands:" -Type Highlight
Write-Host "  Rebuild CMake cache:  powershell -ExecutionPolicy Bypass -File diagnose-dev-env-windows.ps1 -ClearCache"
Write-Host "  Full vcpkg repair:    powershell -ExecutionPolicy Bypass -File diagnose-dev-env-windows.ps1 -RepairVcpkg"
Write-Host "  Verbose output:       powershell -ExecutionPolicy Bypass -File diagnose-dev-env-windows.ps1 -Verbose"
Write-Host ""

# Press key to exit
if ($Host.Name -eq "ConsoleHost") {
    Write-Host ""
    Write-Host "Press any key to exit..." -ForegroundColor Gray
    $null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")
}
