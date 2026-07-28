# Incremental Commit Strategy for the APPEND Feature

To ensure a clean, easily bisectable pull request that satisfies the "checked that all my commits can be built" requirement, it is best to split the `APPEND` implementation into logical chunks when porting it over to a fresh fork. 

This guide details the strategy to safely break up the work into 4 independent commits that can each be built and verified sequentially.

---

### Commit 1: Core APPEND State Management & Multiplex API
**Goal:** Introduce the underlying state storage (the directory list), initialization, and the INT 2Fh multiplexer API without hooking it into the active filesystem yet.
**Files to add:**
- `src/dos/dos_append.h` (New file)
- `src/dos/dos_append.cpp` (New file)
- `tests/dos_append_tests.cpp` (New file - you can include the state and multiplex tests here, but comment out or omit the `ResolveName` / `DOS_OpenFile` tests for now so it compiles and passes).
- `tests/CMakeLists.txt` (Update to include `dos_append_tests.cpp`)

**Verification:** Build the project (`cmake --build build --target dosbox_tests`). Run `dosbox_tests.exe` to ensure the state management unit tests pass.

---

### Commit 2: Command-Line Parser and Registration
**Goal:** Introduce the `APPEND.COM` program logic to parse user input from the shell and update the state using the API from Commit 1.
**Files to add:**
- `src/dos/programs/append.h` (New file)
- `src/dos/programs/append.cpp` (New file)

**Verification:** Build the project (`cmake --build build --target dosbox-staging`). You can launch DOSBox Staging and type `APPEND` to ensure the command is recognized by the shell and responds to input.

---

### Commit 3: Emulated Filesystem Integration (The Hook)
**Goal:** Connect the `APPEND` state to the emulator's core file-opening operations so that missing files trigger a search through the APPEND directories.
**Files to add:**
- `src/dos/dos_files.cpp` (Modified to include the `dos_append::ResolveName` hook inside `DOS_OpenFile`)
- `tests/dos_append_tests.cpp` (Uncomment or add the remaining test cases that validate the `DOS_OpenFile` behavioral hooks).

**Verification:** Build the project and run the full test suite (`cmake --build build --target dosbox_tests`). All 26 `DosAppendTest` test cases should now compile and pass. Launch the emulator and manually verify that navigating to `C:\` and running a program in another directory via `APPEND` works.

---

### Commit 4: Website Documentation
**Goal:** Document the newly exposed command for end-users on the website.
**Files to add:**
- `website/docs/0.83/manual/using-dosbox-staging/commands.md` (Modified to include `APPEND` in the table).

**Verification:** Run `mkdocs build` or `mkdocs serve` within the `website/` directory to ensure the markdown renders cleanly in the manual without syntax warnings.

---

### Suggested Git Workflow for the Fresh Fork
1. Clone your fresh fork locally and create a new branch: `git checkout -b feature/native-append`
2. Copy the modified/new files over from this workspace into the fresh clone.
3. Use `git add -p` (patch mode) or stage files individually matching the groups above.
4. Run your build commands.
5. Commit using a clear message (e.g. `git commit -m "Add core dos_append state management and multiplex handler"`).
6. Repeat for Commits 2, 3, and 4.
7. Push to your fork: `git push -u origin feature/native-append`
