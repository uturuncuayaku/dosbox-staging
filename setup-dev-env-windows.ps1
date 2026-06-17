# DOSBox Staging - Windows 11 Development Environment Setup
# ============================================================
# This script configures a complete development environment for building DOSBox Staging on Windows 11
# Run with: powershell -ExecutionPolicy Bypass -File setup-dev-env-windows.ps1
#
# Features:
# - Installs all required dependencies via winget
# - Supports GitHub releases for tools
# - Includes optional development tools
# - Handles elevation and privilege requirements
# - Creates necessary environment configurations

param(
    [switch]$SkipOptional = $false,
    [switch]$Verbose = $false
)

# ============================================================
# Configuration
# ============================================================

$ErrorActionPreference = "Stop"
$WarningPreference = "Continue"

# Color codes for output
$colors = @{
    Success = 'Green'
    Error = 'Red'
    Warning = 'Yellow'
    Info = 'Cyan'
    Highlight = 'Magenta'
}

function Write-Log {
    param(
        [Parameter(Mandatory=$true)]
        [string]$Message,
        [ValidateSet('Success', 'Error', 'Warning', 'Info', 'Highlight')]
        [string]$Type = 'Info'
    )
    
    $timestamp = Get-Date -Format "HH:mm:ss"
    $color = $colors[$Type]
    Write-Host "[$timestamp] " -NoNewline
    Write-Host $Message -ForegroundColor $color
}

