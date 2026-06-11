# DOSBox Staging - Windows 11 Development Setup Guide

Complete guide for setting up a Windows 11 development environment to contribute to DOSBox Staging.

## Table of Contents

1. [Quick Start](#quick-start)
2. [System Requirements](#system-requirements)
3. [Automated Setup](#automated-setup)
4. [Manual Installation](#manual-installation)
5. [Building the Project](#building-the-project)
6. [Troubleshooting](#troubleshooting)
7. [Next Steps](#next-steps)

---

## Quick Start

**For most users, copy and paste this single command:**

```powershell
powershell -ExecutionPolicy Bypass -File setup-dev-env-windows.ps1
```

**The script will:**
- Detect your system configuration
- Request administrator privileges (if needed)
- Download and install all dependencies via winget
- Configure the build environment
- Verify everything works

---

## System Requirements

### Minimum Requirements

- **OS:** Windows 11 (Build 22000 or later)
- **RAM:** 8 GB (16 GB recommended for faster builds)
- **Disk Space:** 
  - 10 GB for Visual Studio 2022 Community
  - 5 GB for build artifacts and dependencies
  - 2 GB for the repository
- **Internet Connection:** Required for installation and during first build

### Hardware for Optimal Development

- **CPU:** Multi-core processor (6+ cores recommended for parallel builds)
- **RAM:** 16 GB+
- **Storage:** SSD (fast builds, especially with Ninja)

### Required Administrator Privileges

The setup script will request elevation to:
- Install system-wide packages
- Modify environment variables
- Create build directories

---

## Automated Setup

### Step 1: Verify Prerequisites

1. **Windows 11 Update:**
   ```powershell
   winver
   ```
   Ensure you're on Windows 11 Build 22000+

2. **Administrator Access:**
   - Press `Win + R`, type `cmd`, press `Ctrl+Shift+Enter`
   - If successful, you have admin rights
   - The setup script will handle elevation automatically

3. **Internet Connection:**
   - Stable connection required (300+ MB for initial setup)

### Step 2: Run the Setup Script

**Option A: Standard Installation** (Includes optional tools)
```powershell
powershell -ExecutionPolicy Bypass -File setup-dev-env-windows.ps1
```

**Option B: Core Only** (Skip optional tools like VSCode, Ninja)
```powershell
powershell -ExecutionPolicy Bypass -File setup-dev-env-windows.ps1 -SkipOptional
```

**Option C: With Verbose Output**
```powershell
powershell -ExecutionPolicy Bypass -File setup-dev-env-windows.ps1 -Verbose
```

### Step 3: Wait for Installation

The script will:
1. Check for existing installations
2. Request elevation (if not already running as admin)
3. Install Git (if missing)
4. Install Visual Studio 2022 Community
   - **IMPORTANT:** When the installer opens, ensure you select:
     - ✓ "Desktop development with C++"
     - ✓ "C++ Clang tools for Windows"
     - ✓ "CMake tools for Windows" (optional)
5. Install CMake and Python
6. Install optional development tools
7. Verify all installations

### Step 4: Follow Post-Installation Instructions

The script displays detailed next steps, including how to:
- Clone the repository
- View available CMake presets
- Build the project
- Run your first build

---

## Manual Installation

If you prefer to install components manually, follow this sequence:

### 1. Git

```powershell
winget install --id Git.Git --exact
```

Verify:
```powershell
git --version
```

### 2. Visual Studio 2022 Community

**Command Line Installation:**
```powershell
winget install --id Microsoft.VisualStudio.2022.Community --exact
```

**Manual Installation (Recommended):**
1. Visit: https://visualstudio.microsoft.com/vs/community/
2. Click "Download"
3. Run the installer
4. Select workload: "Desktop development with C++"
5. In "Individual components", search for and select:
   - "C++ Clang tools for Windows"
   - "CMake tools for Windows"
6. Click "Install"

**Verify:**
```powershell
cl
```

If successful, shows: `Microsoft (R) C/C++ Optimizing Compiler`

### 3. CMake

```powershell
winget install --id Kitware.CMake --exact
```

Verify:
```powershell
cmake --version
```

Required: CMake 3.25+

### 4. Python 3

```powershell
winget install --id Python.Python.3.12 --exact
```

During installation, **ensure "Add Python to PATH" is checked**.

Verify:
```powershell
python --version
```

Should show: Python 3.12.x or higher

### 5. Optional Tools

#### Ninja (Faster Builds)
```powershell
winget install --id Ninja-build.Ninja --exact
```

Verify:
```powershell
ninja --version
```

#### Visual Studio Code
```powershell
winget install --id Microsoft.VisualStudioCode --exact
```

#### TortoiseGit (Git GUI)
```powershell
winget install --id TortoiseGit.TortoiseGit --exact
```

---

## Building the Project

### Method 1: Command Line (Recommended for Development)

**Clone the repository:**
```powershell
git clone https://github.com/dosbox-staging/dosbox-staging.git
cd dosbox-staging
```

**List available presets:**
```powershell
cmake --list-presets
```

**Build with CMake:**
```powershell
# Debug build (with debugging symbols, slower but better for development)
cmake --preset debug-windows
cmake --build --preset debug-windows

# Release build (optimized, faster)
cmake --preset release-windows
cmake --build --preset release-windows
```

**With verbose output:**
```powershell
cmake --build --preset debug-windows -- -v
```

**Build documentation (optional):**
```powershell
cmake --preset debug-windows-manual
cmake --build --preset debug-windows-manual
```

### Method 2: Visual Studio IDE

1. **Open Visual Studio 2022**
2. Select "Open a local folder"
3. Navigate to the `dosbox-staging` directory
4. Wait for CMake project setup to complete
5. In the toolbar, select a preset:
   - `debug-windows` - for development
   - `release-windows` - for optimized builds
6. Press `F7` or select **Build → Build All**

### Method 3: Visual Studio with CMake Console

1. **Open: Tools → Command Line → Developer Command Prompt**
2. **Navigate to project:**
   ```powershell
   cd path\to\dosbox-staging
   ```
3. **Configure and build:**
   ```powershell
   cmake --preset debug-windows
   cmake --build --preset debug-windows
   ```

### Build Output

- **Debug build:** `build/debug-windows/dosbox.exe`
- **Release build:** `build/release-windows/dosbox.exe`
- **Artifacts:** Check `build/debug-windows/` directory

### First Build Considerations

- **Duration:** 15-45 minutes (depending on your system and SSD speed)
- **vcpkg:** Will download and compile all dependencies automatically
- **Subsequent Builds:** Much faster (usually 2-10 minutes)
- **Disk Space:** May use 15+ GB during build process

### Environment Variables

The setup script doesn't require manual environment variable configuration because:
- **winget** adds tools to the system PATH automatically
- **Visual Studio** provides its own compiler environment
- **CMake** locates all dependencies via vcpkg automatically

---

## Troubleshooting

### Problem: "cmake: command not found"

**Solution:**
```powershell
# Verify installation
winget list cmake

# If not installed
winget install --id Kitware.CMake --exact

# Restart PowerShell/cmd after installation
```

### Problem: "cl: command not found" (C++ compiler)

**Solution:**
1. Reinstall Visual Studio C++ toolset:
   ```powershell
   winget install --id Microsoft.VisualStudio.2022.Community --exact
   ```
2. Or use Visual Studio Installer to repair installation
3. Restart your terminal after installation

### Problem: vcpkg builds fail during CMake configure

**Possible Causes:**
- Disk space (requires 15+ GB free)
- Antivirus interfering (temporarily disable or add exception)
- Network issues (retry with `cmake --preset debug-windows` again)

**Solution:**
```powershell
# Remove corrupted build directory
Remove-Item -Recurse -Force build/

# Clean CMake cache
cmake --fresh --preset debug-windows

# Rebuild
cmake --build --preset debug-windows
```

### Problem: "The system cannot find the path specified" during build

**Solution:**
1. Ensure you're in the project root directory:
   ```powershell
   cd path\to\dosbox-staging
   pwd  # Should show the correct path
   ```
2. Close and reopen PowerShell
3. Try again

### Problem: Script execution denied

**Solution:**
```powershell
# Use bypass for one-time execution
powershell -ExecutionPolicy Bypass -File setup-dev-env-windows.ps1

# Or set permanent policy (not recommended)
Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope CurrentUser
```

### Problem: Administrator elevation fails

**Solution 1 - Manual Elevation:**
1. Right-click PowerShell
2. Select "Run as Administrator"
3. Run the script:
   ```powershell
   powershell -ExecutionPolicy Bypass -File setup-dev-env-windows.ps1
   ```

**Solution 2 - Create Scheduled Task:**
```powershell
# Run as administrator
$trigger = New-ScheduledTaskTrigger -AtLogon
$action = New-ScheduledTaskAction -Execute 'powershell' `
  -Argument "-ExecutionPolicy Bypass -File setup-dev-env-windows.ps1"
Register-ScheduledTask -Trigger $trigger -Action $action -TaskName "Setup DevEnv"
```

### Problem: winget not found or not working

**Solution:**
1. Ensure Windows 11 (Build 22000+)
2. Update via Microsoft Store:
   - Open Microsoft Store
   - Search "App Installer"
   - Click "Update"
3. Or download from GitHub:
   - https://github.com/microsoft/winget-cli/releases

### Problem: "Python not found" during documentation build

**Solution:**
```powershell
# Reinstall Python and ensure PATH is updated
winget install --id Python.Python.3.12 --exact

# Verify Python is on PATH
python --version

# If not working, restart your terminal
```

### Problem: Antivirus blocks installation

**Solution:**
1. Temporarily disable antivirus during setup
2. Or add these to antivirus whitelist:
   - `C:\Program Files\CMake\`
   - `C:\Program Files (x86)\Git\`
   - `C:\Program Files\Microsoft Visual Studio\`
   - Your project's `build/` directory

---

## Next Steps

### After Successfully Building

1. **Run the built executable:**
   ```powershell
   .\build\debug-windows\dosbox.exe --help
   ```

2. **Read the Contributing Guide:**
   - [docs/CONTRIBUTING.md](docs/CONTRIBUTING.md)
   - https://www.dosbox-staging.org/contribute/

3. **Review Code Style:**
   - [.claude/rules/code-style.md](.claude/rules/code-style.md)

4. **Understand Commit Format:**
   - [.claude/rules/commits.md](.claude/rules/commits.md)

### Useful Commands for Development

```powershell
# Clean build (removes all build artifacts)
Remove-Item -Recurse -Force build/

# Rebuild only (faster than clean)
cmake --build --preset debug-windows --target clean
cmake --build --preset debug-windows

# Run tests
cmake --build --preset debug-windows --target run_tests

# List all available targets
cmake --build --preset debug-windows --target help
```

### Essential Resources

- **Project Website:** https://www.dosbox-staging.org/
- **GitHub Repository:** https://github.com/dosbox-staging/dosbox-staging
- **Contributing:** https://www.dosbox-staging.org/contribute/
- **Issues:** https://github.com/dosbox-staging/dosbox-staging/issues
- **Discussions:** https://github.com/dosbox-staging/dosbox-staging/discussions
- **Discord Chat:** [Link in README.md](README.md)

### Tips for Success

1. **Join the Community:** Check out Discord or discussions for questions
2. **Start Small:** Look for "good first issue" labels on GitHub
3. **Read Tests:** Existing tests show how code should behave
4. **Use Git Branches:** Keep your changes organized
5. **Request Reviews Early:** Feedback helps improve contributions
6. **Build Frequently:** Catch issues early with regular builds

---

## Advanced Configuration

### Using Ninja for Faster Builds

If you installed Ninja:

```powershell
# Configure with Ninja generator
cmake --preset debug-windows -G Ninja
cmake --build --preset debug-windows
```

Builds are typically 2-3x faster with Ninja.

### Using ccache for Faster Incremental Builds

ccache is automatically detected and used if available. To install:

```powershell
# Note: ccache is primarily Linux/macOS; Windows alternatives exist
# CMake automatically uses any available caching system
```

### Custom CMake Options

```powershell
# Build with documentation
cmake --preset debug-windows -DOPT_DOCUMENTATION=ON

# Build with specific flags
cmake --preset debug-windows -DCMAKE_BUILD_TYPE=RelWithDebInfo
```

---

## Getting Help

If you encounter issues:

1. **Check this guide** - Most common issues are documented above
2. **Search GitHub Issues:** https://github.com/dosbox-staging/dosbox-staging/issues
3. **Ask in Discussions:** https://github.com/dosbox-staging/dosbox-staging/discussions
4. **Join Discord:** Link in [README.md](README.md)
5. **Check build logs:** Look in `build/debug-windows/CMakeFiles/CMakeError.log`

---

**Last Updated:** 2026-06-10  
**For:** Windows 11 with DOSBox Staging development
