# Defensive C++23 Programming

The DOSBox-Staging project uses modern C++23. Under no circumstances should legacy C-style patterns be used if a safer C++ alternative exists.

## 1. File Path Extraction (No Pointer Arithmetic)
Do not use `strrchr` or manual pointer arithmetic to extract filenames from absolute paths. This leads to off-by-one buffer overruns.
*   **DO:** Convert to `std::string_view` or `std::string`.
*   **DO:** Use `.find_last_of("\\/")` to safely locate path separators.
*   **DO:** Use C++20 `std::views::split` for tokenizing semicolon-separated lists.

## 2. Strict Immutability
When passing resolved strings (like executable paths) into internal emulator functions (like `MakeEnv`), explicitly enforce `const char*` or `std::string_view` parameters.
*   Do not accept `char*` if the function does not need to mutate the string.
*   This guarantees that memory cannot be accidentally corrupted or truncated during process setup.

## 3. State Management (RAII)
Never manually manage boolean state flags, system locks, or resources that must be reset at the end of a function scope. If an early return or exception occurs, manual toggles will fail and leave the emulator in a corrupted state.
*   **DO NOT:** Manually toggle states (e.g., `flag = true; ... return; ... flag = false;`).
*   **DO:** Use the **RAII** (Resource Acquisition Is Initialization) pattern. 
*   Use a local struct or class whose constructor acquires the state/resource and whose destructor guarantees cleanup when it goes out of scope, ensuring complete safety even with early returns.

## 4. Transactional Atomicity (Parsers)
When writing shell commands (e.g., parsing `APPEND /X:ON =C:\Dir`), ensure the global state is not corrupted by a malformed command.
*   **DO:** Extract all flags destructively from the ephemeral `CommandLine` object using `cmd->FindExistRemoveAll()`.
*   **DO:** Perform all structural validation (e.g., testing directory existence via `TestDir`) first.
*   **DO:** Mutate the global state (e.g., `dos_append::SetFlags()`) at the absolute bottom of the `Run()` function. If the parser hits an early `return;` due to invalid input, it must abort safely without leaving the system in a partially-applied state.
