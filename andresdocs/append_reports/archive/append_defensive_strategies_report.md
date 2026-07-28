> [!NOTE]
> **Historical snapshot - may not match current implementation.** See [append_consolidated_report.md](../append_consolidated_report.md) for the current authoritative specification.

# MS-DOS APPEND: Defensive Strategies & Code Changes Report

This report outlines the specific code changes implemented for the Phase 2 `APPEND` features (`/X`, `/E`, and `/PATH`) and the strict defensive programming strategies employed to prevent crashes, memory corruption, and security issues within DOSBox-Staging.

## 1. Safely Handling `/PATH` and Path Extraction
When `/PATH:ON` is active, MS-DOS expects `APPEND` to intercept requests even if they contain absolute drives or directory paths (like `C:\DATA\FILE.TXT`).

**The Change:**
I upgraded the `should_bypass_append(const char* name)` check. If a colon `:`, backslash `\`, or forward slash `/` is detected, it will now only bypass `APPEND` if `path_on_mode` is false. 

**Defensive Strategy (Modern C++ String Safety):**
To extract the actual filename from a requested absolute path, I implemented `extract_basename`. 
Instead of relying on legacy, unsafe C-style pointer arithmetic (like `strrchr` and modifying pointers directly, which can easily lead to off-by-one buffer overruns), I converted the path to a `std::string` and used `.find_last_of("\\/")`. This guarantees that extracting `FILE.TXT` from `C:\DATA\FILE.TXT` is memory-safe and cannot access out-of-bounds memory.

## 2. Preventing Infinite Recursion (`currently_resolving`)
Because `APPEND` intercepts file system calls (like `DOS_GetFileAttr`), but must *itself* use the file system to check if candidate files exist (via `DOS_GetFileAttr`), there is a massive risk of infinite recursion crashing the emulator stack.

**The Change:**
I implemented a strict boolean guard called `currently_resolving` inside `dos_append::ResolveName()`.

**Defensive Strategy (Reentrancy Guard):**
Before `ResolveName` attempts to test if a file exists, it sets `currently_resolving = true`. If the file system hooks try to call back into `APPEND` while this flag is set, they are instantly rejected. This completely immunizes DOSBox-Staging against stack overflow crashes due to `APPEND` recursively calling itself.

## 3. Transactional Atomicity for the Parser
When a user inputs a command like `APPEND /X:ON =C:\BadDir;;=`, we must guarantee that a bad directory doesn't result in a partially applied state.

**The Change:**
In `APPEND::Run()`, all state mutations (`dos_append::SetFlags()` and directory list assignments) were moved to the absolute bottom of the function.

**Defensive Strategy (Atomic Commits):**
The parser gathers flags into local boolean variables (`x_on`, `path_on`, etc.) and tests the directory strings first. If `DOS_MakeName` detects an illegally formatted path (e.g., extra `=` signs or nonexistent drives), the parser hits an immediate `return;`. Because the `SetFlags` commit happens at the very end of the function, a malformed command aborts safely without ever touching or corrupting the global system state.

## 4. Strict Type Safety in Execution Hooks
When injecting the `APPEND` resolver into `DOS_Execute`, we needed to pass the newly resolved executable path into the environment builder (`MakeEnv`).

**The Change:**
I altered `MakeEnv()` in `dos_execute.cpp` to explicitly require a `const char*` instead of a mutable `char*`.

**Defensive Strategy (Immutability):**
By strictly enforcing `const char*`, we guarantee at compile-time that `MakeEnv` cannot accidentally manipulate or truncate the resolved application path string before executing it. This prevents arbitrary bugs where programs fail to start due to corrupted path memory.

## 5. Ephemeral Command Line Parsing
We utilized `cmd->FindExistRemoveAll(string)` to parse flags like `/X:ON` securely.

**Defensive Strategy (Destructive Parsing on Ephemeral Buffers):**
Instead of writing complex regular expressions to find flags, we simply pluck them out of the command string destructively. Because the `cmd` object is a temporary, ephemeral buffer allocated just for the lifespan of that specific command invocation, modifying it in-place is incredibly secure, eliminates parsing ambiguity, and guarantees that flags are completely separated from the directory paths before structural validation begins.

