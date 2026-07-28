---
name: dosbox-memory-explorer
description: Use this skill to explore DOSBox-Staging's simulated memory architecture. Trigger this whenever the user asks about Memory Control Blocks (MCBs), pointer translation (PhysPt, RealPt, HostPt), or how to safely read/write guest memory without crashing the emulator.
---

# DOSBox-Staging Memory Explorer

When you need to analyze how memory is allocated, accessed, or manipulated in DOSBox-Staging, do not rely on standard C++ pointer knowledge. The emulator has a strict separation between host and guest memory. Follow this research framework:

## 1. Analyze the Memory API (`src/hardware/memory.h`)
When investigating how a feature reads or writes memory:
*   Understand the three pointer types:
    *   `PhysPt`: A 32-bit linear physical address.
    *   `RealPt`: A Segment:Offset 16:16 address (stored as a 32-bit integer).
    *   `HostPt`: A native C++ pointer (`uint8_t*`) into the host machine's RAM.
*   Look for safety functions: `real_readb()`, `phys_writeb()`, and `MEM_BlockCopy()`. You should almost never see `memcpy` used directly with a `RealPt` or `PhysPt`.

## 2. Analyze DOS Memory Allocation (`src/dos/dos_memory.cpp`)
When investigating how MS-DOS programs get RAM:
*   Use `grep_search` to find `DOS_BuildUMBChain` or `DOS_AllocateMemory`.
*   Analyze how Memory Control Blocks (MCBs) are constructed and linked together to form the DOS heap.
*   Check how the Program Segment Prefix (PSP) is tied to these allocations.

## 3. Extract Best Practices
After researching the memory subsystem, extract the staging team's best practices. Look for:
*   How they prevent out-of-bounds reads when a guest program requests a memory block.
*   How they ensure host memory (like C++ strings) is safely copied into the emulated 1MB address space (`MEM_StrCopy`).

When you report your findings to the user, frame them as actionable code-review rules or new guidelines that can be inherited by higher-level components (like Programs or Shell commands).
