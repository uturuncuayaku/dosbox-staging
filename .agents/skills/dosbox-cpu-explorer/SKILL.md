---
name: dosbox-cpu-explorer
description: Use this skill to explore DOSBox-Staging's emulated CPU architecture. Trigger this whenever the user asks about hardware interrupts, CPU cycles, modifying emulated registers (e.g., reg_ax, reg_bx), or how C++ callbacks interface with the x86 guest.
---

# DOSBox-Staging CPU Explorer

When you need to analyze how the CPU is emulated or how native C++ code intercepts guest x86 execution, do not rely on standard x86 assembly knowledge. DOSBox-Staging uses a highly custom dynamic recompiler and callback system. Follow this research framework:

## 1. Analyze CPU Registers (`include/cpu/registers.h`)
When investigating how an interrupt handler reads or writes state:
*   Open `include/cpu/registers.h`.
*   Understand that general-purpose registers are exposed as global variables (e.g., `reg_eax`, `reg_ax`, `reg_ah`, `reg_al`).
*   Understand the Segment Registers (`SegValue(ds)`, `SegValue(es)`) and how they are read.

## 2. Analyze the Callback System (`src/cpu/callback.cpp`)
When investigating how MS-DOS features (like INT 21h) are implemented in native C++:
*   Understand that `CALLBACK_Setup` is used to register a C++ function.
*   The system works by placing an illegal x86 opcode in guest memory. When the emulated CPU hits this opcode, it traps out to the host C++ code.
*   Look for `CALLBACK_RunRealInt()` to see how native code can force the guest CPU to execute a software interrupt.

## 3. Analyze Cycle Management (`src/cpu/cpu.cpp`)
When investigating performance or retro-gaming accuracy:
*   Use `grep_search` for `modify_cycles` or `CPU_Cycles`.
*   Understand how DOSBox simulates hardware delays (e.g., floppy drives) by subtracting from `CPU_Cycles` so that the emulated game doesn't run infinitely fast.

## 4. Extract Best Practices
After researching the CPU subsystem, extract the staging team's best practices. Look for:
*   How they safely read parameters from `reg_ax` vs `reg_al` in a Multiplex handler.
*   How they ensure the CPU state is preserved (`CPU_STI()`, `CALLBACK_SZF(true)` to set the zero flag).

When you report your findings to the user, frame them as actionable code-review rules or new guidelines that can be inherited by higher-level components (like Programs or Shell commands).
