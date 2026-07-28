---
name: git-workflow
description: DOSBox-Staging Git and Pull Request workflow. Make sure to use this skill whenever the user asks to manage git branches, recover commits, prepare for a pull request, or review git history.
---

# DOSBox-Staging Git & PR Workflow

This skill contains the established conventions for managing git branches and structuring Pull Requests in the DOSBox-Staging repository, as synthesized from past PR review reports and git management plans.

## 1. Branch Management & Commit Recovery
When asked to manage branches or recover commits:
*   Always check for dangling commits using `git fsck --dangling` before performing major branch resets, as lost work can often be found there.
*   If recovering a dangling commit, create a clearly named branch (e.g., `recovered-work`) pointing to the commit hash to stabilize it.
*   When synchronizing identical changes (like `.agents` skills) across multiple branches (`main`, `native-append-extended`, `native-append-feature`), use `git cherry-pick` to maintain commit history cleanly.

## 2. Pull Request Preparation Checklist
Before claiming a branch is ready for a Pull Request, verify the following:
*   **Code Formatting:** Ensure the code conforms to standard DOSBox-Staging formatting conventions.
*   **No Debug Artifacts:** Ensure no `printf`, `std::cout`, or logging remnants (used for debugging) are left in the source files.
*   **Test Coverage:** Full coverage of all logic paths in GTest must be present.
*   **Passing Tests:** Ensure all upstream tests pass locally using the standard CMake preset workflow.

## 3. Pull Request Report Structure
When asked to generate a PR description or review report, use the following structured template:

```markdown
# Pull Request Review: [Feature Name]

[Brief summary of the changes and architecture, e.g., "The PR implements..."]

Total footprint: **[X] files changed, [Y] insertions**

---

## 1. Core State & Resolution Engine (New Files)
*   **`src/path/file.cpp`**
    *   Description of state management and core logic.

## 2. Subsystem Hooks (Modified Files)
*   **`src/path/file.cpp`**
    *   Description of where and how the feature hooks into the existing emulator.

## 3. Build System & Registration (Modified Files)
*   **`src/dos/CMakeLists.txt`**
    *   Registered `[file].cpp` into the build target sources.

## 4. Comprehensive Unit Testing
*   **`tests/[feature]_tests.cpp`**
    *   Brief summary of test coverage (e.g., "State Tests", "Parser Tests").
```

Following this structure ensures reviewers have a clear, comprehensive understanding of the architecture.
