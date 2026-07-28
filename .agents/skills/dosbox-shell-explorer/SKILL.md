---
name: dosbox-shell-explorer
description: Use this skill to explore the internal DOSBox-Staging Shell (command prompt). Trigger this whenever the user asks about adding built-in commands (like DIR or ECHO), command redirection, batch files, or how the user first interacts with the emulator.
---

# DOSBox-Staging Shell Explorer

The DOSBox-Staging Shell is the first thing a user interacts with. It acts as both the command prompt and the batch file processor. When investigating the shell, follow this research framework:

## 1. Analyze the Shell Architecture (`src/shell/shell.h`)
When investigating how built-in shell commands work:
*   Open `src/shell/shell.h`.
*   Understand that `DOS_Shell` itself inherits from the `Program` class.
*   Built-in commands (like `DIR`, `COPY`, `ECHO`) are implemented directly as methods on the `DOS_Shell` class (e.g., `void CMD_DIR(char* args);`).
*   External commands (like `.COM` or `.EXE` files, or our custom `APPEND` command) are *not* built into `DOS_Shell`. Instead, `ExecuteShellCommand()` delegates them to the `Programs` subsystem to be executed.

## 2. Analyze Shell Built-ins (`src/shell/shell_cmds.cpp`)
If you need to add a small, built-in utility:
*   Check `src/shell/shell_cmds.cpp` to see how commands like `CMD_ECHO` or `CMD_PAUSE` are written.
*   Notice how they manipulate the `DOS_Shell::echo` state or read arguments.
*   Look at `AddShellCmdsToHelpList()` to see how commands are registered with their help text.

## 3. Analyze Batch and Redirection
*   If a user asks about pipes (`|`) or redirection (`>`), look at `DOS_Shell::GetRedirection()`.
*   If a user asks about `.BAT` files, look at the `BatchFile` class which reads lines and recursively invokes `DOS_Shell::ParseLine()`.

## 4. Extract Best Practices
After researching, extract the staging team's best practices. Look for:
*   How they parse arguments inside `shell_cmds.cpp` using `to_search_pattern` or by manually skipping whitespace.
*   How they handle localized output (`format_date`, `format_time`).

When you report your findings, make a clear distinction for the user: *Is this feature complex enough to be an external `Program` (like `APPEND`), or simple enough to be a built-in `CMD_*` method inside the shell?*
