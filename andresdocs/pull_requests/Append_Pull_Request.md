# Description

Implemented the classic MS-DOS `APPEND` command, enabling DOS applications to locate data files outside their current working directory using a pre-configured search path. 

**Summary of accomplishments step-by-step:**
1. **Core Implementation (`src/dos/dos_append.cpp`)**: Built the backing state management for the directory search list (`dos_append::SetDirList`, `dos_append::GetDirList`), allocating the list into a dedicated 256-byte emulated DOS memory block so that applications can query it directly.
2. **File Hooking (`DOS_OpenFile`)**: Intercepted the DOS file-opening subsystem to iterate through the `APPEND` search paths whenever a file is not found in the current directory, successfully bridging the emulator's filesystem with the `APPEND` state.
3. **Command Parser (`src/dos/programs/append.cpp`)**: Re-implemented the MS-DOS 4.0-style `SYSPARSE` parser, which fully handles trailing separators, invalid paths, and whitespace/quoted strings inside the directory list arguments. Ignored standard switches like `/X` and `/E` silently to maximize compatibility with automated scripts.
4. **Multiplex Interrupt Handling (`INT 2Fh`)**: Registered `APPEND` as an interrupt multiplexer (`AX=B700h`), mimicking the MS-DOS 4.0 API to satisfy games and utilities that rely on `APPEND` installation and version checks.
5. **Testing & QA (`tests/dos_append_tests.cpp`)**: Created a robust Google Test suite consisting of 26 passing test cases. These tests rigorously validate state initialization, string parsing, path resolution ordering, file API hooks, and multiplex functionality.
6. **Code Cleanup**: Removed all trace and debug macro definitions (`AppendTraceScope`) for clean compilation, and resolved all C++ linting warnings across the modified files.
7. **Documentation (`website/`)**: Spliced the `APPEND` command into the DOSBox Staging `commands.md` manual for the MkDocs website generator.


# Release notes

Added support for the `APPEND` command, allowing MS-DOS applications to access data files in other directories as if they were in the current working directory. The implementation closely mimics the parsing and multiplex API behaviors found in MS-DOS 4.0, greatly improving out-of-the-box compatibility with complex batch scripts and multi-directory installations.


# Manual testing

**Testing the Command Line:**
1. Open DOSBox Staging and type `APPEND /?` to ensure help displays.
2. Type `APPEND C:\;C:\DOS` and then just `APPEND` to verify the state was saved properly.
3. Type `APPEND ;` to ensure the list clears.

**Testing Path Resolution:**
1. Create a `DATA` folder inside your mounted `C:\` drive.
2. Place a `TEST.TXT` file inside `C:\DATA`.
3. In DOSBox Staging, navigate to `C:\` and execute `APPEND C:\DATA`.
4. Run `TYPE TEST.TXT` from the root directory. DOSBox Staging should intercept the missing file, query the `APPEND` list, and successfully type the contents of `C:\DATA\TEST.TXT`.

The change has been manually tested on:

- [ ] Windows
- [ ] macOS
- [ ] Linux


# Checklist

- [ ] I am a human, I have read and understood the [project's policy on the use of generative AI tools](https://github.com/dosbox-staging/dosbox-staging/blob/master/docs/CONTRIBUTING.md#policy-on-the-use-of-generative-ai-tools), and I have acted in accordance to it.

I have:

- [ ] followed the project's [contributing guidelines](https://github.com/dosbox-staging/dosbox-staging/blob/master/docs/CONTRIBUTING.md) and [code of conduct](https://github.com/dosbox-staging/dosbox-staging/blob/master/docs/CODE_OF_CONDUCT.md).
- [ ] performed a self-review of my work (especially important for AI-assisted contributions).
- [ ] commented on the particularly hard-to-understand areas of my code.
- [ ] split my work into well-defined, bisectable commits, and I [named my commits well](https://github.com/dosbox-staging/dosbox-staging/blob/main/docs/CONTRIBUTING.md#commit-messages).
- [ ] applied the appropriate labels (bug, enhancement, refactoring, documentation, etc.)
- [ ] [checked](https://github.com/dosbox-staging/dosbox-staging/blob/main/scripts/tools/compile-commits.sh) that all my commits can be built.
- [ ] my change has been manually tested on Windows, macOS, and Linux.
- [ ] confirmed that my code does not cause performance regressions (e.g., by running the Quake benchmark).
- [ ] added unit tests where applicable to prove the correctness of my code and to avoid future regressions.
- [ ] provided the release notes draft (for significant user-facing changes).
- [ ] made corresponding changes to the documentation or the website according to the [documentation guidelines](https://github.com/dosbox-staging/dosbox-staging/blob/main/docs/DOCUMENTATION.md).
- [ ] [locally verified](https://github.com/dosbox-staging/dosbox-staging/blob/main/docs/DOCUMENTATION.md#previewing-documentation-changes-locally) my website or documentation changes.
