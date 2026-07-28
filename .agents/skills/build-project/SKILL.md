---
name: Build DOSBox-Staging
description: Instructions for compiling and testing DOSBox-Staging on Windows to avoid compiler toolchain errors.
---

# Building DOSBox-Staging on Windows

When compiling DOSBox-Staging or running tests in the background on Windows, you **MUST NOT** run generic `ninja` or `cmake --build .` commands in unconfigured build directories. The DOSBox-Staging project (specifically the FPU emulation) strictly requires the Clang compiler (`clang-cl`), and failing to configure the toolchain properly will result in spurious errors such as `SDL_qsort redefinition` and `#error "Requires Clang under Visual Studio"`.

## Proper Compilation Workflow

To compile the codebase and run tests properly as an agent, always use the project's predefined CMake presets which automatically load the `ClangCL` toolchain:

1. **Configure the Project** (if not already configured):
   ```bash
   cmake --preset debug-windows
   ```
2. **Build the Project**:
   ```bash
   cmake --build --preset debug-windows
   ```
3. **Run Tests**:
   Do not run tests in generic folders. Run them strictly from the preset build folder:
   ```bash
   cd build/debug-windows
   ctest --output-on-failure
   ```

Do not manually invoke `cl.exe` or `ninja` outside of these preset configurations.
