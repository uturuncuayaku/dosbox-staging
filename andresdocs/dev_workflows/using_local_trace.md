# Using Your Local Untracked Trace System

Now that you have a fully untracked trace implementation in `scripts/cpp_scripts/state_trace.hpp`, here is how you can practically use it to trace `dos_append.cpp` (or adapt it for any other subsystem you are debugging).

## 1. Preparing the Target File (`dos_append.cpp`)

Because we removed the class declaration from `dos_append.h`, the compiler doesn't know what a `StateTrace` is by default. 

To use it in `dos_append.cpp`, you must include your untracked `.hpp` file **at the top of the file**, right after your standard `#include` headers, so the compiler sees the class definition *before* your functions.

**Example at the top of `dos_append.cpp`:**
```cpp
#include "dos_append.h"
#include <string>
#include "misc/logging.h"

// --- ADD THIS TO USE YOUR TRACE ---
#if __has_include("../../scripts/cpp_scripts/state_trace.hpp")
#include "../../scripts/cpp_scripts/state_trace.hpp"
#endif
// ----------------------------------

namespace dos_append {
// ... rest of the code ...
```

*(Note: If you just want it to compile but not use the trace inside functions, you can leave this `#if __has_include` check at the bottom of the file instead of the top.)*

## 2. Implementing the Logging (in `state_trace.hpp`)

Right now, your `.hpp` file just has empty function bodies. You'll want to wire them up to `LOG_MSG` (DOSBox Staging's logging macro) or simply `std::cout` so you can see the output.

Here is an example of how you might fill out the implementations inside `scripts/cpp_scripts/state_trace.hpp`:

```cpp
#include "misc/logging.h" // Needed for LOG_MSG

StateTrace::StateTrace(const char* func, const char* rationale)
        : func_name(func), rationale(rationale) {
    LOG_MSG("ENTER: %s() - %s", func_name.c_str(), rationale.c_str());
    depth++;
}

StateTrace::~StateTrace() {
    depth--;
    LOG_MSG("EXIT:  %s()", func_name.c_str());
}

void StateTrace::State(const char* key, const std::string& value) {
    LOG_MSG("  [%s] STATE: %s = %s", func_name.c_str(), key, value.c_str());
}

void StateTrace::Decision(const char* choice, const char* reason) {
    LOG_MSG("  [%s] DECISION: %s (Reason: %s)", func_name.c_str(), choice, reason);
}
// ... implement the others similarly ...
```

## 3. Tracing Your Code

To trace a function, instantiate your trace scope at the very top of the function. Because it's a C++ class with a destructor, it will automatically log when the function exits (even if there are early `return` statements!).

**Example inside `dos_append.cpp`:**
```cpp
bool ResolveName(const char* name, std::string& out_path)
{
    // 1. Start the trace (logs "ENTER: ResolveName...")
    StateTrace trace("ResolveName", "Trying to find requested file");

    if (!enabled) {
        trace.Decision("Abort", "APPEND is not enabled");
        return false; // Automatically logs "EXIT: ResolveName"
    }

    auto basename = extract_basename(name);
    trace.State("Basename", basename);

    // ... rest of your logic ...
    
    trace.Result("Success! File found.");
    return true;
}
```

## 4. Reusing for Other Subsystems

Because `scripts/cpp_scripts/state_trace.hpp` is ignored by Git, you can reuse this anywhere. If you move on to debugging the CD-ROM subsystem, you can:
1. Add `#if __has_include("../../scripts/cpp_scripts/state_trace.hpp") ... #endif` to the top of `cdrom_image.cpp` (adjusting the `../` relative path as needed depending on where the `.cpp` file is located).
2. Put `StateTrace trace("CDROM_Read", "Reading sector");` inside the CD-ROM code.

## ⚠️ Crucial Git Reminder

Because we removed the dummy fallback block from the shared headers to keep them clean, **your trace code will not compile for anyone else**. 

Before you commit and push your PR, you **MUST** ensure you revert or do not stage:
1. The `#if __has_include(...)` block in the `.cpp` file.
2. Any `StateTrace trace(...)` lines inside your functions.

If you use `git add -p` (patch staging) or a visual Git GUI like VS Code's Source Control tab, you can easily select the valid bug fixes to commit while leaving your trace lines safely unstaged on your local machine!
