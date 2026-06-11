# Windows 11 Development Setup - Quick Start Guide

Complete and frictionless development environment setup for DOSBox Staging on Windows 11.

## 📋 What You'll Get

After running the setup, you'll have:
- ✅ Git version control
- ✅ Visual Studio 2022 Community with C++ and Clang compiler
- ✅ CMake build system (3.25+)
- ✅ Python 3 for documentation
- ✅ All C++ dependencies (handled by vcpkg)
- ✅ Optional: Ninja (faster builds), VSCode, TortoiseGit
- ✅ Zero configuration needed - everything automatic!

## 🚀 Quick Start (5 seconds)

**Copy and paste this into PowerShell:**

```powershell
powershell -ExecutionPolicy Bypass -File setup-dev-env-windows.ps1
```

**Or use the batch file (Windows Explorer):**
- Double-click: `setup-dev-env-windows.bat`
- Choose option 1 for full installation

That's it! The script handles everything.

---

## 📚 Documentation Files

| File | Purpose |
|------|---------|
| **[setup-dev-env-windows.ps1](setup-dev-env-windows.ps1)** | Main setup script (automated) |
| **[setup-dev-env-windows.bat](setup-dev-env-windows.bat)** | Batch launcher (double-click friendly) |
| **[WINDOWS_DEV_SETUP.md](WINDOWS_DEV_SETUP.md)** | Detailed setup guide with troubleshooting |
| **[WINDOWS_DEV_CHECKLIST.md](WINDOWS_DEV_CHECKLIST.md)** | Post-installation verification checklist |
| **[diagnose-dev-env-windows.ps1](diagnose-dev-env-windows.ps1)** | Diagnostic tool for troubleshooting |

---

## 🛠️ How to Use This Setup

### Option 1: Fastest (Recommended)

```powershell
# In PowerShell (any directory)
powershell -ExecutionPolicy Bypass -File setup-dev-env-windows.ps1
```

The script will:
1. Request admin privileges
2. Install all dependencies via winget
3. Verify everything works
4. Show you the next steps

### Option 2: Menu-Driven (Visual)

```bash
# Double-click from Windows Explorer:
setup-dev-env-windows.bat
```

Then choose your option:
- `1` = Full installation (recommended)
- `2` = Core only (skip optional tools)
- `3` = View setup guide
- `4` = Exit

### Option 3: Core Only (Minimal)

```powershell
# Skip optional tools
powershell -ExecutionPolicy Bypass -File setup-dev-env-windows.ps1 -SkipOptional
```

### Option 4: Manual Installation

