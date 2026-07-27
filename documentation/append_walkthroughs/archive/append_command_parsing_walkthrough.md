> [!NOTE]
> **Historical snapshot - may not match current implementation.** See [append_consolidated_report.md](../append_consolidated_report.md) for the current authoritative specification.

# APPEND Command Parsing Walkthrough

## What Was Done

Updated the command-line parsing logic for the DOSBox `APPEND` command. The command now formats directory lists and checks them against the internal filesystem before accepting them. Added a basic unit test suite.

## Implementation Details

1. **Switch Parsing:**
   - Modified `src/dos/programs/append.cpp` to ignore native DOS switches (`/X`, `/X:ON`, `/X:OFF`, `/E`, `/PATH:ON`, `/PATH:OFF`).
   - Used `CommandLine::FindExistRemoveAll()` to remove these flags from the command-line arguments.

2. **Whitespace and Quotations:**
   - Added logic to trim spaces and double-quotes enclosing directory names (`"C:\DIR"` -> `C:\DIR`).

3. **Path Expansion and Validation:**
   - Used `DOS_MakeName()` to expand relative directories (e.g. `DATA` -> `C:\TEST\DATA`).
   - Checked filesystem existence using `Drives[drive]->TestDir()`. If the directory is invalid, it prints `PROGRAM_APPEND_INVALID_PATH` and aborts.

4. **Drive Letter Fix:**
   - `DOS_MakeName()` outputs the path without the drive letter. Added code to re-attach the drive letter (e.g. `C:\fullname`) before storing it.

## Testing

The changes were tested in `tests/dos_append_tests.cpp`.

- **Tests added:** 6 tests.
- **Tested scenarios:**
  - Switch removal
  - Duplicate directories
  - Whitespace and quote trimming
  - Empty tokens
  - Absolute path expansion
  - Invalid path rejection
- **Test result:** 26 tests passed.

## MS-DOS 4.0 `APPEND.ASM` Parser Comparison

To ensure authenticity, we analyzed the original MS-DOS 4.0 `APPEND.ASM` and `APPENDP.INC` source files to compare their parsing logic with DOSBox Staging.

In MS-DOS 4.0, APPEND did not manually parse strings. Instead, it relied on `SYSPARSE`, a centralized parser module shared among DOS utilities. `APPENDP.INC` simply defined a configuration block (`$P_PARMS_Blk`) telling `SYSPARSE` how to behave:

- **Switch Definitions:** It explicitly registers `/E`, `/X`, and `/PATH`.
- **Value Definitions:** It registers `ON` and `OFF` string values for `/X` and `/PATH` (e.g., `/X:ON`).
- **File Table Formatting (`$P_CAP_File`):** MS-DOS 4.0 introduced international code pages. Setting this flag instructed `SYSPARSE` to run the parsed directory string through the internal DOS capitalization mapping table. This normalized the path by properly uppercasing foreign and standard characters according to DOS filename rules.
- **Quote Support (`QUSSW EQU 0`):** MS-DOS's `SYSPARSE` had built-in support for encapsulating arguments in double quotes. This allowed paths with spaces or reserved delimiters (like `;`) to be treated as a single token. Setting this flag to `0` ("Do not suppress") enabled this feature, meaning `SYSPARSE` handled identifying and stripping the quotes before handing the clean path back to APPEND.

### Similarities to DOSBox Staging
Our C++ implementation achieves the exact same functional outcome as the original `SYSPARSE` configuration:
- We explicitly look for and consume the exact same `/E`, `/X:ON|OFF`, `/PATH:ON|OFF` switches.
- We apply DOS filename rules via `DOS_MakeName` and manual trimming, similar to the `$P_CAP_File` table formatting.
- We support and trim quoted strings, matching the `SYSPARSE` quoted string support.

