# Local Untracked Code Workflows

When developing or debugging complex features (like the `AppendTraceScope` in `dos_append.cpp`), you often want to maintain local, temporary, or messy debugging code on your machine without committing it or polluting the main branch in Pull Requests. 

There are two primary approaches to handle this in C/C++ projects, especially when modifying shared headers.

## The Problem: Linker Errors
If a class or function is declared in a shared header (e.g. `class AppendTraceScope`), the compiler leaves a placeholder for its implementation. If the implementation is stored in a purely local, untracked file, *your* project builds fine. However, when other developers pull the PR and compile in Debug mode, their compilers will find the declaration but their linkers will crash ("Unresolved external symbol"), because the implementation file does not exist on their machines.

To prevent this, the *declaration* itself must be hidden from other developers, or we must provide dummy inline implementations for them to fall back on.

---

## Approach 1: The `__has_include` Trick (Recommended)

This approach leverages the modern C++ `__has_include` preprocessor directive, which tests if a file exists on the developer's local filesystem before attempting to include it.

### How It Works
1. Create your local implementation file (e.g., `src/dos/dos_append_trace.inc`).
2. Add that file to your local `.git/info/exclude` file (so Git ignores it locally without modifying the shared `.gitignore`).
3. In the shared header (`dos_append.h`), wrap the class declaration with a check:
   ```cpp
   #if !defined(NDEBUG) && __has_include("dos_append_trace.inc")
   // Your real class declaration
   #else
   // Dummy inline implementations for everyone else
   #endif
   ```
4. In the shared `.cpp` file (`dos_append.cpp`), include the file dynamically:
   ```cpp
   #if __has_include("dos_append_trace.inc")
   #include "dos_append_trace.inc"
   #endif
   ```

### Pros
- **Zero friction for others:** If another developer pulls your PR, `__has_include` returns false, and the code seamlessly falls back to dummy inline functions. No linker errors.
- **Set and forget:** You can commit the `#if __has_include` blocks directly into the PR. They are tiny, unobtrusive, and you never have to actively stash or revert them before pushing.
- **Maintains local code:** Your implementation (`dos_append_trace.inc`) stays safe and untouched on your machine, ignored by Git.

### Cons
- **Pollutes the PR slightly:** The PR will contain a few lines referencing `dos_append_trace.inc`. Reviewers might ask what that file is.
- **Requires C++17:** `__has_include` is a C++17 feature (though DOSBox Staging targets modern C++, so this is fine).

---

## Approach 2: Local Preprocessor Macros (CMake flags)

This approach relies on passing a custom `-D` flag to your local compiler build step, avoiding *any* mention of the debug file in the main codebase if you carefully manage your commits.

### How It Works
1. Create your local implementation file (`src/dos/dos_append_trace.inc`) and exclude it via `.git/info/exclude`.
2. In your local IDE or CMake settings (e.g., `CMakeUserPresets.json`), pass a flag like `-DLOCAL_DEBUG_TRACE`.
3. Wrap your headers and includes using standard `#ifdef`:
   ```cpp
   #ifdef LOCAL_DEBUG_TRACE
   #include "dos_append_trace.inc"
   #endif
   ```
4. **Before committing/pushing:** Ensure you *do not* stage the `#ifdef` blocks in the main files.

### Pros
- **Completely invisible in PRs:** If managed correctly, there is zero trace of your debugging setup in the upstream codebase.
- **Standardized:** Works with any version of C/C++.

### Cons
- **High friction:** You must be extremely careful not to accidentally commit the `#ifdef` blocks. If you do, other developers won't have `-DLOCAL_DEBUG_TRACE` and might still face issues if the `#else` fallback logic isn't perfect.
- **Constant Git juggling:** You will constantly see `dos_append.cpp` and `dos_append.h` as "modified" in your `git status` because of the `#ifdef` lines. You must use `git stash` or interactive staging (`git add -p`) every single time you commit to avoid leaking it.

## Conclusion
For DOSBox Staging, **Approach 1 (`__has_include`)** is heavily recommended. It aligns with modern C++ practices, completely eliminates the risk of breaking the build for other developers (like the linker error caveat), and saves you from the headache of juggling `git stash` every time you want to commit.
