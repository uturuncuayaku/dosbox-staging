---
name: skill-updater
description: Use this skill to perform a "Skill Audit". This skill instructs you to check the live DOSBox-Staging codebase and automatically rewrite other SKILL.md files if the upstream APIs they reference have changed.
---

# Skill Updater (Automated Audit)

The DOSBox-Staging architecture skills (Memory, CPU, Kernel, Shell, Programs) contain highly specific internal API names (e.g., `DOS_OpenFile`, `CALLBACK_SZF`). If the DOSBox-Staging team merges a PR that renames, refactors, or deletes these APIs, our custom skills will become stale and cause future agents to hallucinate.

When triggered to run a "Skill Audit", follow this strict procedure:

## 1. Target Discovery
1. Use `list_dir` on `.agents/skills/`.
2. For every `SKILL.md` file found, use `view_file` to read its contents.
3. Extract the specific source files it references (e.g., `src/hardware/memory.h`).

## 2. Upstream Verification
1. Read the live version of the referenced source file using `view_file`.
2. Check if the specific functions, macros, or best practices mentioned in the `SKILL.md` (e.g., `MEM_StrCopy`, `DOS_21Handler`) still exist.
3. If they don't exist, use `grep_search` to find what they were renamed to or replaced by.

## 3. Auto-Correction
1. If an API has been deprecated, renamed, or refactored, use the `replace_file_content` tool to surgically update the `SKILL.md` file.
2. Ensure the skill reflects the new standard. Do NOT delete the skill entirely.

## 4. Reporting
When the audit is complete, write a summary to the user indicating which skills were verified as accurate, and which skills were updated due to upstream changes.
