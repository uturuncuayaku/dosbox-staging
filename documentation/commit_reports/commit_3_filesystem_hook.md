<!-- Created by Antigravity -->
# Commit 3 Report: DOS_OpenFile Filesystem Hook Integration

**Commit Hash:** `e9c8a6096`
**Files Modified:**
- [dos_files.cpp](file:///c:/Users/andtr/Documents/GitHub/dosbox-staging-attempt3/dosbox-staging/src/dos/dos_files.cpp)

---

## 1. Technical Purpose & Architecture

This commit hooks `dos_append::ResolveName()` directly into `DOS_OpenFile()`. `DOS_OpenFile` is the primary entry point in DOSBox Staging for opening existing files on mounted drives (handling INT 21h AH=3Dh open file requests). By placing the hook inside `DOS_OpenFile`, any DOS application attempting to open a file that does not exist in the current working directory will automatically search through the directories configured in `APPEND`.

---

## 2. Key C++ Concepts & Implementation Mechanics

### A. Fallback and Retry Hook Location
Inside `DOS_OpenFile(const char* name, uint8_t flags, uint16_t* handle)`:
1. `DOS_OpenFile` first attempts normal path resolution against the current directory or specified path.
2. If normal file opening fails (file not found), `DOS_OpenFile` checks:
   ```cpp
   std::string resolved;
   if (dos_append::ResolveName(name, resolved)) {
       return DOS_OpenFile(resolved.c_str(), flags, handle);
   }
   ```
3. If `dos_append::ResolveName` finds a matching candidate in one of the `APPEND` search directories, it sets `resolved` to the absolute candidate path (e.g. `C:\DATA\CONFIG.DAT`) and recursively invokes `DOS_OpenFile` with `resolved.c_str()`.

### B. Error Code Preservation
- If `dos_append::ResolveName` returns `false` (meaning the file was not found in any `APPEND` directory or `APPEND` is disabled), `DOS_OpenFile` preserves the original `dos.errorcode` (`DOSERR_FILE_NOT_FOUND`).
- This ensures that callers receive standard DOS error return codes without pollution from internal lookup attempts.

---

## 3. Analysis & Trade-offs

### Pros:
- Single point of integration: Hooking `DOS_OpenFile` automatically covers file opening requests across all DOS application calls without requiring modifications to individual drive handlers (`localDrive`, `isoDrive`, etc.).
- Transparent retry: Re-entering `DOS_OpenFile` with the absolute resolved path ensures all normal file permissions, open flags, and handle allocations occur through standard code paths.

### Cons:
- When `APPEND` is enabled with multiple search directories, failed file opens experience additional directory existence check overhead before returning `DOSERR_FILE_NOT_FOUND`.
- `ResolveName` bypasses paths containing drive letters (`C:`) or directory separators (`\`, `/`), ensuring relative single-name file opens trigger `APPEND` while explicit file paths bypass it.

---

## 4. Cross References
- [append_walkthrough.md](file:///c:/Users/andtr/Documents/GitHub/dosbox-staging-attempt3/dosbox-staging/documentation/append_walkthrough.md) for details on `DOS_OpenFile` integration.
- [append_implementation_glossary.md](file:///c:/Users/andtr/Documents/GitHub/dosbox-staging-attempt3/dosbox-staging/documentation/append_implementation_glossary.md) for error code definitions (`DOSERR_FILE_NOT_FOUND`).
