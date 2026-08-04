---
name: Session Handoff
description: Guidelines for safely resuming work after a quota interruption and preparing work for future agents.
---

# Session Handoff Protocol

Because sessions can be interrupted by quota limits or timeouts, context can be lost when starting a new chat. Agents must follow these practices to ensure a smooth transition.

## 1. When Resuming Work

If the user asks you to "resume work" or "continue where we left off":
- **Read the Handoff File:** Immediately read `.agents/handoff.md` in the workspace root if it exists.
- **Verify Code State:** Run `git status` and `git diff` to see what changes were left uncommitted by the previous agent.
- **Run the Tests:** Run the build or relevant tests (using the "Build DOSBox-Staging" skill) to get concrete footing on the current code state before making new changes.

## 2. Preparing for Handoff (When Asked)

If the user asks you to "prepare for handoff" or notes that quota is running low:
- Create or update `.agents/handoff.md` in the workspace root. Do NOT create this as a standard artifact, as conversation IDs change between sessions.
- Document the following clearly:
  1. **The Goal:** What is the overarching feature or bug fix being worked on.
  2. **Completed Steps:** What was successfully finished.
  3. **Current State:** What is currently broken, half-finished, or failing.
  4. **Next Steps:** The exact next actions the future agent should take (e.g., "Fix the return logic in `dos_append.cpp`").
  5. **Verification:** The exact test or build command to run to verify the next step.
