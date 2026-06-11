# DOSBox Staging - Windows 11 Developer Checklist

Quick reference checklist to verify your development environment is properly configured.

## ✓ Pre-Installation

- [ ] Running Windows 11 Build 22000 or later
  - Verify: Press `Win + R`, type `winver`, press Enter
  - Should show: "Windows 11" with "Build 22000" or higher

- [ ] Administrator privileges available
  - Verify: Right-click PowerShell → "Run as Administrator"
  - If successful, PowerShell opens without error

- [ ] Internet connection active
  - Verify: Open https://github.com in your browser
  - Required for downloading dependencies

- [ ] At least 20 GB free disk space
  - Verify: Right-click drive → Properties
  - Needed for Visual Studio, dependencies, and build artifacts

## ✓ Installation Phase

- [ ] Setup script execution
  - Ran: `powershell -ExecutionPolicy Bypass -File setup-dev-env-windows.ps1`
  - Status: Script completed without errors

- [ ] Visual Studio 2022 Community installed
  - Verify: `cl` in PowerShell
  - Should show: "Microsoft (R) C/C++ Optimizing Compiler"
  - Required selections made:
    - [ ] Desktop development with C++
    - [ ] C++ Clang tools for Windows
    - [ ] CMake tools for Windows (optional)

- [ ] Git installed
  - Verify: `git --version` in PowerShell
  - Should show: "git version X.X.X or higher"

- [ ] CMake installed (version 3.25+)
  - Verify: `cmake --version` in PowerShell
  - Should show: "cmake version 3.25" or higher

- [ ] Python installed (version 3.10+)
  - Verify: `python --version` in PowerShell
  - Should show: "Python 3.X.X"
  - Important: Added to PATH during installation

## ✓ Optional Tools

- [ ] Ninja installed (for faster builds)
  - Verify: `ninja --version` in PowerShell
  - Recommended for development

- [ ] Visual Studio Code installed
  - Verify: `code --version` in PowerShell
  - Useful for editing code

- [ ] TortoiseGit installed
  - Verify: Right-click folder → "Git" menu appears
  - Provides Git GUI operations

## ✓ Post-Installation Verification

- [ ] PowerShell recognizes all tools
  - [ ] `git --version` works
  - [ ] `cmake --version` works
  - [ ] `python --version` works
  - [ ] `cl` shows compiler info

- [ ] Environment variables set correctly
  - Verify in new PowerShell: `$env:PATH`
  - Should include paths to Git, CMake, Python
  - If not, restart your computer

## ✓ Repository Setup

- [ ] Repository cloned
  - Command: `git clone https://github.com/dosbox-staging/dosbox-staging.git`
  - Location: Any directory you prefer

- [ ] Project root directory correct
  - Verify: Files present:
    - [ ] `CMakeLists.txt`
    - [ ] `vcpkg.json`
    - [ ] `README.md`
    - [ ] `setup-dev-env-windows.ps1`

- [ ] No conflicting build directories
  - Verify: Run `rm -Recurse -Force build` to clean old builds
  - (Only needed if you previously attempted builds)

## ✓ First Build Test

- [ ] List available presets
  - Command: `cmake --list-presets`
  - Should show: `debug-windows`, `release-windows`, etc.

- [ ] Debug build configuration
  - Command: `cmake --preset debug-windows`
  - Should complete without errors
  - Time: Usually 2-10 minutes
  - Creates: `build/debug-windows` directory

- [ ] Debug build compilation
  - Command: `cmake --build --preset debug-windows`
  - Time: First build 15-45 minutes (vcpkg downloads dependencies)
  - Should complete with: `Build files have been written to...`
  - Output executable: `build/debug-windows/dosbox.exe`

- [ ] Executable verification
  - Command: `.\build\debug-windows\dosbox.exe --version`
  - Should show: DOSBox Staging version info
  - Confirms: Build successful and runnable

