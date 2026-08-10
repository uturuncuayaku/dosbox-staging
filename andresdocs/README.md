# DOSBox-Staging: Native MS-DOS `APPEND` Subsystem — Master Documentation & Iteration Overview

**Repository Fork:** [uturuncuayaku/dosbox-staging](https://github.com/uturuncuayaku/dosbox-staging)  
**Branch:** `docs` (Tracking feature implementation on `native-append-feature` / `pr-append`)  
**Specification Standard:** ISO/IEC WG21 C++23 (N4950 Draft Standard)  
**Ground Truth Reference:** Microsoft MS-DOS 4.0 `APPEND.ASM` (MIT License)  

---

## 1. Current Development Iteration Overview

### Iteration Focus: Production-Ready `APPEND` Integration & Pull Request Decomposition

The current development iteration implements a **native, TSR-less C++23 MS-DOS `APPEND` command subsystem** inside DOSBox-Staging. This subsystem allows DOS games and software to resolve and open data files across configured search paths without altering working directory state or executing Real-Mode TSR interrupt vectors.

```
┌─────────────────────────────────────────────────────────────────────────────────┐
│                           DOSBox-Staging MS-DOS Shell                           │
│  User / Game issues APPEND C:\GAMES\AUDIO;C:\GAMES\DATA or sets %APPEND% env    │
└────────────────────────────────────────┬────────────────────────────────────────┘
                                         │
                                         ▼
┌─────────────────────────────────────────────────────────────────────────────────┐
│                      Command Line Parser & Synchronizer                         │
│   • Parses /E, /X, /PATH, /? switches (APPEND::Run)                              │
│   • Trims quotes, normalizes paths via DOS_MakeName, validates via TestDir       │
│   • Mirror-syncs 256-byte path string into emulated DOS RAM (AX=B704h)           │
└────────────────────────────────────────┬────────────────────────────────────────┘
                                         │
                                         ▼
┌─────────────────────────────────────────────────────────────────────────────────┐
│                       DOS Kernel Hook (DOS_OpenFile)                            │
│   1. Standard open attempted in current directory                               │
│   2. If missing (DOSERR_FILE_NOT_FOUND), delegates to dos_append::ResolveName()  │
│   3. Evaluates active flags (/X:ON vs /X:OFF, /PATH:ON vs /PATH:OFF)           │
│   4. Iterates semicolon-delimited candidate paths, returns transparent DOS path │
└────────────────────────────────────────┬────────────────────────────────────────┘
                                         │
                                         ▼
┌─────────────────────────────────────────────────────────────────────────────────┐
│                        INT 2Fh Multiplex API Server                             │
│   Responds to legacy DOS application multiplex queries (AH=B7h):                │
│   • 00h: Installation check (Returns AL=FFh)                                    │
│   • 01h: Set search list                                                        │
│   • 02h: Version check (Returns AX=0400h)                                       │
│   • 04h: Get directory list pointer (Returns ES:DI)                             │
│   • 06h / 07h: Get / Set mode flags (BX bitmasks)                               │
│   • 10h: Get version info                                                       │
│   • 11h: Set state / flags                                                      │
└─────────────────────────────────────────────────────────────────────────────────┘
```

### Key Milestones Completed in Current Iteration:
1. **INT 2Fh Subfunctions `01h` & `11h` Implementation:** Expanded `dos_append` multiplex interface to support dynamic search list assignment (`01h`) and state control flag mutations (`11h`).
2. **`/E` Environment Block Synchronization:** Dynamic bi-directional synchronization between the `%APPEND%` environment variable and internal C++ `dos_append` directory vector.
3. **`FindFirst` / `FindNext` Search Resolution:** Integrated fallback directory resolution for directory search operations when `/X:ON` is active.
4. **Google Test Unit Suite Verification:** 36 passing unit tests covering edge cases, quoted paths, relative path resolution, re-entrancy guards, and register state assertions.
5. **Incremental Commit Decomposition:** Structured the feature branch into 5 clean, reviewable commits for upstream PR submission.

---

## 2. Master Documentation Index & Summary of Reports

The `andresdocs` directory contains technical analysis, architectural blueprints, commit-by-commit breakdowns, and PR submission assets.

### 2.1 Technical & Architecture Reports (`append_reports/`)

* **[Consolidated Technical Report](append_reports/append_consolidated_report.md)**  
  *Master Ground Truth Specification & Parity Matrix.* Maps every routine in Microsoft's MS-DOS 4.0 `APPEND.ASM` to its native C++23 implementation in DOSBox-Staging. Details memory layout, C++20 `std::views::split` zero-allocation parsing, and reentrancy protection.
* **[/E Mode & FindFirst Resolution Report](append_reports/append_e_mode_and_findfirst_resolution_report.md)**  
  *Deep-dive into Environment Variable Sync and File Search.* Explains how `/E` binds `GetDirectories()` dynamically to `%APPEND%` and details `DOS_FindFirst` search hook execution.
* **[APPEND Architecture & Control Flow Diagrams](append_reports/append_diagrams.md)**  
  *Visual Architecture Blueprints (DIAG-01 to DIAG-08).* Contains ASCII diagrams for kernel file redirection, memory pointer mirroring, multiplex handling, and state flags.
* **[Archived Append Technical Reports](append_reports/archive/)**  
  *Historical analysis & phase reports:*
  * [Consolidated Execution Framework](append_reports/archive/append_detailed_execution_framework.md)
  * [Fuzz Testing Implementation Report](append_reports/archive/msdos_append_fuzz_testing_implementation_report.md)
  * [Defensive Strategies & Security Audit](append_reports/archive/append_defensive_strategies_report.md)
  * [Environment Block Architectural Deep-Dive](append_reports/archive/append_environment_block_report.md)
  * [Implementation Glossary & Symbol Table](append_reports/archive/append_implementation_glossary.md)
  * [DOS Files Integration Technical Report](append_reports/archive/msdos_append_dos_files_integration_report.md)

---

### 2.2 Incremental Commit Breakdown Reports (`commit_reports/`)

* **[Commit Reports Index & Variable Analysis](commit_reports/README.md)**  
  *Master index for incremental pull request commits.*
* **[Commit 1: Core State & Multiplex API](commit_reports/commit_1_core_state_and_multiplex.md)**  
  *`dos_append` Namespace & INT 2Fh Multiplex Handler.* Details state storage, DOS memory mirroring (`AX=B704h`), reentrancy guard (`currently_resolving`), and multiplex subfunctions.
* **[Commit 2: Command Parser & Shell Integration](commit_reports/commit_2_command_parser.md)**  
  *`APPEND::Run` Shell Command.* Details switch parsing (`/E`, `/X`, `/PATH`), path normalization (`DOS_MakeName`), and directory existence validation (`TestDir()`).
* **[Commit 3: DOS_OpenFile Filesystem Hook](commit_reports/commit_3_filesystem_hook.md)**  
  *Kernel File Redirection.* Details fallback resolution in `DOS_OpenFile` and preserving error codes on misses.
* **[Commit 4: Unit Test Suite Breakdown](commit_reports/commit_4_unit_tests.md)**  
  *Google Test Verification.* Details the 36 unit tests covering state transitions, path resolution, and multiplex registers.
* **[Commit 5: Website Documentation](commit_reports/commit_5_website_documentation.md)**  
  *User Manual Updates.* Details updates to `commands.md` in the DOSBox-Staging manual.
* **[Variable & Object Analysis Across Commits](commit_reports/variable_object_analysis.md)**  
  *C++ Variable Lifecycle Audit.* Traces object definitions, initialization, memory allocation, and lifetime across all 5 commits.

---

### 2.3 Pull Request & Upstream Submission Assets (`pull_requests/`)

* **[Official Upstream Pull Request Description](pull_requests/Append_Pull_Request.md)**  
  *Ready-to-submit PR body.* Formatted for the DOSBox-Staging repository maintainers with feature list, compliance table, and testing evidence.
* **[Pull Request Draft & Checklist](pull_requests/Append-PR-Draft.md)**  
  *Draft PR text & review checklist.*
* **[Incremental Commit Strategy](pull_requests/Incremental_Commit_Strategy.md)**  
  *Decomposition roadmap.* Explains why the branch is split into 5 modular commits to facilitate easy maintainer review.
* **[Pull Request Review Audit Report](pull_requests/pr_review_report.md)**  
  *Code review & audit findings.* Resolves potential warnings, deprecations, and style guidelines.

---

### 2.4 Developer Workflows & Developer Tooling (`dev_workflows/`)

* **[Website Documentation Technical Report](dev_workflows/website_documentation_report.md)**
* **[Loguru Logging Integration Walkthrough](dev_workflows/loguru_walkthrough.md)**
* **[Local Untracked Code Workflows](dev_workflows/local_untracked_code_workflows.md)**
* **[Trace & Debug Walkthrough](dev_workflows/trace_walkthrough.md)**
* **[Clangd Language Server Setup](dev_workflows/clangd_walkthrough.md)**
* **[Local Trace Infrastructure Guide](dev_workflows/using_local_trace.md)**

---

### 2.5 AI Agent & Skill Development Reports (`agent_reports/`)

* **[AI Agent Skill Development Report (2026-07-28)](agent_reports/2026_07_28_skill_development_report.md)**  
  *Agentic Workflow Summary.* Documents custom agent skills, automation scripts, and LLM coding guidelines used during development.

---

## 3. Directory Structure Summary

```text
andresdocs/
├── README.md                               <-- (This File) Master Index & Iteration Summary
├── agent_reports/
│   └── 2026_07_28_skill_development_report.md
├── append_reports/
│   ├── append_consolidated_report.md       <-- Master Ground Truth Specification
│   ├── append_diagrams.md                  <-- ASCII Control Flow Diagrams
│   ├── append_e_mode_and_findfirst_resolution_report.md
│   └── archive/                            <-- Historical Phase Reports
├── append_walkthroughs/
│   └── archive/                            <-- Implementation Walkthroughs
├── commit_reports/
│   ├── README.md                           <-- Commit Reports Index
│   ├── commit_1_core_state_and_multiplex.md
│   ├── commit_2_command_parser.md
│   ├── commit_3_filesystem_hook.md
│   ├── commit_4_unit_tests.md
│   ├── commit_5_website_documentation.md
│   └── variable_object_analysis.md
├── dev_workflows/                          <-- Tooling & Debugging Guides
├── pull_requests/
│   ├── Append_Pull_Request.md              <-- Upstream PR Submission Document
│   ├── Incremental_Commit_Strategy.md
│   └── pr_review_report.md
└── reference/                              <-- MS-DOS 4.0 APPEND.ASM Ground Truth & Standards
```
