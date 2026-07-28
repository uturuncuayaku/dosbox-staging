# Loguru Logging Framework

DOSBox-Staging uses **Loguru** for robust, thread-safe logging. 

## 1. The Wrapper (logging.h)
Do not use raw Loguru macros directly for standard emulator logging. 
*   **DO:** Include `src/misc/logging.h` and use the DOSBox-Staging wrappers: `LOG_MSG`, `LOG_DEBUG`, `LOG_WARN`, `LOG_ERROR`.
*   These wrappers automatically inject ANSI escape codes (e.g., making `LOG_DEBUG` bold green in the terminal).

## 2. Dynamic Preamble Configuration
Every log line has a hardcoded preamble: `YYYY-MM-DD HH:MM:SS.mmm ( uptime ) [ thread id ] file:line VERBOSITY| message`.
While you cannot arbitrarily reorder these fields, you can dynamically toggle them on/off at runtime using global flags defined in `loguru.hpp`:
```cpp
loguru::g_preamble_date
loguru::g_preamble_time
loguru::g_preamble_uptime
loguru::g_preamble_thread
loguru::g_preamble_file
loguru::g_preamble_verbose
loguru::g_preamble_pipe
```

### Temporary Overrides
To strip noise (like timestamps and thread IDs) during specific test runs or localized trace blocks:
```cpp
bool old_date = loguru::g_preamble_date;
loguru::g_preamble_date = false;

LOG_DEBUG("Clean log without a date");

loguru::g_preamble_date = old_date; // Restore
```

## 3. Disabling the Preamble Entirely
To print a clean visual border (`=====`) without any prefix, toggle the master preamble flag:
```cpp
loguru::g_preamble = false;
LOG_DEBUG("==================================");
loguru::g_preamble = true;
```