See [WINDOWS_DEV_SETUP.md](WINDOWS_DEV_SETUP.md#manual-installation) for step-by-step manual installation.

---

## ✅ Verify Installation

After setup completes, run this to verify everything:

```powershell
# Check all tools
git --version
cmake --version
python --version
cl          # Shows C++ compiler info

# Should show version numbers for each
```

Or run the diagnostic tool:

```powershell
powershell -ExecutionPolicy Bypass -File diagnose-dev-env-windows.ps1
```

---

## 🏗️ Build the Project

### Quick Build (Command Line)

```powershell
# 1. Clone repository (if not already done)
git clone https://github.com/dosbox-staging/dosbox-staging.git
cd dosbox-staging

# 2. List available presets
cmake --list-presets

# 3. Configure debug build
cmake --preset debug-windows

# 4. Build
cmake --build --preset debug-windows

# Result: build/debug-windows/dosbox.exe
```

### Build Options

```powershell
# Debug build (with debugging symbols)
cmake --preset debug-windows
cmake --build --preset debug-windows

# Release build (optimized)
cmake --preset release-windows
cmake --build --preset release-windows

# Build with documentation
cmake --preset debug-windows-manual
cmake --build --preset debug-windows-manual

# With verbose output
cmake --build --preset debug-windows -- -v

# Force rebuild (clear cache)
cmake --fresh --preset debug-windows
cmake --build --preset debug-windows
```

### Using Visual Studio IDE

1. Open **Visual Studio 2022**
2. Select **"Open a local folder"**
3. Navigate to `dosbox-staging` directory
4. Wait for CMake project to load
5. Choose a preset: `debug-windows` or `release-windows`
6. Press **F7** or **Build → Build All**

---

## 🔍 Troubleshooting

### Script won't run

```powershell
# If you get "execution policy" error:
powershell -ExecutionPolicy Bypass -File setup-dev-env-windows.ps1

# Or set policy permanently (not recommended):
Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope CurrentUser
```

### Tools not found after installation

1. **Close and reopen PowerShell** - Environment variables need to reload
2. **Restart your computer** - If still not working
3. **Run diagnostics:**
   ```powershell
   powershell -ExecutionPolicy Bypass -File diagnose-dev-env-windows.ps1
   ```

### Visual Studio installer missing features

1. Open **Visual Studio Installer**
2. Find **Visual Studio 2022 Community**
3. Click **Modify**
4. Ensure these are checked:
   - ✓ "Desktop development with C++"
   - ✓ "C++ Clang tools for Windows"
   - ✓ "CMake tools for Windows" (optional)
5. Click **Modify**

### Build fails on first attempt

This is normal! The first build downloads and compiles dependencies. **Just run again:**

```powershell
cmake --build --preset debug-windows
```

For more help, see [WINDOWS_DEV_SETUP.md](WINDOWS_DEV_SETUP.md#troubleshooting).

---

## 📖 What Gets Installed

### Required Tools
- **Git** - Version control
- **Visual Studio 2022 Community** - C++ compiler and IDE
- **CMake 3.25+** - Build system
- **Python 3** - Documentation building

### C++ Dependencies (Auto-Installed by vcpkg)
```
asio                 - Networking
gtest                - Testing framework
iir1                 - Digital Signal Processing
libmt32emu           - MT-32 MIDI emulation
libpng               - PNG image support
opusfile             - Opus audio codec
fluidsynth           - MIDI synthesizer
SDL2 & SDL2-image    - Graphics and input
speexdsp             - Audio processing
zlib-ng              - Compression
```

### Optional Tools
- **Ninja** - Faster builds (2-3x speedup)
- **Visual Studio Code** - Code editor
- **TortoiseGit** - Git GUI integration
- **Notepad++** - Text editor

---

## 📝 Contributing

Once your environment is set up:

1. **Read the guides:**
   - [docs/CONTRIBUTING.md](docs/CONTRIBUTING.md) - How to contribute
   - [.claude/rules/code-style.md](.claude/rules/code-style.md) - C++ code style
   - [.claude/rules/commits.md](.claude/rules/commits.md) - Commit format

2. **Configure Git (first time):**
   ```powershell
   git config --global user.name "Your Name"
   git config --global user.email "your.email@example.com"
   ```

3. **Create a branch for your work:**
   ```powershell
   git checkout -b my-feature-name
   ```

4. **Start contributing!**
   - Look for ["good first issue" labels](https://github.com/dosbox-staging/dosbox-staging/labels/good%20first%20issue)
   - Check [discussions](https://github.com/dosbox-staging/dosbox-staging/discussions)
   - Join the [Discord community](README.md)

---

## 🎯 Environment Details

### Automated Paths
The setup script adds these to your PATH automatically:

| Tool | Typical Path |
|------|-------|
| Git | `C:\Program Files\Git\cmd` |
| CMake | `C:\Program Files\CMake\bin` |
| Python | `C:\Users\<You>\AppData\Local\Programs\Python\Python312` |
| MSVC | `C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\<version>\bin\Hostx64\x64` |

### Build Directory Structure
```
dosbox-staging/
├── build/                           # Build outputs
│   ├── debug-windows/              # Debug build
│   │   ├── dosbox.exe
│   │   ├── tests.exe
│   │   └── CMakeFiles/
│   └── release-windows/            # Release build
├── src/                            # Source code
├── tests/                          # Unit tests
├── CMakeLists.txt                  # Build configuration
└── vcpkg.json                      # C++ dependencies
```

---

## ⏱️ Timeline

- **Setup script:** 10-20 minutes
- **First build:** 15-45 minutes (long because vcpkg downloads/builds dependencies)
- **Subsequent builds:** 2-10 minutes
- **With Ninja:** 40-60% faster builds

---

## 💡 Pro Tips

1. **Faster Builds:** Ninja build system
   ```powershell
   cmake --preset debug-windows -G Ninja
   cmake --build --preset debug-windows
   ```

2. **Rebuilding After Git Changes:**
   ```powershell
   # After pulling major changes:
   cmake --fresh --preset debug-windows
   cmake --build --preset debug-windows
   ```

3. **Run Tests:**
   ```powershell
   cmake --build --preset debug-windows --target run_tests
   ```

4. **Clean Build:**
   ```powershell
   Remove-Item build -Recurse -Force  # Windows PowerShell
   cmake --preset debug-windows
   cmake --build --preset debug-windows
   ```

---

## 🆘 Getting Help

1. **Check documentation:**
   - This file (you're here!)
   - [WINDOWS_DEV_SETUP.md](WINDOWS_DEV_SETUP.md) - Detailed setup
   - [WINDOWS_DEV_CHECKLIST.md](WINDOWS_DEV_CHECKLIST.md) - Verification

2. **Run diagnostics:**
   ```powershell
   powershell -ExecutionPolicy Bypass -File diagnose-dev-env-windows.ps1
   ```

3. **Search for solutions:**
   - [GitHub Issues](https://github.com/dosbox-staging/dosbox-staging/issues)
   - [GitHub Discussions](https://github.com/dosbox-staging/dosbox-staging/discussions)

4. **Ask the community:**
   - Discord (see [README.md](README.md))
   - GitHub Discussions
   - Project website: https://www.dosbox-staging.org/

---

## 📋 Checklist for New Contributors

- [ ] Windows 11 Build 22000+
- [ ] Administrator privileges available
- [ ] 20+ GB free disk space
- [ ] Internet connection stable
- [ ] Run setup script
- [ ] Verify all tools installed
- [ ] Build project successfully
- [ ] Read [CONTRIBUTING.md](docs/CONTRIBUTING.md)
- [ ] Read code style guide
- [ ] Configure Git with name/email
- [ ] Ready to contribute!

---

## 🔗 Important Links

- **Website:** https://www.dosbox-staging.org/
- **Repository:** https://github.com/dosbox-staging/dosbox-staging
- **Contributing:** https://www.dosbox-staging.org/contribute/
- **Issues:** https://github.com/dosbox-staging/dosbox-staging/issues
- **Discussions:** https://github.com/dosbox-staging/dosbox-staging/discussions
- **Discord:** See [README.md](README.md)

---

## ✨ Features

- ✅ **Fully Automated** - No manual configuration needed
- ✅ **Winget-Based** - Uses Microsoft's official package manager
- ✅ **GitHub Releases** - Direct downloads when appropriate
- ✅ **Elevation Handling** - Automatically requests admin when needed
- ✅ **Error Checking** - Verifies each installation step
- ✅ **Optional Tools** - Includes productivity enhancements
- ✅ **Diagnostics** - Troubleshoot issues easily
- ✅ **Comprehensive Docs** - Everything explained clearly

---

**Last Updated:** 2026-06-10  
**For:** Windows 11 DOSBox Staging Development

Ready to start? Run: `powershell -ExecutionPolicy Bypass -File setup-dev-env-windows.ps1`
