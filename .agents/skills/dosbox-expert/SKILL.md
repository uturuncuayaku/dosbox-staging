---
name: dosbox-expert
description: Deep expertise in DOSBox-Staging architecture, C++23 standards, MS-DOS API hooks, and defensive programming. Make sure to use this skill whenever the user asks to modify DOS internals, intercept MS-DOS interrupts (like INT 21h or INT 2Fh), write new shell commands, or implement features requiring strict memory safety and RAII in the DOSBox-Staging codebase.
---

# DOSBox-Staging Expert Coding Skill

This skill contains the comprehensive architectural knowledge, C++23 coding conventions, and defensive programming strategies synthesized from extensive historical work on the DOSBox-Staging project (e.g., the `APPEND` subsystem).

When modifying the codebase, **do not guess** how memory is managed, how to hook MS-DOS interrupts, or what C++ standards to follow. Read the appropriate reference file below immediately to ensure your code matches the project's strict guidelines.

## Available Reference Files

You have access to highly detailed reference files. Read them using the `view_file` tool when applicable to your current task:

1. **Architecture & DOS Internals** (`.agents/skills/dosbox-expert/references/architecture.md`)
   - Read this when hooking MS-DOS interrupts (INT 21h, INT 2Fh).
   - Read this when allocating DOS conventional memory (MCBs, PSPs, DTAs).
   - Read this to understand how Multiplex Handlers work natively in C++ vs emulated TSRs.

2. **Defensive C++23 Programming** (`.agents/skills/dosbox-expert/references/defensive_cpp.md`)
   - Read this when parsing command-line strings.
   - Read this when handling recursive DOS file system calls.
   - Read this to understand the project's strict rules against C-style pointer arithmetic, the mandatory use of RAII, and transactional atomicity.

## General Workflow

1. Identify which component you are modifying (Shell command, DOS API hook, hardware emulation).
2. Read the relevant reference document from the `references/` folder.
3. Keep changes atomic. Do not mutate global state until all validation passes.
4. Ensure no infinite recursion can occur when DOS API hooks call back into the emulator.
