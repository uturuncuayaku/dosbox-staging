# Created by Antigravity
# Multi-Branch & Orphan Strategy Workflow Report

## Executive Summary
This report documents the multi-branch and orphan strategy established for managing the DOSBox-Staging repository with AI pair-programming assistance. It separates **upstream feature code**, **AI agent configuration skills**, and **personal developer documentation**.

---

## 1. Branch Architecture Overview

| Branch Name | Type | Purpose & Scope | Upstream PR Eligible? |
| :--- | :--- | :--- | :--- |
| **`main` / `native-append-extended`** | Standard Feature Branch | Pure C++ source code (`src/`), build definitions (`CMakeLists.txt`), and GTest files (`tests/`). | **YES** |
| **`append-ai-configs`** | Orphan Branch | Version-controls AI agent skills (`.agents/skills/`) and workspace rules (`.agents/AGENTS.md`). | **NO** (Internal AI Config) |
| **`docs`** | Documentation Branch | Version-controls personal developer reports, architecture diagrams, and helper scripts (`andresdocs/`). | **NO** (Personal Notes) |

---

## 2. Personal Local Exclusion (`.git/info/exclude`)

To prevent local AI configs and personal documentation from leaking into standard feature commits, [.git/info/exclude](file:///home/aftg/Documents/github/revising/dosbox-staging/.git/info/exclude) isolates these paths locally on your machine without modifying `.gitignore`:

```gitignore
# Personal git exclude entries for APPEND test files
tests/files/append/

# Personal documentation, scripts, reports, and PR drafts
andresdocs/
andresdocs/scripts/

# Agent workspace configurations & skills
.agents/
.agents/skills/
```

---

## 3. Step-by-Step Workflows

### A. Daily Feature Coding Workflow
1. Work on `native-append-extended` branch.
2. Execute tests without blocking turn execution via local test script:
   ```bash
   ./andresdocs/scripts/run_append_tests.sh
   ```
3. Commit and push standard C++ and unit test changes directly to `origin/native-append-extended`.

### B. Updating AI Agent Skills (`append-ai-configs`)
When creating or editing skills/rules in `.agents/`:
```bash
# 1. Switch to AI config timeline
git checkout append-ai-configs
git reset

# 2. Stage updated skills
git add -f .agents/
git commit -m "Update Antigravity skills"
git push origin append-ai-configs

# 3. Return to feature coding
git checkout -f native-append-extended
```

### C. Updating Personal Documentation & Reports (`docs`)
When saving developer notes, architecture reports, or PR drafts in `andresdocs/`:
```bash
# 1. Switch to docs timeline
git checkout docs
git reset

# 2. Stage personal documentation
git add -f andresdocs/
git commit -m "Update development reports and notes"
git push origin docs

# 3. Return to feature coding
git checkout -f native-append-extended
```

### D. Preparing & Opening an Upstream Pull Request
When submitting the final feature PR to `dosbox-staging/dosbox-staging`:
```bash
# 1. Create clean PR branch off upstream main
git checkout -b pr/ms-dos-append upstream/main

# 2. Cherry-pick only clean feature commits
git cherry-pick <commit-hashes>

# 3. Push to your GitHub fork and submit PR
git push -u origin pr/ms-dos-append
```
