<!-- Created by Antigravity -->
# Walkthrough: Append Testing & Logging

## Changes Made
- Modified `tests/dos_append_tests.cpp` to remove usage of the core `LOG_MSG` and `LOG_DEBUG` macros.
- Replaced the core logging with `std::cout`, which uses the standard C++ output stream. This way we can log custom test messages during the execution of the test suite without affecting or relying on the core codebase logging systems (`misc/logging.h`).

## Running The Tests
When running the `APPEND` unit tests, `ctest` runs the unified test executable `dosbox-staging-tests.exe` and passes the matched test names to it.

```powershell
ctest --preset debug-windows -R "DOS_Append" -V
```
By adding the `-V` (Verbose) flag, any `std::cout` statements you add in your tests will immediately be printed to your terminal.

## About `log.txt`
The `log.txt` file in `tests/logging/` acts as a static text file capturing output from previous test runs.
**`ctest` does not automatically update `log.txt`.** It executes the test binary behind the scenes and prints the results directly to your console. 

If you want `ctest` to specifically save the output of your test run into `log.txt`, you can redirect its console output using standard terminal redirection like this:

```powershell
ctest --preset debug-windows -R "DOS_Append" -V > tests/logging/log.txt
```
