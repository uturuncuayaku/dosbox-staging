<!-- Created by Antigravity -->
# Commit 1 Report: Core APPEND State Management and Multiplex API

**Commit Hash:** `f94136f3d`
**Files Modified:**
- [dos_append.h](file:///c:/Users/andtr/Documents/GitHub/dosbox-staging-attempt3/dosbox-staging/src/dos/dos_append.h)
- [dos_append.cpp](file:///c:/Users/andtr/Documents/GitHub/dosbox-staging-attempt3/dosbox-staging/src/dos/dos_append.cpp)

---

## 1. Technical Purpose & Architecture

This commit introduces the core state storage, path resolution engine, and interrupt multiplex handler for `APPEND`. In MS-DOS 4.0, `APPEND` functions as a TSR (Terminate and Stay Resident) program that hooks `INT 2Fh` (Multiplex Interrupt, `AH=B7h`) and exposes memory structures to software. In DOSBox Staging, this is implemented as an internal C++ subsystem under the `dos_append` namespace.

---

## 2. Key C++ Concepts & Implementation Mechanics

### A. Memory Mirroring (`syncDirListToDos`)
- Real DOS programs query subfunction `AX=B704h` to obtain a far pointer (`ES:DI`) pointing to the null-terminated search path string in DOS memory.
- `DOS_GetMemory(kDirlistDosPages)` allocates 16 paragraphs (256 bytes) in conventional emulated DOS memory during `Init()`.
- `syncDirListToDos()` writes `dir_list` into physical DOS memory via `MEM_BlockWrite` at address `PhysPt(dirlist_dos_segment) << 4`.
- The memory buffer is zero-filled with `mem_writeb` before copying to prevent data leakage from previous strings.

### B. Efficient String Splitting (`std::views::split` & `std::string_view`)
- `ResolveName` parses the semicolon-delimited search path string without allocating new string objects on the heap.
- C++20 ranges `sv | std::views::split(';')` are used to iterate over `std::string_view` subranges of `dir_list`.
- `checkCandidate()` strips trailing directory separators with `remove_suffix(1)` and constructs candidate paths for checking against `DOS_FileExists()`.

### C. Reentrancy Guard (`currently_resolving`)
- `ResolveName` sets a boolean flag `currently_resolving = true` while searching.
- If an internal filesystem operation (such as `DOS_FileExists` or drive checks) triggers another file lookup, `ResolveName` detects `currently_resolving == true` and immediately returns `false`. This prevents stack overflow caused by recursive resolution loops.

### D. Multiplex Handler (`MultiplexHandler`)
Handles `INT 2Fh AH=B7h` subfunctions:
- `AL=00h`: Returns `AL=0xFF` (Installation check).
- `AL=02h`: Returns `AX=0xFFFF` (MS-DOS 4.0 `APPEND.ASM` identity signature, distinguishing it from IBM PC Network `APPEND`).
- `AL=04h`: Sets `ES` to `dirlist_dos_segment` and `DI=0x0000` (Return directory list pointer).
- `AL=06h`: Returns `BX=0x0001` if enabled, `0x0000` if disabled (Get state).
- `AL=07h`: Sets `enabled` status based on `BX & 0x0001` (Set state).
- `AL=10h`: Returns `AX=mode_flags`, `BX=0`, `CX=0`, `DL=major`, `DH=minor` (MS-DOS version query).

---

## 3. Analysis & Trade-offs

### Pros:
- Using `std::string_view` and `std::views::split` eliminates heap allocations during path resolution.
- Reentrancy guard prevents recursive loops during nested DOS file operations.
- Full compliance with MS-DOS 4.0 `INT 2Fh AH=B7h` multiplex subfunctions allows real DOS software to inspect `APPEND` state without crashing.

### Cons:
- Conventional DOS memory allocation (256 bytes) is fixed at emulator startup and cannot expand beyond 255 characters (matching MS-DOS 4.0 buffer size limits).
- Shared global namespace state (`dos_append`) is not thread-safe, matching DOSBox Staging's single-threaded DOS emulator execution model.

---

## 4. Cross References
- [append_asm_report.md](file:///c:/Users/andtr/Documents/GitHub/dosbox-staging-attempt3/dosbox-staging/documentation/append_asm_report.md) for details on MS-DOS 4.0 `APPEND.ASM` multiplex subfunctions `00h`, `02h`, `04h`, `06h`, `07h`, `10h`.
- [append_implementation_glossary.md](file:///c:/Users/andtr/Documents/GitHub/dosbox-staging-attempt3/dosbox-staging/documentation/append_implementation_glossary.md) for terminology regarding conventional memory mirroring and multiplex handlers.
