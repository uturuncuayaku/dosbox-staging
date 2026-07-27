> [!NOTE]
> **Historical snapshot - may not match current implementation.** See [append_consolidated_report.md](../append_consolidated_report.md) for the current authoritative specification.

# DOS APPEND Phase 2 Implementation Plan

This plan outlines the architecture for integrating the remaining authentic MS-DOS APPEND features into DOSBox-Staging.

## User Review Required

> [!CAUTION]
> **Find First / Find Next Merging (`/X:ON`)**
> Hooking directory listings across multiple paths requires maintaining an iteration state inside the DOS DTA (Disk Transfer Area), which only has ~21 bytes available. Due to this complexity, I propose we implement `/X:ON` for `EXEC` (running programs) first, and defer the `Find First/Next` (directory merging) to a Phase 3. Let me know if you agree.

## Proposed Changes

### `src/dos/dos_append.h`
- Add a new function signature: `void SetFlags(bool env, bool path_on, bool exec);`
- Add getters: `bool IsPathOverrideOn();`, `bool IsExecOn();`, `bool IsEnvOn();`

### `src/dos/dos_append.cpp`
- Add anonymous namespace variables: `env_mode`, `path_on_mode` (default true for DOS 4.0+), and `exec_mode`.
- Implement `SetFlags()` to update these variables.
- Update `should_bypass_append(name)` to respect `path_on_mode`.
- **The Sync State Hook:** Update `GetDirList()` (or the start of `ResolveName`) to check `env_mode`. If true, call `DOS_GetFirstShell()->psp->GetEnvironmentValue("APPEND")` to dynamically retrieve the list directly from DOS memory, syncing it instantly with any user `SET` commands.

### `src/dos/programs/append.cpp`
- **Equal Sign Parsing:** Update the trimming loop to strip `=` alongside spaces.
- **Flag Parsing:** 
  - Use `cmd->FindExistRemoveAll()` to detect `/E`, `/X`, `/X:ON`, `/X:OFF`, `/PATH:ON`, and `/PATH:OFF`.
  - Extract the flags but **only** pass the resolved booleans to `dos_append::SetFlags()` at the very end of `APPEND::Run()`.
- **Programmatic /E Injection:** If `/E` is active, instead of calling `dos_append::SetDirList(...)`, directly call `DOS_GetFirstShell()->SetEnv("APPEND", cleaned_paths.c_str())`.

### `src/dos/dos_files.cpp`
- **File Attributes Hook:** 
  - In `DOS_GetFileAttr()` and `DOS_SetFileAttr()`, inject `dos_append::ResolveName(name, resolved_name)` at the start, mirroring the `DOS_OpenFile` hook.
  - If a file is successfully resolved, use the new resolved path to fetch or set attributes.

### `src/dos/dos_execute.cpp`
- **EXEC Hook (`/X` feature):**
  - In `DOS_Execute()`, if `dos_append::IsExecOn()` is true, attempt to resolve the executable name using `dos_append::ResolveName()`. This allows users to execute programs located in APPEND directories.

## Verification Plan

### Automated Tests
We will add new tests to `tests/dos_append_tests.cpp`:
- `ParserEqualSign`: Verify `APPEND=C:\DATA` is correctly parsed as `C:\DATA`.
- `ParserFlags`: Verify the internal states update correctly when passing `/X:ON`, `/PATH:OFF`, etc.
- `HookGetFileAttr`: Verify `DOS_GetFileAttr` returns success for a file in an APPEND directory.
- `PathOverride`: Verify that requesting `A:FILE.TXT` successfully resolves to `C:\APPEND_DIR\FILE.TXT` when `/PATH:ON` is active.

