// Created by Antigravity
# Agent Skill Development Report
**Date:** July 28, 2026
**Project:** DOSBox-Staging

## 1. Executive Summary
Today's session focused on rapidly maturing the AI Agent's capability to safely modify and review the DOSBox-Staging emulator. Instead of relying on pre-trained (and often hallucinated) generic C++ knowledge, we developed a suite of **7 specialized skills** residing in the `.agents/skills/` directory. These skills force future agents to research specific emulator subsystems, internal APIs, and reference material before writing code or conducting reviews.

## 2. The 5 Architecture Skills
We adopted an iterative, granular approach to the architecture. Instead of one monolithic skill, we created 5 distinct subsystem explorers.

### 2.1 `dosbox-memory-explorer`
*   **Purpose:** Guides agents to understand the strict separation between host and guest memory.
*   **Key Learnings:** Taught the agent the difference between `PhysPt`, `RealPt`, and `HostPt`.
*   **Safeguards Established:** Agents must use `MEM_StrCopy()` and `phys_writeb()` instead of standard `memcpy()` to prevent host memory corruption.

### 2.2 `dosbox-cpu-explorer`
*   **Purpose:** Guides agents to safely interact with the emulated CPU, registers, and timing.
*   **Key Learnings:** Taught the agent how `modify_cycles` simulates hardware delays.
*   **Safeguards Established:** Agents must use `CALLBACK_SZF(true)` to manipulate guest flags rather than attempting inline assembly.

### 2.3 `dosbox-kernel-explorer`
*   **Purpose:** Guides agents on intercepting MS-DOS functions (INT 21h, INT 2Fh).
*   **Key Learnings:** Taught the agent how the `MultiplexHandler` works and how `DOS_OpenFile` maps guest paths to the host OS.
*   **Safeguards Established:** Agents must return kernel errors by setting the carry flag via `CALLBACK_SCF(true)`.

### 2.4 `dosbox-programs-explorer`
*   **Purpose:** Guides agents on writing new DOS binaries/commands that inherit from the `Program` class.
*   **Key Learnings:** Taught the agent how `CommandLine` parsing works.
*   **Safeguards Established:** Agents must use `WriteOut()` instead of `std::cout` to ensure localized output hits the guest console, not the host console.

### 2.5 `dosbox-shell-explorer`
*   **Purpose:** Guides agents on interacting with the internal `DOS_Shell`.
*   **Key Learnings:** Taught the agent how `CMD_*` built-ins are registered and how `GetRedirection()` parses command pipes.
*   **Safeguards Established:** Agents now know the architectural boundary between a built-in shell command and an external `Program`.

## 3. The `append-asm-reference` Skill
*   **Purpose:** To achieve 1:1 parity with MS-DOS 4.0, we created a skill specifically targeting the 3,400-line `documentation/reference/appendasm.txt` file.
*   **Impact:** This skill forces agents to cross-reference our C++ `APPEND` implementation against the original Microsoft assembly. It specifically checks for TopView multitasking barriers (`tv_flag`), exact INT 21h intercepts (`0Fh`, `4Bh`, `6Ch`, etc.), and correct error injection.

## 4. The `skill-updater` (Automation)
*   **Purpose:** To prevent these new skills from becoming stale.
*   **Impact:** If the DOSBox-Staging team merges a PR that renames an internal API (e.g., refactoring `DOS_OpenFile`), the `skill-updater` will perform an automated audit, detect the broken references in the `SKILL.md` files, and surgically rewrite the skills to match the live codebase.
*   **Execution:** This was configured to run automatically on a weekly basis via the `/schedule` cron system.

## 5. Conclusion & Ultimate Code Review
Following the creation of these skills, we performed a comprehensive Code Review of the DOSBox-Staging `APPEND` subsystem.
*   **Result:** The implementation was verified as structurally sound, architecturally safe, and functionally identical to MS-DOS 4.0. 
*   **Next Steps:** These skills are now permanently embedded in the repository's `.agents/` folder, ensuring any future AI agents that touch DOSBox-Staging will operate with unprecedented safety and domain expertise.
