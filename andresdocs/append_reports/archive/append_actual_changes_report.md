> [!NOTE]
> **Historical snapshot - may not match current implementation.** See [append_consolidated_report.md](../append_consolidated_report.md) for the current authoritative specification.

# Proposed Code Changes for APPEND Phase 2

This report strips away the historical MS-DOS explanations and focuses purely on the actual C++ changes we will write in the DOSBox-Staging codebase. We are completely ignoring the `/E` Environment block and `Find First/Next` wildcard merging.

## 1. Updating the Core APPEND State
**Files:** `src/dos/dos_append.h` and `src/dos/dos_append.cpp`

We need to add state variables so the backend knows what flags the user passed.
*   **Changes:**
    *   Add two boolean variables to the anonymous namespace in `dos_append.cpp`: `path_override` (default true) and `exec_search` (default false).
    *   Create a new setter: `void SetFlags(bool path_on, bool exec_on);`
    *   Create a getter: `bool IsExecEnabled();`
*   **Path Override Logic:**
    *   Modify the existing `should_bypass_append()` function. Currently, it always returns `true` (skips APPEND) if it sees a drive letter (`:`) or directory slash (`\`). We will change this so that if `path_override` is `true`, it ignores the slashes/colons and searches the APPEND list anyway.

## 2. Fixing the Parser & Flags
**File:** `src/dos/programs/append.cpp`

We need to make the command line program parse `=` and detect the `/X` and `/PATH` switches.
*   **Changes:**
    *   **Leading Equal Sign:** Update the whitespace trimming loop. Instead of just `while (args.front() == ' ')`, change it to `while (args.front() == ' ' || args.front() == '=')`. This explicitly fixes the `APPEND=C:\DATA` syntax at the very start of the command line.
    *   **Strict Token Validation & Safe Rejection:** During the semicolon tokenization loop, we will strictly leave the `token.find_first_not_of(" \"")` as it is (without adding an equals sign). 
    *(Note on C++ Syntax: The string `" \""` contains only a space and a double-quote. The backslash is just an escape character, so this function translates to: "Find the first character that is NOT a space, and is NOT a double-quotation mark".)*
    As you pointed out, if a user injects extra equals signs into the path (e.g., `=C:\Data2`), we *want* it to fail! By passing that un-stripped string directly to `DOS_MakeName`, it will instantly recognize the structural flaw and reject it:
        ```cpp
        // If a malicious or structurally invalid path is provided, DOS_MakeName returns false.
        if (!DOS_MakeName(token.c_str(), fullname, &drive) ||
            !Drives[drive] ||
            !Drives[drive]->TestDir(fullname)) {
                
            WriteOut(MSG_Get("PROGRAM_APPEND_INVALID_PATH"));
            return; // Aborts APPEND::Run() immediately!
        }
        ```
        Because this `return;` instantly exits the function, our `SetFlags` call at the end of the file is never reached, perfectly protecting the DOS state.
    *   **Flag Detection:** At the top of `APPEND::Run()`, capture the flags:
        ```cpp
        bool x_on = cmd->FindExistRemoveAll("/X:ON") || cmd->FindExistRemoveAll("/X");
        bool x_off = cmd->FindExistRemoveAll("/X:OFF");
        bool path_on = cmd->FindExistRemoveAll("/PATH:ON");
        bool path_off = cmd->FindExistRemoveAll("/PATH:OFF");
        ```
    *   **Safe Commit:** At the very end of `APPEND::Run()`, *after* the `DOS_MakeName` loop successfully verifies all the appended directories, call `dos_append::SetFlags(...)`. This ensures a syntax error aborts the command without changing the state.

## 3. Hooking File Attributes (The Core of `/X:ON`)
**File:** `src/dos/dos_files.cpp`

When `COMMAND.COM` tries to run a batch file or program, it usually calls `DOS_GetFileAttr` first to see if it exists.
*   **Changes:**
    *   Inside `DOS_GetFileAttr(const char* name, ...)` and `DOS_SetFileAttr(...)`, add the same hook you already successfully used for `DOS_OpenFile`:
        ```cpp
        std::string resolved;
        if (dos_append::ResolveName(name, resolved)) {
            // Use 'resolved' instead of 'name' for the rest of the function
        }
        ```
    *   This forces any program checking for a file's existence to receive a "Yes, it exists!" if the file is in an APPEND directory.

## 4. Hooking the Executable Loader (`/X:ON`)
**File:** `src/dos/dos_execute.cpp`

This physically allows the DOSBox kernel to load `.EXE` and `.COM` files from the APPEND directories into memory.
*   **Changes:**
    *   Inside `DOS_Execute(const char* name, ...)`, add a hook at the top:
        ```cpp
        std::string resolved;
        if (dos_append::IsExecEnabled() && dos_append::ResolveName(name, resolved)) {
            // The file was found in the APPEND list, load it from 'resolved'
        }
        ```
    *   This guarantees that if the user typed `APPEND /X:ON`, the DOS kernel will seamlessly load programs located inside the appended directories.