## ✓ Development Environment

- [ ] Read contributing guidelines
  - [ ] Opened: `docs/CONTRIBUTING.md`
  - [ ] Understood: Contribution workflow

- [ ] Review code style
  - [ ] Opened: `.claude/rules/code-style.md`
  - [ ] Understood: C++ coding conventions

- [ ] Understand commit format
  - [ ] Opened: `.claude/rules/commits.md`
  - [ ] Understood: Commit message format

- [ ] Git configured
  - Verify: `git config --list`
  - Should include:
    - [ ] `user.name` set
    - [ ] `user.email` set
  - Set if missing:
    ```powershell
    git config --global user.name "Your Name"
    git config --global user.email "your.email@example.com"
    ```

- [ ] Git account authenticated
  - [ ] GitHub account created (if not already)
  - [ ] SSH keys configured (or use HTTPS with token)
  - Test: `git ls-remote https://github.com/dosbox-staging/dosbox-staging.git`

## ✓ IDE Setup (Choose One)

### Visual Studio 2022 IDE Option
- [ ] Visual Studio 2022 opened
- [ ] Project imported: File → Open folder → Select dosbox-staging
- [ ] CMake project loaded successfully
- [ ] Preset selected: `debug-windows` or `release-windows`
- [ ] First build successful: Build → Build All (F7)

### Visual Studio Code Option
- [ ] Visual Studio Code opened
- [ ] CMake extension installed
- [ ] Project folder opened: `dosbox-staging`
- [ ] CMake project detected and loaded
- [ ] Preset selected: `debug-windows`
- [ ] C++ IntelliSense working

### Command Line Option
- [ ] PowerShell/Command Prompt working
- [ ] In correct directory: `dosbox-staging` folder
- [ ] CMake commands work:
  - [ ] `cmake --preset debug-windows`
  - [ ] `cmake --build --preset debug-windows`

## ✓ Building and Testing

- [ ] Release build successful (optional test)
  - Command: `cmake --preset release-windows`
  - Then: `cmake --build --preset release-windows`

- [ ] Tests executable location known
  - Path: `build/debug-windows/tests.exe` or similar
  - Can run: `.\build\debug-windows\run_tests`

- [ ] Documentation build (optional)
  - Command: `cmake --preset debug-windows-manual -DOPT_DOCUMENTATION=ON`
  - Output: `build/debug-windows/Resources/docs/`

## ✓ Troubleshooting Status

- [ ] All tools verified working
- [ ] First build completed successfully
- [ ] Executable runs and shows version info
- [ ] Ready to start contributing!

---

## Common Verification Commands

Run these in PowerShell to verify your setup:

```powershell
# Check all essential tools
Write-Host "Checking installation..." -ForegroundColor Cyan
Write-Host "Git: $(git --version)"
Write-Host "CMake: $(cmake --version | Select-Object -First 1)"
Write-Host "Python: $(python --version)"
Write-Host "C++ Compiler: $(cl 2>&1 | Select-Object -First 1)"

# Navigate to project
cd C:\path\to\dosbox-staging

# Check presets
cmake --list-presets

# Configure build
cmake --preset debug-windows

# Start build
cmake --build --preset debug-windows

# Run executable
.\build\debug-windows\dosbox.exe --version
```

---

## Status Indicators

| Symbol | Status |
|--------|--------|
| ✓ Checked | Component installed and working |
| ✗ Unchecked | Component not yet installed/verified |
| ! Issue | Component has a problem (see troubleshooting) |

---

## If You Get Stuck

1. **Check this checklist** - Compare your status with items listed
2. **Review WINDOWS_DEV_SETUP.md** - Troubleshooting section has solutions
3. **Search GitHub Issues** - Your problem may already be solved
4. **Ask on Discord or Discussions** - Community can help

---

**Last Updated:** 2026-06-10  
**For:** Windows 11 DOSBox Staging Development
