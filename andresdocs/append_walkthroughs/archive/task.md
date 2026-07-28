> [!NOTE]
> **Historical snapshot - may not match current implementation.** See [append_consolidated_report.md](../append_consolidated_report.md) for the current authoritative specification.

- `[x]` Update `src/dos/dos_append.h` to declare the new semantic `AppendTraceScope` class.
- `[x]` Update `src/dos/dos_append.cpp` to implement `AppendTraceScope` with ID tracking, scope timing, and depth indentation.
- `[x]` Refactor `AppendTraceScope` usage across `src/dos/dos_append.cpp` to use `State()`, `Action()`, `Decision()`, `Result()`, and `Error()`.
- `[/]` Verify unit tests pass and capture visual formatting.

