> [!NOTE]
> **Historical snapshot - may not match current implementation.** See [append_consolidated_report.md](../append_consolidated_report.md) for the current authoritative specification.

# MS-DOS APPEND Phase 2: Technical Walkthrough & Architecture Report (/PATH Focus)

This report details the implementation of `/PATH:ON` and `/PATH:OFF` functionality within the MS-DOS `APPEND` engine for DOSBox-Staging.

---

## 1. Runtime File Directory Resolution

Runtime file resolution intercepts file system API calls (such as `DOS_OpenFile`, `DOS_GetFileAttr`, and `DOS_Execute`) to resolve missing files across appended directories.

### End-to-End Execution Trace of `out_path`

```
      [DOS_OpenFile()] in dos_files.cpp
             â”‚
 1. Define   â”‚  std::string resolved = {};  (Allocated on caller's stack)
             â”‚
 2. Pass     â”‚  dos_append::find_absolute_path("HELP.TXT", resolved);
             â–¼
      [dos_append::find_absolute_path()] in dos_append.cpp
             â”‚
             â”‚  bool find_absolute_path(const char* name, std::string& out_path)
             â”‚                                         â””â”€â”€â”€â”€â”€â”€â”¬â”€â”€â”€â”€â”€â”€â”˜
             â”‚                          Alias to 'resolved' on caller's stack!
             â”‚
 3. Loop     â”‚  for (const auto& subrange : sv | std::views::split(';')) {
             â”‚      find(dir, filename, out_path);
             â–¼
      [find()] in dos_append.cpp
             â”‚
             â”‚  auto candidate = "C:\DATA\HELP.TXT";
             â”‚  if (DOS_FileExists(candidate)) {
 4. Assign   â”‚      out_path = candidate;   (Populates 'resolved' in DOS_OpenFile!)
             â”‚      return true;
             â”‚  }
             â–¼
 5. Consume  [DOS_OpenFile()] receives true and opens 'resolved' ("C:\DATA\HELP.TXT")!
```

---

### The Main Resolution Engine (`dos_append::find_absolute_path` in [dos_append.cpp](file:///c:/Users/andtr/Documents/GitHub/dosbox-staging-attempt3/dosbox-staging/src/dos/dos_append.cpp))

```cpp
bool find_absolute_path(const char* name, std::string& out_path)
{
	if (!enabled || currently_resolving) {
		return false;
	}

	if (should_skip_append_resolution(name)) {
		return false;
	}

	const auto filename = extract_filename(name);
	if (filename.empty()) {
		return false;
	}

	currently_resolving = true;
	std::string current_list = GetDirList();
	std::string_view sv      = current_list;

	// C++20 Range Pipe Operator 'sv | std::views::split(';')' performs zero-copy slicing
	for (const auto& subrange : sv | std::views::split(';')) {
		std::string_view dir(subrange.begin(), subrange.end());
		if (dir.empty()) continue;

		if (find(dir, filename, out_path)) {
			currently_resolving = false;
			return true;
		}
	}

	currently_resolving = false;
	return false;
}
```

---

## 2. Type-Safe Executable Architecture ([append.cpp](file:///c:/Users/andtr/Documents/GitHub/dosbox-staging-attempt3/dosbox-staging/src/dos/programs/append.cpp))

To strictly enforce DOSBox-Staging's maintainer rules (**all functions < 40 lines**) and provide **compile-time type safety**, `APPEND` uses `std::optional<std::string>` for path validation:

### Type-Safe Validator (`ParseAndValidateDirectories` in [append.cpp](file:///c:/Users/andtr/Documents/GitHub/dosbox-staging-attempt3/dosbox-staging/src/dos/programs/append.cpp#L78))

