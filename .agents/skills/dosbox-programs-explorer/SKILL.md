---
name: dosbox-programs-explorer
description: Use this skill to explore how MS-DOS binaries and shell commands are emulated in DOSBox-Staging. Trigger this whenever the user asks about the `Program` class, writing a new shell command, parsing the DOS command line, or how executables interact with the DOS kernel.
---

# DOSBox-Staging Programs Explorer

When you need to analyze or write a new MS-DOS shell command (like `APPEND`, `DIR`, or `MOUNT`), do not attempt to write a standard C++ `main()` function. Executables in DOSBox-Staging are encapsulated in the `Program` class. Follow this research framework:

## 1. Analyze the Program API (`src/dos/programs.h`)
When investigating how a command is structured:
*   Open `src/dos/programs.h`.
*   Understand that your command must inherit from the `Program` base class and implement the `Run(void)` method.
*   Understand that `WriteOut()` is used to print text localized for the DOS console (you should never use `std::cout`).

## 2. Analyze Command Line Parsing (`src/shell/command_line.h`)
When investigating how arguments are passed:
*   The `Program` class provides a `CommandLine* cmd` pointer.
*   Check `src/shell/command_line.h` to see how you can safely extract arguments (e.g., `cmd->FindCommand()`, `cmd->GetCount()`).

## 3. Analyze Subsystem Integration
Programs are the highest-level components and must respect the emulator's architecture. Use your other skills to trace their impact:
*   **Memory:** If the program allocates a buffer for the guest, ensure it uses MCBs and safe `RealPt` copying (`dosbox-memory-explorer`).
*   **Kernel:** If the program needs to open a file or intercept a DOS interrupt, ensure it uses `DOS_OpenFile` or standard Multiplex hooks (`dosbox-kernel-explorer`).

## 4. Extract Best Practices
After researching a specific program (e.g., `src/dos/dos_append.cpp`), extract the staging team's best practices. Look for:
*   How they register the command with the shell using `PROGRAMS_MakeFile()`.
*   How they manage RAII and cleanup when the `Run()` method exits.

When you report your findings to the user, ensure your code-review advice guarantees the new program will compile cleanly and interact safely with the DOS ecosystem.
