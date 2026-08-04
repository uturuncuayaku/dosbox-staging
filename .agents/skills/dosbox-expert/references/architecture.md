# Architecture & DOS Internals

When modifying the core DOS emulator inside DOSBox-Staging, follow these established patterns:

## 1. Multiplex Handlers (INT 2Fh)
We do not emulate TSRs (Terminate and Stay Resident programs) in x86 assembly. Instead, we implement them natively in C++ for maximum performance, memory savings, and testability.

*   Register a multiplex handler during your subsystem's `Init()` phase:
    ```cpp
    DOS_AddMultiplexHandler(MultiplexHandler);
    ```
*   Your handler should check `reg_ah` to see if the interrupt targets your subsystem. If not, it MUST return `false` so the dispatcher can pass it to the next handler.
    ```cpp
    if (reg_ah != 0xB7) return false; // Not our subsystem
    ```
*   When handling subfunctions via `reg_al`, mutate the CPU registers (e.g., `reg_ax`, `reg_bx`) directly. Return `true` if you handled it.

## 2. DOS Memory Allocation (Conventional Memory)
If your subsystem needs to expose a memory buffer to legacy DOS programs (via Far Pointers, `ES:DI`), you must allocate it within the emulated 640KB RAM.
*   Use `DOS_GetMemory()` during initialization (which allocates paragraphs, 1 paragraph = 16 bytes).
*   Zero-fill and write data to this block using `MEM_BlockWrite(address, buffer, size)`.
*   Pass the pointer back to DOS programs using `SegSet16(es, segment)` and setting the offset (e.g., `reg_di = 0`).

## 3. DOS API Hooks (INT 21h)
When intercepting file operations (e.g., `DOS_OpenFile`, `DOS_FCBOpen`):
*   Hooks are placed directly inside `dos_files.cpp` or `dos_execute.cpp`.
*   Your intercept logic (e.g., `dos_append::find_absolute_path()`) must be lean.
*   **CRITICAL:** If your hook calls another DOS API function (e.g., calling `DOS_FileExists` inside a file open hook), you risk infinite recursion because `DOS_FileExists` calls `DOS_OpenFile`. You **must** use a recursion guard (see Defensive Programming reference).

## 4. Environment Variables (PSP)
If your feature supports interacting with the DOS environment (like `APPEND /E`):
*   To write: Use `shell->SetEnv("VAR", value)`.
*   To read: DO NOT use `shell->GetEnv()`. You must read from the Program Segment Prefix (PSP) of the currently running program: `shell->psp->GetEnvironmentValue("VAR")`. This ensures you see local changes made by the running program.
