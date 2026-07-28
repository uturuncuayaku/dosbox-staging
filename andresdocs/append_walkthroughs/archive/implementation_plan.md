> [!NOTE]
> **Historical snapshot - may not match current implementation.** See [append_consolidated_report.md](../append_consolidated_report.md) for the current authoritative specification.

# Implementation Plan: Semantic APPEND Tracing

This plan outlines the architectural updates to transform the `APPEND` feature's tracing from a flat debug stream into a structured, semantic execution tree that reads like a debugger transcript.

## User Review Required
> [!IMPORTANT]
> The Loguru preamble (e.g. `2026-07-22 ... INFO|`) is hardcoded into Loguru's global state. To remove it cleanly during your tests without permanently affecting the rest of DOSBox, I will save the old Loguru configuration state at the beginning of `DOS_AppendTest::SetUp`, disable the preambles, and restore the state in `TearDown()`. This effectively gives you a completely clean, raw test output. Is this acceptable?

## Proposed Changes

### 1. `tests/dos_append_tests.cpp`
We will encapsulate the tests in a clean logging environment by temporarily overriding Loguru's format flags.

*   **[MODIFY] `tests/dos_append_tests.cpp`**
    *   In `DOS_AppendTest::SetUp()`, save the current states of `loguru::g_preamble_date`, `loguru::g_preamble_time`, `loguru::g_preamble_thread`, `loguru::g_preamble_file`, and `loguru::g_preamble_uptime`.
    *   Set them all to `false` to suppress the noisy prefix.
    *   In `TearDown()`, restore them to their original values.

### 2. `src/dos/dos_append.cpp` (and `.h`)
We will completely overhaul the `AppendTraceScope` class to act as a structured semantic logger rather than a basic text wrapper.

*   **[MODIFY] `AppendTraceScope` constructor/destructor**
    *   Integrate `std::chrono::high_resolution_clock` to track scope entry and exit time.
    *   Format `ENTER:` and `EXIT:` blocks exactly as requested, generating and including a unique `[APPEND-TRACE-XXX]` call ID.
    *   Include caller information using `std::source_location`.
*   **[MODIFY] `AppendTraceScope` API**
    *   Introduce strict semantic logging methods:
        *   `void State(const char* name, const char* value)`
        *   `void Action(const char* action)`
        *   `void Decision(const char* decision, const char* reason)`
        *   `void Result(const char* result)`
*   **[MODIFY] APPEND Subsystem Internal Logic**
    *   Update `SetDirList`, `ShouldBypassAppend`, `CheckCandidate`, `ResolveName`, and `MultiplexHandler` to exclusively use the new semantic trace methods (e.g., calling `trace.Decision(...)` instead of `trace.Trace(...)`).

## Verification Plan
1. Re-run `build\debug-windows\tests\Debug\dosbox_tests.exe --gtest_filter=DOS_AppendTest.Initialization > tests\logging\log.txt`.
2. Verify that the output strictly follows the tree format and contains zero Loguru `INFO|` noise.
3. Validate that test execution times (`Duration: x.xxx ms`) are correctly calculated.

