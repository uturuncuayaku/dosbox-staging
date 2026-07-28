> [!NOTE]
> **Historical snapshot - may not match current implementation.** See [append_consolidated_report.md](../append_consolidated_report.md) for the current authoritative specification.

# DOS APPEND Implementation Analysis

This report outlines the remaining authentic MS-DOS `APPEND` features that have not yet been implemented in DOSBox-Staging, detailing **why** MS-DOS included them and **how** they would need to be built.

## 1. Environment Variable Storage (`/E`)

**Why MS-DOS has it:**
By default, MS-DOS `APPEND` stores its directory list in a private memory block (which you have successfully mirrored in your DOS memory allocation). However, if a user ran `APPEND /E` (which can only be done the first time APPEND is run), MS-DOS stores the directory list in the system's Environment block instead, under the variable name `APPEND`.
This allowed batch files and other programs to easily read the appended paths by checking the `%APPEND%` environment variable, and users could dynamically change the list by typing `SET APPEND=C:\NEWDIR`.

**What needs to happen:**
- Add a boolean flag `env_mode` to `dos_append.cpp`.
- Update `APPEND::Run()` to check for `/E`. If present, instead of syncing the string to the DOS memory block, call DOSBox-Staging's environment API (e.g., `env_manager.Set("APPEND", paths)` or equivalent).
- `dos_append::ResolveName()` will need to fetch the string from the environment variable if `env_mode` is true, rather than using the internal `dir_list`.

---

## 2. Path Override (`/PATH:ON` and `/PATH:OFF`)

**Why MS-DOS has it:**
Originally, `APPEND` only worked on bare filenames (like `README.TXT`). If a program asked for `C:\SUB\README.TXT`, APPEND would ignore it. 
However, starting in MS-DOS 4.0, Microsoft changed the default behavior to `/PATH:ON`. This forced `APPEND` to intercept requests even if a drive letter or directory was specified. If a program asked for `A:README.TXT` and it failed, `APPEND` would chop off the `A:` and search the APPEND list for `README.TXT`.

**What needs to happen:**
- Add a boolean flag `path_on_mode` to `dos_append.cpp`, defaulting to `true` (MS-DOS 4.0 default).
- Parse `/PATH:ON` and `/PATH:OFF` in `APPEND::Run()` to toggle this flag.
- Update `should_bypass_append()`:
  - If `path_on_mode` is **false**, continue the current behavior (return `true` to bypass if a `:` or `\` is present).
  - If `path_on_mode` is **true**, extract the basename from the path and search the APPEND list anyway, ignoring whatever drive/directory the program originally requested.

---

## 3. Additional DOS Kernel API Hooks

**Why MS-DOS has it:**
Currently, you have `DOS_OpenFile` hooked. However, if a program wants to check if a file exists *before* opening it, it typically uses "Get File Attributes" (INT 21h AH=43h) or "Find First File" (INT 21h AH=4Eh). Without hooking these, a program might think a file doesn't exist because `GetAttributes` failed, and it will never even attempt to call `OpenFile`. 
Legacy FCB (File Control Block) programs also use "Get File Size" (AH=23h) to probe files.

**What needs to happen:**
- Locate `DOS_GetFileAttr` and `DOS_SetFileAttr` in DOSBox-Staging (likely in `dos_files.cpp` or `dos.cpp`).
- Inject `dos_append::ResolveName()` at the top of these functions, exactly as you did for `DOS_OpenFile`.
- Locate the FCB get file size API (`DOS_FCBGetFileSize` or INT 21h AH=23h handler) and ensure it also resolves the name through APPEND.

---

## 4. Executable & Directory Searching (`/X` and `/X:ON`)

**Why MS-DOS has it:**
Standard `APPEND` is strictly for **data** files (reading/writing). If you typed a command at the DOS prompt, MS-DOS would only use the `%PATH%` variable to find the executable, and `DIR` would only show files in the current directory.
Using `/X:ON` turns `APPEND` into a super-path. It tells MS-DOS to search the APPEND directories for executables (when typing commands) and forces the `DIR` command (and any program using Find First/Next) to show files merged from across all APPEND directories.

**What needs to happen:**
- **EXEC Hooking:** Intercept INT 21h AH=4Bh (`DOS_Execute`). If `/X:ON` is active, and the executable isn't found in the current directory or the standard `%PATH%`, it must search the APPEND directories.
- **Find First / Find Next Hooking:** This is the most complex feature. If `/X:ON` is active, INT 21h AH=4Eh/4Fh (and FCB 11h/12h) must return merged directory listings. This requires the DOS kernel to remember which APPEND directory it is currently iterating through when returning "Find Next" results, moving to the next APPEND directory when the current one is exhausted.

> [!CAUTION]
> Implementing the `Find First/Next` merging for `/X:ON` is notoriously difficult in emulators because it requires storing complex iteration state inside the DOS DTA (Disk Transfer Area), which only has a few bytes reserved for state. Many emulators skip full `/X:ON` directory merging due to this complexity.

