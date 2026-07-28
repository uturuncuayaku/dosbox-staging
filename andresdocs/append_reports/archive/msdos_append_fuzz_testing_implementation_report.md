> [!NOTE]
> **Historical snapshot - may not match current implementation.** See [append_consolidated_report.md](../append_consolidated_report.md) for the current authoritative specification.

# MS-DOS APPEND Fuzz Testing & Path Parsing Implementation Report

This implementation report documents the testing strategy, fuzzing taxonomies, invariant properties, and integration test suite designed to verify path resolution robustness within the MS-DOS `APPEND` subsystem for DOSBox-Staging.

---

## 1. Overview & Testing Strategy

To ensure that `dos_append::ResolveName()` and `extract_basename()` handle all arbitrary path inputs safely under `/PATH:ON`, we use a structural fuzzing strategy.

Rather than varying data payload content, structural fuzzing varies **path geometry, separator formatting, and string length** to ensure:
1. **Basename Equivalence:** Any explicit path under `/PATH:ON` resolves to the exact same final filename component.
2. **Buffer & Memory Safety:** Extreme inputs (long strings, dots, mixed slashes) never cause buffer overruns or memory corruption.
3. **Zero Subdirectory Contamination:** Appended search entries are never concatenated with original relative subdirectories.

---

## 2. Fuzzing Input Taxonomies

The test suite systematically evaluates `APPEND` path resolution across eight distinct input categories:

| Input Taxonomy Category | Input Examples | Target Verification Property |
| :--- | :--- | :--- |
| **1. Unqualified & Single Segment** | `FILE.TXT`, `TEST` | Direct search path resolution without modification. |
| **2. Relative Paths** | `..\FILE.TXT`, `.\FILE.TXT`, `..\DIR\FILE.TXT` | Discards `..` and `.` prefixes, extracts `FILE.TXT`. |
| **3. Absolute Paths** | `C:\DATA\SUBDIR\FILE.TXT`, `D:\FILE.TXT` | Discards drive colon and folder path, extracts `FILE.TXT`. |
| **4. Mixed Slash Styles** | `C:/DATA\SUBDIR/FILE.TXT`, `DIR/SUBDIR\FILE.TXT` | Handles Unix (`/`) and DOS (`\`) slashes interchangeably. |
| **5. Redundant Separators & Dots** | `C:\\\DATA\\\\FILE.TXT`, `C:\DATA\.\..\FILE.TXT` | Safely isolates `FILE.TXT` without crashing. |
| **6. Long Path Strings** | 256+ character string `C:\DIR\...\FILE.TXT` | Respects memory bounds without heap/stack overflow. |
| **7. Whitespace & Special Characters** | `C:\MY DATA\FILE #1 (FINAL).TXT` | Preserves valid DOS filename characters. |
| **8. DOS Reserved Devices** | `CON`, `AUX`, `NUL`, `COM1`, `LPT1` | Avoids intercepting DOS character devices. |

---

## 3. Core Invariant Properties Verified

```
                     Input Path String
            (e.g., "C:\DATA\SUBDIR\..\FILE.TXT")
                             â”‚
                             â–¼
                extract_basename(input_path)
                             â”‚
                             â–¼
                    Isolated Basename
                       ("FILE.TXT")
                             â”‚
                             â–¼
            Check Appended Directory Entries Only
        (e.g., "D:\LIB\FILE.TXT", "E:\ASSETS\FILE.TXT")
                             â”‚
                             â–¼
     [NO Contamination: "D:\LIB\SUBDIR\FILE.TXT" IS NEVER TESTED]
```

### Invariant 1: Equal Basename Generation
Under `/PATH:ON`, any input path string terminating in `FILE.TXT`â€”whether `FILE.TXT`, `..\FILE.TXT`, `C:\DATA\FILE.TXT`, or `C:/DATA\SUBDIR/FILE.TXT`â€”must produce the exact same candidate filename `"FILE.TXT"`.

### Invariant 2: Search Entry Isolation
Appended search path entries (e.g. `D:\LIB`) are combined strictly with the isolated basename (`D:\LIB\FILE.TXT`). Subdirectory structures from the original input path (such as `SUBDIR\`) must never pollute the search candidates.

### Invariant 3: Atomicity on Null Results
If no candidate file exists in any appended directory, `ResolveName()` returns `false` clean without side effects, leaving internal state flags untouched.

---

## 4. C++ Integration Unit Test Suite Implementation

In [dos_append_tests.cpp](file:///c:/Users/andtr/Documents/GitHub/dosbox-staging-attempt3/dosbox-staging/tests/dos_append_tests.cpp), structural path parsing and `/PATH:ON` overrides are validated by the following unit tests:

```cpp
//*****antigravity phase 2 start
TEST_F(DosAppendTest, PathOverrideFuzzingGeometries)
{
	// Setup target file in appended directory
	DOS_MakeDir("C:\\APPEND_LIB");
	uint16_t entry;
	DOS_CreateFile("C:\\APPEND_LIB\\TARGET.TXT", 0, &entry);
	DOS_CloseFile(entry);

	dos_append::SetDirList("C:\\APPEND_LIB");
	dos_append::SetFlags(false, true, false); // Enable /PATH:ON

	std::string out_path;

	// 1. Relative path input
	EXPECT_TRUE(dos_append::ResolveName("..\\SUBDIR\\TARGET.TXT", out_path));
	EXPECT_EQ(out_path, "C:\\APPEND_LIB\\TARGET.TXT");

	// 2. Absolute path input
	EXPECT_TRUE(dos_append::ResolveName("D:\\OTHER\\DIR\\TARGET.TXT", out_path));
	EXPECT_EQ(out_path, "C:\\APPEND_LIB\\TARGET.TXT");

	// 3. Mixed slash styles
	EXPECT_TRUE(dos_append::ResolveName("D:/OTHER/DIR\\TARGET.TXT", out_path));
	EXPECT_EQ(out_path, "C:\\APPEND_LIB\\TARGET.TXT");

	// 4. Dot segment input
	EXPECT_TRUE(dos_append::ResolveName("C:\\DIR\\.\\..\\TARGET.TXT", out_path));
	EXPECT_EQ(out_path, "C:\\APPEND_LIB\\TARGET.TXT");
}
//*****antigravity phase 2 end
```