```cpp
// Returns std::nullopt if validation fails, preventing unvalidated path commitment.
std::optional<std::string> APPEND::ParseAndValidateDirectories(std::string_view args)
{
	std::string cleaned_paths = {};
	cleaned_paths.reserve(DOS_PATHLENGTH);

	size_t start = 0;
	while (start < args.size()) {
		size_t end = args.find(';', start);
		std::string_view token = args.substr(start, (end == std::string_view::npos) ? std::string_view::npos : end - start);

		size_t first = token.find_first_not_of(" \"");
		if (first != std::string_view::npos) {
			size_t last = token.find_last_not_of(" \"");
			token       = token.substr(first, last - first + 1);

			if (!token.empty()) {
				std::string token_str(token);
				char fullname[DOS_PATHLENGTH];
				uint8_t drive;
				if (!DOS_MakeName(token_str.c_str(), fullname, &drive) ||
				    !Drives[drive] || !Drives[drive]->TestDir(fullname)) {
					WriteOut(MSG_Get("PROGRAM_APPEND_INVALID_PATH"));
					return std::nullopt; // Validation failed -> return nullopt!
				}

				std::string final_path = std::string(1, 'A' + drive) + ":\\";
				if (fullname[0] == '\\') {
					final_path += (fullname + 1);
				} else {
					final_path += fullname;
				}

				if (!cleaned_paths.empty()) {
					cleaned_paths += ';';
				}
				cleaned_paths += final_path;
			}
		}

		if (end == std::string_view::npos) break;
		start = end + 1;
	}
	return cleaned_paths;
}
```

### Main Entrypoint (`APPEND::Run()` â€” 35 Lines)

```cpp
void APPEND::Run()
{
	if (HelpRequested()) {
		WriteOut(MSG_Get("PROGRAM_APPEND_HELP_LONG"));
		return;
	}

	const AppendSwitches sw = parse_switches(cmd);

	std::string args_raw;
	cmd->GetStringRemain(args_raw);
	const std::string_view args = trim_leading(args_raw);

	const CommandMode mode = determine_command_mode(args, sw.changed());

	switch (mode) {
	case CommandMode::ClearList:
		apply_switch_updates(sw);
		dos_append::SetDirList("");
		return;

	case CommandMode::ShowCurrentState:
		ShowCurrentState();
		return;

	case CommandMode::UpdateFlagsOnly:
		apply_switch_updates(sw);
		return;

	case CommandMode::ParseDirectories:
		break;
	}

	// Type-Safe Dispatch: Cannot call CommitDirectoryList unless ParseAndValidateDirectories returns a valid string!
	const auto cleaned_paths = ParseAndValidateDirectories(args);
	if (!cleaned_paths.has_value()) {
		return;
	}

	apply_switch_updates(sw);
	CommitDirectoryList(*cleaned_paths);
}
```

---

## 3. Unit Testing Coverage & QA Matrix

In [dos_append_tests.cpp](file:///c:/Users/andtr/Documents/GitHub/dosbox-staging-attempt3/dosbox-staging/tests/dos_append_tests.cpp), `/PATH` functionality, fuzzing geometries, and QA test matrices are thoroughly verified:

```cpp
TEST_F(DosAppendTest, QAComprehensiveTestMatrix)
{
	DOS_MakeDir("C:\\DATA");
	DOS_MakeDir("C:\\MORE");

	// 1. Whitespace & Quote Trimming (using modern C++ raw string literals R"(...)")
	{
		auto* cmd = new CommandLine("APPEND", R"(  "C:\DATA"  ;  C:\MORE  )");
		APPEND prog;
		prog.cmd = cmd;
		prog.Run();
		EXPECT_EQ(dos_append::GetDirList(), R"(C:\DATA;C:\MORE)");
	}

	// 2. Trailing and Leading Semicolons
	{
		auto* cmd = new CommandLine("APPEND", R"(;C:\DATA;)");
		APPEND prog;
		prog.cmd = cmd;
		prog.Run();
		EXPECT_EQ(dos_append::GetDirList(), R"(C:\DATA)");
	}

	// 3. Invalid Path Abort (Atomicity Check)
	{
		dos_append::SetDirList(R"(C:\DATA)");
		auto* cmd = new CommandLine("APPEND", R"(C:\DATA;Z:\NONEXISTENT)");
		APPEND prog;
		prog.cmd = cmd;
		prog.Run();
		// Should fail validation and keep original dir list intact
		EXPECT_EQ(dos_append::GetDirList(), R"(C:\DATA)");
	}
}
```

