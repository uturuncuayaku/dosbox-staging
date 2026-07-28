---
name: dosbox-kernel-explorer
description: Use this skill to explore the MS-DOS Kernel emulation in DOSBox-Staging. Trigger this whenever the user asks about intercepting DOS interrupts (INT 21h, INT 2Fh), File Control Blocks (FCBs), DOS file handles, or how the emulator interacts with the host filesystem.
---

# DOSBox-Staging Kernel Explorer

When you need to analyze how DOSBox-Staging emulates the MS-DOS kernel, do not rely on standard C++ OS-level APIs (like `<fstream>` or POSIX `open`). The emulator has a complex intercept layer bridging guest DOS requests to the host filesystem. Follow this research framework:

## 1. Analyze the Interrupt Handlers (`src/dos/dos.cpp`)
When investigating how a feature intercepts a DOS command or file operation:
*   Use `grep_search` to find `DOS_21Handler` in `src/dos/dos.cpp`. This is the beating heart of the MS-DOS kernel (INT 21h).
*   Understand that `reg_ah` dictates the subfunction (e.g., `AH=3Dh` is Open File).
*   For Multiplex interrupts (INT 2Fh), look for how `MultiplexHandler` is registered and how it intercepts specific `reg_ah` / `reg_al` combinations (like `APPEND` using `INT 2Fh, AH=B7h`).

## 2. Analyze File Control Blocks & Handles (`src/dos/dos_files.cpp`)
When investigating how files are opened, read, or closed:
*   Open `src/dos/dos_files.cpp`.
*   Understand the distinction between legacy File Control Blocks (FCB) and modern DOS Handles (SFT - System File Tables).
*   Understand that `DOS_File` is a polymorphic base class. Local host files are handled by `localFile`, while CD-ROMs or virtual drives might use other derived classes.
*   Look at `DOS_OpenFile()` to see how it resolves a guest DOS path to a native host path.

## 3. Extract Best Practices
After researching the kernel subsystem, extract the staging team's best practices. Look for:
*   How they ensure error codes are properly preserved and returned in `reg_ax` with the Carry Flag set (`CALLBACK_SCF(true)`) upon failure.
*   How they prevent recursive lockups when a native hook (like `APPEND`) calls back into `DOS_OpenFile`.

When you report your findings to the user, frame them as actionable code-review rules or new guidelines that can be inherited by higher-level components (like Programs or Shell commands).
