> [!NOTE]
> **Historical snapshot - may not match current implementation.** See [append_consolidated_report.md](../append_consolidated_report.md) for the current authoritative specification.

# APPEND Implementation Walkthrough

## Summary

Implemented the native DOS APPEND command for DOSBox-Staging â€” directory-list management and file-open retry behavior. No TSR, no guest memory, no /X or /E flags.

## New Files

### [dos_append.h](file:///c:/Users/andtr/Documents/GitHub/dosbox-staging-attempt3/dosbox-staging/src/dos/dos_append.h)

Header for the `dos_append` namespace. Exposes `Init`, `IsEnabled`, `IsResolving`, `ResolveName`, `SetDirList`, `GetDirList`, and `MultiplexHandler`.

### [dos_append.cpp](file:///c:/Users/andtr/Documents/GitHub/dosbox-staging-attempt3/dosbox-staging/src/dos/dos_append.cpp)

State module encapsulating the active `APPEND` environment.

**Key State Variables:**
- `dir_list`: A semicolon-delimited `std::string` of directories to search.
- `enabled`: A boolean optimization flag indicating if `dir_list` is populated.
- `currently_resolving`: 
  > **Recursion Guard (Reentrancy Lock)**
  > This is a critical boolean flag preventing infinite recursion during path resolution. 
  > When `ResolveName` tests a candidate path, it calls native `DOS_FileExists()`. Native DOS file access routines inherently trigger `DOS_OpenFile` hooks, which in turn call back into `ResolveName` to check if `APPEND` is needed. 
  > Without `currently_resolving`, this cycle would repeat infinitely, causing a stack overflow. By asserting `currently_resolving = true` at the entry of the resolution logic, any recursive hooks instantly bypass `APPEND`, safely terminating the loop.

