---
# Created by Antigravity
name: Setup Development Environment
description: Comprehensive Windows 11 Pro environment setup, toolchain requirements, compilation rules, error prevention guide, and test workflows for DOSBox-Staging. Use whenever setting up, configuring, building, or troubleshooting compilation/test failures on Windows.
---

# DOSBox-Staging Windows 11 Pro Development Environment & Build Guide

This skill documents the complete setup, toolchain requirements, CMake preset architecture, and error prevention guide for building and testing **DOSBox-Staging** on Windows 11 Pro.

---

## 1. System Requirements & Toolchain

DOSBox-Staging on Windows 11 Pro requires the **Clang/LLVM toolchain (`clang-cl`)** under Visual Studio 2022. Raw MSVC (`cl.exe`) without Clang will fail to compile due to FPU emulation macro dependencies.

### Required Software
* **OS:** Windows 11 Pro (x64)
* **Compiler:** Visual Studio 2022 Build Tools with `C++ Clang tools for Windows` (`clang-cl`)
* **Build Generator:** CMake (≥ 3.21) + MSBuild / Ninja
* **Dependency Manager:** `vcpkg` (integrated automatically via CMake presets)
* **Scripting:** PowerShell 5.1 / PowerShell 7, Python 3.12 (for documentation build)

---

## 2. Automated Environment Setup

To automatically check, download, and install all required dependencies (Visual Studio Build Tools, Git, CMake, Python 3, vcpkg dependencies):

```powershell
powershell -ExecutionPolicy Bypass -File .agents/skills/setup-dev-env/scripts/setup-dev-env-windows.ps1
```

---

## 3. Canonical Build Workflows

Always use predefined CMake presets (`debug-windows` or `release-windows`). The presets automatically select the `ClangCL` toolchain and set up `vcpkg` dependencies.

### A. Initial Configuration
```powershell
cmake --preset debug-windows
```

### B. Building the Main Executable
```powershell
cmake --build --preset debug-windows
```
*Output executable:* `build\debug-windows\Debug\dosbox_with_debugger.exe` (or `dosbox.exe`)

### C. Building the Test Suite
```powershell
cmake --build --preset debug-windows --target dosbox_tests
```
*Output test binary:* `build\debug-windows\tests\Debug\dosbox_tests.exe`

### D. Running Unit Tests
To run all tests or specific filtered tests:
```powershell
# Run all tests
build\debug-windows\tests\Debug\dosbox_tests.exe

# Run filtered tests (e.g. APPEND subsystem tests)
build\debug-windows\tests\Debug\dosbox_tests.exe --gtest_filter=*Append*
```

---

## 4. Compilation & Shell Execution Error Prevention

The following table summarizes common command and compilation errors encountered during Windows agent sessions and how to prevent them:

| Error Symptom | Cause | Prevention Rule |
| :--- | :--- | :--- |
| **`The token '&&' is not a valid statement separator`** | Running Bash syntax (`cmd1 && cmd2`) in Windows PowerShell. | **Do NOT use `&&` in PowerShell.** Run commands as separate lines or use `;` (e.g. `cmake --build ... ; build\...\dosbox_tests.exe`). |
| **`#error "Requires Clang under Visual Studio"`** or `SDL_qsort redefinition` | Running generic `cmake --build .` or invoking `cl.exe` directly in an unconfigured folder. | **Always use CMake presets** (`--preset debug-windows`), which enforce the `ClangCL` toolchain. |
| **`MSB1009: Project file does not exist. Switch: dos_append_tests.vcxproj`** | Passing invalid target names like `dos_append_tests` or `dos_files_tests` to `cmake --build`. | The unit test target is **`dosbox_tests`** (not named after individual test source files). |
| **CTest `include could not find requested file: ..._include-.cmake`** | Running `ctest` before building the test target, or after build dir modification. | Always run `cmake --build --preset debug-windows --target dosbox_tests` first, then run `dosbox_tests.exe` directly. |
| **Git CRLF warning: `LF will be replaced by CRLF`** | Editing C++ source files with LF line endings on Windows. | Normal Git behavior on Windows. Ensure proper code formatting with `clang-format` if modifying core files. |

---

## 5. Verification Checklist

Before declaring any build or code modification complete:
1. Rebuild the affected targets: `cmake --build --preset debug-windows --target dosbox_tests`
2. Run the target unit tests directly: `build\debug-windows\tests\Debug\dosbox_tests.exe --gtest_filter=<Filter>`
3. Ensure zero compiler errors and 100% test pass rate.