function Test-IsAdmin {
    $currentUser = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = New-Object Security.Principal.WindowsPrincipal($currentUser)
    return $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

function Request-AdminElevation {
    if (-not (Test-IsAdmin)) {
        Write-Log "This script requires administrator privileges to install system-wide packages." -Type Warning
        Write-Log "Attempting to elevate privileges..." -Type Info
        
        $scriptPath = $MyInvocation.MyCommand.Path
        $arguments = "-ExecutionPolicy Bypass -File `"$scriptPath`""
        
        if ($SkipOptional) {
            $arguments += " -SkipOptional"
        }
        if ($Verbose) {
            $arguments += " -Verbose"
        }
        
        Start-Process powershell -ArgumentList $arguments -Verb RunAs
        exit 0
    }
}

function Test-CommandExists {
    param(
        [Parameter(Mandatory=$true)]
        [string]$Command
    )
    
    return $null -ne (Get-Command $Command -ErrorAction SilentlyContinue)
}

function Install-FromWinget {
    param(
        [Parameter(Mandatory=$true)]
        [string]$PackageId,
        [string]$DisplayName = $PackageId,
        [switch]$Optional = $false
    )
    
    try {
        if (Test-CommandExists $DisplayName.Split('.')[0]) {
            Write-Log "$DisplayName is already installed" -Type Success
            return $true
        }
        
        Write-Log "Installing $DisplayName..." -Type Highlight
        
        # Try to install via winget
        $output = & winget install --id $PackageId --exact --silent 2>&1
        
        if ($LASTEXITCODE -eq 0) {
            Write-Log "$DisplayName installed successfully" -Type Success
            return $true
        }
        else {
            if ($Optional) {
                Write-Log "$DisplayName installation failed (optional, continuing)" -Type Warning
                return $false
            }
            else {
                Write-Log "$DisplayName installation failed: $output" -Type Error
                throw "Failed to install $DisplayName"
            }
        }
    }
    catch {
        if ($Optional) {
            Write-Log "Optional package $DisplayName could not be installed" -Type Warning
            return $false
        }
        throw
    }
}

function Get-GitHubLatestRelease {
    param(
        [Parameter(Mandatory=$true)]
        [string]$Owner,
        [Parameter(Mandatory=$true)]
        [string]$Repo,
        [string]$AssetPattern = $null
    )
    
    try {
        $apiUrl = "https://api.github.com/repos/$Owner/$Repo/releases/latest"
        $response = Invoke-RestMethod -Uri $apiUrl -Headers @{'Accept' = 'application/vnd.github.v3+json'}
        
        if ($AssetPattern) {
            $asset = $response.assets | Where-Object { $_.name -match $AssetPattern } | Select-Object -First 1
            if ($asset) {
                return @{
                    Version = $response.tag_name
                    DownloadUrl = $asset.browser_download_url
                    FileName = $asset.name
                }
            }
        }
        
        return @{
            Version = $response.tag_name
            DownloadUrl = $response.html_url
        }
    }
    catch {
        Write-Log "Could not fetch GitHub release information: $_" -Type Warning
        return $null
    }
}

function Test-InternetConnection {
    try {
        $null = Invoke-WebRequest -Uri "https://github.com" -UseBasicParsing -TimeoutSec 5
        return $true
    }
    catch {
        return $false
    }
}

# ============================================================
# Main Setup Logic
# ============================================================

function Initialize-Setup {
    Write-Log "========================================" -Type Highlight
    Write-Log "DOSBox Staging - Windows 11 Dev Setup" -Type Highlight
    Write-Log "========================================" -Type Highlight
    Write-Host ""
    
    # Check for Internet
    Write-Log "Checking internet connectivity..." -Type Info
    if (-not (Test-InternetConnection)) {
        Write-Log "WARNING: No internet connection detected. Some installations may fail." -Type Warning
    }
    else {
        Write-Log "Internet connection verified" -Type Success
    }
    
    Write-Host ""
    Write-Log "System Information:" -Type Info
    Write-Host "  OS: $(Get-CimInstance Win32_OperatingSystem | Select-Object -ExpandProperty Caption)"
    Write-Host "  Admin: $(if (Test-IsAdmin) { 'Yes' } else { 'No (will request)' })"
    Write-Host ""
}
# ============================================================
# Compiler and Dependency Setup
# ============================================================
function Import-VSEnvironment {
    # The exact path to your standalone Build Tools installation
    $vcvars = "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
    
    # Fallback to vswhere (looking for ALL products, including Build Tools) if the hardcoded path changes
    if (-Not (Test-Path $vcvars)) {
        $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
        if (Test-Path $vswhere) {
            $vsPath = & $vswhere -products * -latest -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
            if ($vsPath) {
                $vcvars = Join-Path $vsPath "VC\Auxiliary\Build\vcvars64.bat"
            }
        }
    }

    if (-Not $vcvars -or -Not (Test-Path $vcvars)) { 
        return $false 
    }

    Write-Log "Loading Visual Studio Build Tools Environment into PowerShell..." -Type Info
    
    # Execute the batch file silently, then run 'set' to output all environment variables
    $cmd = "`"$vcvars`" >NUL && set"
    
    # Parse the output and inject it into the current PowerShell process
    cmd.exe /c $cmd | ForEach-Object {
        if ($_ -match "^([^=]+)=(.*)$") {
            [Environment]::SetEnvironmentVariable($matches[1], $matches[2], "Process")
        }
    }
    return $true
}

function Install-CoreDependencies {
    Write-Log "========================================" -Type Highlight
    Write-Log "Installing Core Dependencies" -Type Highlight
    Write-Log "========================================" -Type Highlight
    Write-Host ""
    
    # Git - essential for cloning repository
    Write-Log "Checking Git installation..." -Type Info
    if (Test-CommandExists "git") {
        $gitVersion = & git --version
        Write-Log "$gitVersion is already installed" -Type Success
    }
    else {
        Install-FromWinget "Git.Git" "Git"
    }
    Write-Host ""
    
# Visual Studio C++ Build Tools
Write-Log "Checking for Visual Studio C++ Compiler..." -Type Info

# Attempt to load the environment first using our helper function
$envLoaded = Import-VSEnvironment

if ($envLoaded -and (Test-CommandExists "cl")) {
    Write-Log "Visual Studio C++ compiler is already installed and loaded." -Type Success
}
else {
    Write-Log "C++ Compiler not found. Initiating automated download and installation..." -Type Highlight
    
    # Define paths and the official Microsoft aka.ms permalink for Build Tools
    $installerPath = Join-Path $env:TEMP "vs_buildtools.exe"
    $downloadUrl = "https://aka.ms/vs/17/release/vs_buildtools.exe"
    
    Write-Log "Downloading VS 2022 Build Tools bootstrapper..." -Type Info
    Invoke-WebRequest -Uri $downloadUrl -OutFile $installerPath
    
    Write-Log "Installing C++ workloads silently (This will take several minutes)..." -Type Warning
    
    # Command line arguments for a silent, unattended installation with specific workloads
    $installArgs = @(
        "--quiet",
        "--wait",
        "--norestart",
        "--add", "Microsoft.VisualStudio.Workload.VCTools",
        "--add", "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
        "--add", "Microsoft.VisualStudio.Component.VC.CMake.Project",
        "--includeRecommended"
    )
    
    # Execute the installer and wait for it to finish
    $process = Start-Process -FilePath $installerPath -ArgumentList $installArgs -Wait -PassThru -NoNewWindow
    
    # Exit codes: 0 is success, 3010 is success but requires a reboot
    if ($process.ExitCode -eq 0 -or $process.ExitCode -eq 3010) {
        Write-Log "Visual Studio Build Tools installed successfully." -Type Success
        
        if ($process.ExitCode -eq 3010) {
            Write-Log "Note: A system reboot may be required later to finalize all components." -Type Warning
        }
        
        # Load the newly installed environment variables into the current session
        Import-VSEnvironment | Out-Null
    }
    else {
        Write-Log "Visual Studio installation failed with exit code: $($process.ExitCode)" -Type Error
        Write-Log "You may need to run the installer manually from $installerPath" -Type Error
        exit 1
    }
    
    # Clean up the bootstrapper executable
    Remove-Item -Path $installerPath -Force -ErrorAction SilentlyContinue
}
Write-Host ""
    Write-Host ""
    
    # CMake
    Write-Log "Checking CMake installation..." -Type Info
    if (Test-CommandExists "cmake") {
        $cmakeVersion = & cmake --version | Select-Object -First 1
        Write-Log "$cmakeVersion is already installed" -Type Success
    }
    else {
        Install-FromWinget "Kitware.CMake" "CMake"
    }
    Write-Host ""
    
    # Python 3 (for documentation building)
    Write-Log "Checking Python 3 installation..." -Type Info
    if (Test-CommandExists "python") {
        $pythonVersion = & python --version
        Write-Log "$pythonVersion is already installed" -Type Success
    }
    else {
        Write-Log "Installing Python 3 (required for documentation building)..." -Type Highlight
        Install-FromWinget "Python.Python.3.12" "Python 3"
    }
    Write-Host ""
}

function Configure-Vcpkg {
    Write-Log "========================================" -Type Highlight
    Write-Log "Configuring vcpkg" -Type Highlight
    Write-Log "========================================" -Type Highlight
    Write-Host ""
    
    # Check if vcpkg is already installed with Visual Studio
    $vsPath = & "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64 `>nul 2`>`&1
    
    Write-Log "Visual Studio vcpkg integration check..." -Type Info
    Write-Host ""
    
    Write-Log "vcpkg will be managed through Visual Studio's integrated vcpkg." -Type Info
    Write-Log "When you open the project in Visual Studio or use CMake with presets," -Type Info
    Write-Log "vcpkg will automatically download and build all required dependencies:" -Type Info
    Write-Host ""
    Write-Host "  Required C++ Dependencies:"
    Write-Host "  - asio (networking library)"
    Write-Host "  - gtest (testing framework)"
    Write-Host "  - iir1 (DSP library)"
    Write-Host "  - libmt32emu (MT-32 emulation)"
    Write-Host "  - libpng (PNG support)"
    Write-Host "  - opusfile (Opus audio support)"
    Write-Host "  - fluidsynth (MIDI support)"
    Write-Host "  - sdl2 (graphics and input)"
    Write-Host "  - sdl2-image (image support)"
    Write-Host "  - speexdsp (audio DSP)"
    Write-Host "  - zlib-ng (compression)"
    Write-Host ""
    
    Write-Log "These will be installed automatically during the first CMake configuration" -Type Success
    Write-Host ""
}

function Install-OptionalTools {
    Write-Log "========================================" -Type Highlight
    Write-Log "Installing Optional Development Tools" -Type Highlight
    Write-Log "========================================" -Type Highlight
    Write-Host ""
    
    if ($SkipOptional) {
        Write-Log "Skipping optional tools as requested" -Type Info
        Write-Host ""
        return
    }
    
    # Ninja Build System - faster builds than Visual Studio's default
    Write-Log "Checking Ninja build system..." -Type Info
    if (Test-CommandExists "ninja") {
        $ninjaVersion = & ninja --version
        Write-Log "Ninja $ninjaVersion is already installed (faster build system)" -Type Success
    }
    else {
        Install-FromWinget "Ninja-build.Ninja" "Ninja" -Optional
        if ($?) {
            Write-Log "Ninja installed successfully - enables faster parallel builds" -Type Success
        }
    }
    Write-Host ""
    
    # Visual Studio Code
    Write-Log "Checking Visual Studio Code..." -Type Info
    if (Test-CommandExists "code") {
        Write-Log "Visual Studio Code is already installed" -Type Success
    }
    else {
        Install-FromWinget "Microsoft.VisualStudioCode" "Visual Studio Code" -Optional
        if ($?) {
            Write-Log "Visual Studio Code installed successfully" -Type Success
        }
    }
    Write-Host ""
    
    # Git GUI Tools
    Write-Log "Checking Git GUI tools..." -Type Info
    $hasTortoiseGit = Test-Path "C:\Program Files\TortoiseGit\bin\TortoiseGitProc.exe"
    if ($hasTortoiseGit -or (Test-CommandExists "tortoisegitproc")) {
        Write-Log "TortoiseGit is already installed" -Type Success
    }
    else {
        Install-FromWinget "TortoiseGit.TortoiseGit" "TortoiseGit" -Optional
        if ($?) {
            Write-Log "TortoiseGit installed successfully for Git GUI operations" -Type Success
        }
    }
    Write-Host ""
    
    # Notepad++
    Write-Log "Checking Notepad++ (optional editor)..." -Type Info
    if (Test-CommandExists "notepad++") {
        Write-Log "Notepad++ is already installed" -Type Success
    }
    else {
        Install-FromWinget "Notepad++.Notepad++" "Notepad++" -Optional
        if ($?) {
            Write-Log "Notepad++ installed successfully" -Type Success
        }
    }
    Write-Host ""
}

function Setup-ProjectDirectories {
    Write-Log "========================================" -Type Highlight
    Write-Log "Setting Up Project Directories" -Type Highlight
    Write-Log "========================================" -Type Highlight
    Write-Host ""
    
    $projectRoot = Split-Path -Parent $PSScriptRoot
    $buildDir = Join-Path $projectRoot "build"
    
    if (-not (Test-Path $buildDir)) {
        Write-Log "Creating build directory at: $buildDir" -Type Info
        New-Item -ItemType Directory -Path $buildDir | Out-Null
        Write-Log "Build directory created" -Type Success
    }
    else {
        Write-Log "Build directory already exists: $buildDir" -Type Info
    }
    
    Write-Host ""
}

function Show-PostInstallInstructions {
    Write-Log "========================================" -Type Highlight
    Write-Log "Setup Complete!" -Type Highlight
    Write-Log "========================================" -Type Highlight
    Write-Host ""
    
    Write-Log "Next Steps to Start Contributing:" -Type Highlight
    Write-Host ""
    
    Write-Host "1. VERIFY INSTALLATION" -ForegroundColor Cyan
    Write-Host "   Open a new PowerShell or Command Prompt and run:"
    Write-Host "   cmake --version"
    Write-Host "   git --version"
    Write-Host "   python --version"
    Write-Host ""
    
    Write-Host "2. CLONE THE REPOSITORY (if not already done)" -ForegroundColor Cyan
    Write-Host "   git clone https://github.com/dosbox-staging/dosbox-staging.git"
    Write-Host "   cd dosbox-staging"
    Write-Host ""
    
    Write-Host "3. VIEW AVAILABLE CMAKE PRESETS" -ForegroundColor Cyan
    Write-Host "   cmake --list-presets"
    Write-Host ""
    
    Write-Host "4. BUILD USING CMAKE" -ForegroundColor Cyan
    Write-Host "   Option A - Command Line (Recommended for development):"
    Write-Host "   cmake --preset debug-windows"
    Write-Host "   cmake --build --preset debug-windows"
    Write-Host ""
    Write-Host "   Option B - Visual Studio IDE:"
    Write-Host "   1. Open Visual Studio 2022"
    Write-Host "   2. Select 'Open a local folder'"
    Write-Host "   3. Navigate to the dosbox-staging directory"
    Write-Host "   4. Select a CMake preset (debug-windows or release-windows)"
    Write-Host "   5. Press F7 or select Build > Build All"
    Write-Host ""
    
    Write-Host "5. RUN THE BUILD" -ForegroundColor Cyan
    Write-Host "   cmake --build --preset debug-windows -- -v"
    Write-Host ""
    
    Write-Host "6. INSTALL DOCUMENTATION (Optional)" -ForegroundColor Cyan
    Write-Host "   To build documentation:"
    Write-Host "   cmake --preset debug-windows-manual"
    Write-Host "   cmake --build --preset debug-windows-manual"
    Write-Host ""
    
    Write-Host "7. FIRST-TIME BUILD NOTES" -ForegroundColor Cyan
    Write-Host "   - First build will take longer as vcpkg downloads and builds dependencies"
    Write-Host "   - Subsequent builds will be much faster"
    Write-Host "   - Build artifacts are in: build/debug-windows/"
    Write-Host "   - Executable will be at: build/debug-windows/dosbox"
    Write-Host ""
    
    Write-Host "8. CONTRIBUTING" -ForegroundColor Cyan
    Write-Host "   - Read: docs/CONTRIBUTING.md"
    Write-Host "   - Code style: See .claude/rules/code-style.md"
    Write-Host "   - Commit format: See .claude/rules/commits.md"
    Write-Host ""
    
    Write-Host "RESOURCES:" -ForegroundColor Magenta
    Write-Host "   Main Website: https://www.dosbox-staging.org/"
    Write-Host "   Repository: https://github.com/dosbox-staging/dosbox-staging"
    Write-Host "   Contributing Guide: https://www.dosbox-staging.org/contribute/"
    Write-Host "   Issue Tracker: https://github.com/dosbox-staging/dosbox-staging/issues"
    Write-Host ""
    
    Write-Log "Environment setup is complete! You're ready to start contributing to DOSBox Staging." -Type Success
    Write-Host ""
}

function Test-SetupCompletion {
    Write-Log "========================================" -Type Highlight
    Write-Log "Verifying Installation" -Type Highlight
    Write-Log "========================================" -Type Highlight
    Write-Host ""
    
    $checks = @{
        'Git' = { Test-CommandExists 'git' }
        'CMake' = { Test-CommandExists 'cmake' }
        'Python' = { Test-CommandExists 'python' }
        'Visual Studio C++ Compiler' = { Test-CommandExists 'cl' }
    }
    
    $allPassed = $true
    foreach ($name in $checks.Keys) {
        $result = & $checks[$name]
        if ($result) {
            Write-Log "$name ✓" -Type Success
        }
        else {
            Write-Log "$name ✗ (FAILED)" -Type Error
            $allPassed = $false
        }
    }
    
    Write-Host ""
    if ($allPassed) {
        Write-Log "All required tools are installed and ready!" -Type Success
    }
    else {
        Write-Log "Some required tools failed verification. Please check the errors above." -Type Error
        exit 1
    }
    
    Write-Host ""
}

# ============================================================
# Main Execution Flow
# ============================================================

try {
    # Request elevation if needed
    Request-AdminElevation
    
    # Initialize
    Initialize-Setup
    
    # Install dependencies
    Install-CoreDependencies
    Configure-Vcpkg
    Install-OptionalTools
    
    # Setup project structure
    Setup-ProjectDirectories
    
    # Verify installation
    Test-SetupCompletion
    
    # Show instructions
    Show-PostInstallInstructions
    
    Write-Log "Press any key to exit..." -Type Info
    $null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")
}
catch {
    Write-Log "Setup failed: $_" -Type Error
    Write-Host $_.ScriptStackTrace
    exit 1
}