### 3. File Operations Hook (`src/dos/dos_files.cpp`)
- Intercepted `DOS_OpenFile` relative path lookups to search through APPEND directories as a fallback.
- Explicitly bypassed APPEND for paths containing drive letters (`:`) or directory separators (`\` or `/`).
- Validated that the hook respects `dos_append::IsResolving()` to prevent infinite loops (e.g. if the user mounts a folder that redirects calls back into DOS).
- Ensured original DOS error codes (e.g. `DOSERR_FILE_NOT_FOUND`) are properly restored when all APPEND paths fail.

### 4. Multiplex Interrupts Handler (`dos_append::MultiplexHandler`)
- Registered `INT 2Fh, AH=B7h` in `src/dos/dos.cpp`.
- Handled the APPEND Installation Check (`AL=00h`), returning `AL=FFh` to indicate the TSR is present.

### 5. Exhaustive Unit and Behavioral Testing (`tests/dos_append_tests.cpp`)
Implemented an exhaustive 16-test suite using Google Test, verifying:
- State Management (enabling, disabling, clearing)
- Parser normalizations (trailing slashes, deduplication, uppercase mapping)
- ResolveName boundary tests (preventing bypassing absolute/directory-containing paths)
- DOS_OpenFile behavioral flows and original DOS error code preservation
- Multiplex check and ignored subfunctions
- **Fixes Discovered by Tests:** Tests uncovered a potential heap corruption issue (due to `CommandLine` needing heap allocation in the mock test shell), and an edge case where absolute paths were being incorrectly extracted and processed by APPEND. Both issues were patched immediately.

## Verification
- Local build compiled successfully.
- Ran `dosbox_tests.exe --gtest_filter=DOS_AppendTest.*` achieving 16/16 test passes.
- Adhered strictly to DOSBox Staging's commit style (isolated, compiling commits using `build:`, `style:`, etc. where applicable, and no prefixes for functional changes).

## Logging and Tracing

We developed an extensive, zero-overhead execution narrative using `LOG_DEBUG` targeting the `debug-windows` build preset.

> [!TIP]
> **RAII Tracing**: We introduced `AppendTraceScope` in `dos_append.h`, a debug-only RAII scope helper. It automatically manages call-depth indentation and injects ASCII function entry/exit banners without duplicating logic across the subsystem!

By separating **state** (what data do we have?) from **narrative** (why did we make this decision?), the tracing provides an instantly readable execution flow. It covers every decision branch, validation step, and iterative parsing loop. 

**Log Example (`tests/logging/log.txt`):**
```text
======================================================================
| ResolveName()             | Find file in APPEND paths                |
======================================================================

  Requested file       : "README.TXT"
  --------------------------------------------------
  Iteration            : 1
  Parser offset        : (auto)
  Remaining input      : ""
  --------------------------------------------------
  Token extracted      : "C:\APPEND_DIR"
  Trimmed              : "C:\APPEND_DIR"
  Candidate            : "C:\APPEND_DIR\README.TXT"
  Exists               : yes
  Decision             : Stop search
  Reason               : First matching file found

  ============================================================
  ResolveName() Summary
  ============================================================
  Input filename       : "README.TXT"
  Directories searched : 1
  Candidates generated : 1
  Matches found        : 1
  Resolved path        : "C:\APPEND_DIR\README.TXT"
  Result               : true
----------------------------------------------------------------------
Leaving ResolveName()
----------------------------------------------------------------------
```

## Compiling with Tracing Enabled

Since `LOG_DEBUG` and `AppendTraceScope` are strictly wrapped in `#ifndef NDEBUG`, they have **zero** impact on release builds. To enable and view these messages, you must compile DOSBox-Staging with the internal debugger enabled.

### Windows (MSVC / Clang-cl via VCPKG)
If you're using the standard setup with `vcpkg`, you can configure and build using the `debug-windows` preset:

```powershell
# 1. Ensure your Visual Studio dev environment and VCPKG_ROOT are set
Import-VSEnvironment
$env:VCPKG_ROOT = "C:\path\to\vcpkg"

# 2. Configure the project with the debug preset (Run once)
cmake --preset debug-windows -DOPT_DEBUGGER=ON

# 3. Build the dosbox_tests target (Run after making code changes)
cmake --build --preset debug-windows --target dosbox_tests

# 4. Run the newly compiled tests, routing all stdout/stderr directly into a log file
cmd.exe /c "build\debug-windows\tests\Debug\dosbox_tests.exe --gtest_filter=DOS_AppendTest.* > tests\logging\log.txt 2>&1"
```

> [!NOTE]
> The final command uses `cmd.exe /c` to run the tests and redirect standard output (`>`) and standard error (`2>&1`) to `tests\logging\log.txt`. This bypasses a known PowerShell issue where native error streams can occasionally be mangled into red `RemoteException` errors.

## Logging Configuration Notes
During testing and development, you may notice the `[log]` section (e.g. `vga = on`, `cpu = on`, `logfile = ...`) in configuration files like `tests/files/dosbox-staging-tests.conf`. 
- **Standard Builds**: In a standard release build (without `OPT_DEBUGGER`), the detailed component logging macros (like `LOG(LOG_VGA, ...)`) evaluate to an empty inline struct (`src/misc/logging.h`). This means the compiler strips all detailed logs out of the final executable to ensure zero runtime overhead. No matter what the `.conf` file says, these component logs will not output to `stdout` or a file. (Generic application events logged via `LOG_INFO` or `LOG_MSG` using Loguru will still print).
- **Debugger Builds**: To actually trace the deep hardware, CPU, and filesystem logs, DOSBox must be compiled with the Internal Debugger enabled (`C_DEBUGGER`). In this build (typically `dosbox_with_debugger.exe`), the internal debugger parses the `[log]` configuration block and routes the massive stream of output either to the built-in debugger console or to the file specified in the `logfile=` parameter.h](file:///c:/Users/andtr/Documents/GitHub/dosbox-staging-attempt3/dosbox-staging/src/dos/programs/append.h)

Command class inheriting from `Program`. Uses `HELP_Category::File`, follows the SETVER pattern with `AddMessages()`.

### [append.cpp](file:///c:/Users/andtr/Documents/GitHub/dosbox-staging-attempt3/dosbox-staging/src/dos/programs/append.cpp)

Command implementation:
- `APPEND C:\DATA;D:\MORE` â†’ sets the directory list
- `APPEND` (no args) â†’ prints `APPEND=<list>` or "No APPEND directories"
- `APPEND ;` â†’ clears the list
- `APPEND /?` â†’ shows help text

---

## Modified Files

### [dos_files.cpp](file:///c:/Users/andtr/Documents/GitHub/dosbox-staging-attempt3/dosbox-staging/src/dos/dos_files.cpp)

Hook at [line 893](file:///c:/Users/andtr/Documents/GitHub/dosbox-staging-attempt3/dosbox-staging/src/dos/dos_files.cpp#L893-L900) â€” after `DOS_OpenFile()` fails to open a file on the drive (but before the error-type classification), checks if APPEND is enabled and not already resolving, then calls `ResolveName()`. On success, recursively calls `DOS_OpenFile()` with the resolved path. The recursion guard prevents infinite loops.

### [dos_programs.cpp](file:///c:/Users/andtr/Documents/GitHub/dosbox-staging-attempt3/dosbox-staging/src/dos/dos_programs.cpp)

Added `#include "programs/append.h"` and `PROGRAMS_MakeFile("APPEND.EXE", ...)` in alphabetical order.

### [CMakeLists.txt](file:///c:/Users/andtr/Documents/GitHub/dosbox-staging-attempt3/dosbox-staging/src/dos/CMakeLists.txt)

Added `dos_append.cpp` to `dosboxcommon` target and `programs/append.cpp` to the programs list.

### [dos.cpp](file:///c:/Users/andtr/Documents/GitHub/dosbox-staging-attempt3/dosbox-staging/src/dos/dos.cpp)

Added `#include "dos/dos_append.h"` and `dos_append::Init()` call at [line 1844](file:///c:/Users/andtr/Documents/GitHub/dosbox-staging-attempt3/dosbox-staging/src/dos/dos.cpp#L1844), right before the Windows INT 2F handler registration.

---

## Design Decisions

| Decision | Rationale |
|---|---|
| `DOS_FileExists()` for probing | Avoids side effects (file handle allocation) during the search loop. The actual open happens through the recursive `DOS_OpenFile()` call |
| `std::string& out_path` (not `char*`) | Code-style rules say no C string functions |
| Hook placement after `FileOpen` null | Only genuine file-not-found triggers APPEND â€” device opens, lock failures, and access code errors are unaffected |
| `HELP_Category::File` | Per user request |

## Files NOT Modified

Per spec: `dos_execute.cpp`, `dos_find.cpp`, `dos_tables.cpp` â€” untouched.

## Build Verification

```
cmake --build --preset release-windows
```

- **Result**: âœ… Success
- **Errors**: 0
- **New warnings**: 0 (only pre-existing vendor library warnings)
- `dosboxcommon.lib`, `dosbox.exe`, `dosbox_tests.exe` all built cleanly

