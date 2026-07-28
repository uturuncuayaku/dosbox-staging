---
name: dosbox-architecture-explorer
description: Explore DOSBox-Staging's architecture to analyze and extract best practices for the MS-DOS Kernel, CPU, and Memory subsystems. Make sure to use this skill whenever the user asks you to deep dive into the codebase, explain how a DOS feature works, research hardware emulation, or generate tests based on kernel best practices.
---

# DOSBox-Staging Architecture Explorer

When the user asks you to analyze the codebase to understand how MS-DOS features, CPU APIs, or Memory APIs work, **DO NOT rely on your pre-trained knowledge**. Instead, actively research the codebase using the following structured approach.

## 1. Research the Kernel (`src/dos/`)
The beating heart of MS-DOS emulation is `src/dos/dos.cpp`. When researching how DOS handles commands, files, or interrupts:
*   Use `grep_search` to find `DOS_21Handler`. This is the entry point for almost all DOS interrupts.
*   Look at how `DOS_PerformDiskIoDelay` injects simulated delays for retro games.
*   Check `dos_memory.cpp` for how Memory Control Blocks (MCBs) are allocated to Programs.

## 2. Research the Memory (`src/hardware/memory.cpp`)
Memory in DOSBox is heavily simulated. Do not assume standard C++ pointer arithmetic works on guest memory.
*   Look at `include/hardware/memory.h`. Understand the strict separation between `PhysPt` (physical 32-bit), `RealPt` (Segment:Offset), and `HostPt` (native C++ pointers).
*   Find where `real_readb` or `phys_writeb` are used instead of `memcpy`.

## 3. Research the CPU (`src/cpu/`)
The CPU interacts directly with C++ code via Callbacks.
*   Look at `src/cpu/callback.cpp` to see how x86 interrupts are hooked into native functions.
*   Look at `include/cpu/registers.h` to see how the CPU state (`reg_ax`, `reg_bx`) is exposed.

## 4. Synthesize Good Coding Practices
After your research, extract the **dosbox-staging specific** coding practices you found. For example:
*   How do they use RAII in the kernel?
*   How do they handle parsing (like in `src/dos/dos_files.cpp`)?
*   How do they log errors or handle assertions?

Present these findings to the user as actionable code-review advice or propose converting them into permanent reference documents (like new `.md` files in the `dosbox-expert` skill).
