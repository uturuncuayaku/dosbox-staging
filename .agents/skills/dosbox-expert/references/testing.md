# DOSBox-Staging Testing Guidelines

When writing or modifying unit tests in DOSBox-Staging, follow these strict guidelines based on the project's Google Test (GTest) framework implementation.

## 1. Observable Behavior Over Implementation Details
*   **DO NOT** assert on internal implementation details. For example, never assert on the value of internal private boolean flags like `currently_resolving` or `is_active`.
*   **DO** test observable behavior and edge cases. If a feature is disabled, test that its public API returns the expected failure code.

## 2. Test Categorization
Structure your test suites into logical categories:
*   **State Management:** Test initialization, activation, deactivation, and consecutive state replacements.
*   **Parser and Normalization:** Test how the subsystem handles trailing separators, multiple delimiters, empty clears, duplicates, and case sensitivity.
*   **Path Resolution:** Ensure absolute vs relative paths are handled correctly, and ordering (First Match Wins) is respected.

## 3. Integration & Behavioral Tests
When testing subsystem hooks (e.g., intercepting `DOS_OpenFile`):
*   Verify the core feature flow (failure -> retry -> success).
*   **Error Preservation:** Ensure that if a fallback or intercept fails, the subsystem returns the *original* error code from the primary operation, not a generic error.
*   **Negative Tests:** Explicitly write tests for inputs that should completely bypass your hook (e.g., providing an absolute path `C:\DIR\FILE.TXT` to a hook that only works on bare filenames).
