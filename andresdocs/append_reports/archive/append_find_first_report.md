> [!NOTE]
> **Historical snapshot - may not match current implementation.** See [append_consolidated_report.md](../append_consolidated_report.md) for the current authoritative specification.

# The Complexity of Find First / Find Next (`/X:ON`)

This report details how MS-DOS natively handles directory merging when `APPEND /X:ON` is active, focusing on the DOS kernel resources, the `append.asm` hook, and the Disk Transfer Area (DTA).

## 1. How MS-DOS Directory Searching Works

When a program wants to list files (like the `DIR` command), it uses two DOS interrupts:
1. **Find First (INT 21h, AH=4Eh):** The program provides a file mask (e.g., `*.TXT`). DOS finds the first match and populates the **Disk Transfer Area (DTA)**.
2. **Find Next (INT 21h, AH=4Fh):** The program calls this repeatedly to get subsequent files. DOS knows where to resume the search solely by reading the "state" it left behind in the DTA during the previous call.

### The DTA Structure (43 Bytes Total)
The DOS kernel populates a 43-byte structure in the program's DTA. The first 21 bytes are strictly reserved for DOS internal state:
* `Byte 0` (1 byte): Search drive letter.
* `Bytes 1-11` (11 bytes): The 8.3 search mask (e.g., `*       TXT`).
* `Bytes 12-16` (5 bytes): Directory entry cluster/sector pointers.
* `Bytes 17-20` (4 bytes): Parent directory pointers and attributes.
* `Bytes 21-42` (22 bytes): The actual file results (Name, Size, Date, Time) shown to the user.

## 2. The `append.asm` Implementation Challenge

When `APPEND /X:ON` is active, it hooks INT 21h. If a program calls `Find First` (`AH=4Eh`) and the file is not found in the current directory, `APPEND` intercepts the `FILE_NOT_FOUND` error.
It then replaces the search path with the first directory in the APPEND list and calls the DOS kernel again. 

### The "Find Next" State Problem
If a match is found in `C:\APPEND_DIR_1`, DOS populates the DTA and returns success to the program. 
When the program calls `Find Next` (`AH=4Fh`), DOS will continue searching `C:\APPEND_DIR_1`. Eventually, that directory will run out of `*.TXT` files. DOS will return `FILE_NOT_FOUND` to the program.

To merge directories properly, `APPEND` must intercept this `FILE_NOT_FOUND` from `Find Next`, realize that it just finished `C:\APPEND_DIR_1`, seamlessly switch the search path to `C:\APPEND_DIR_2`, and perform a brand new `Find First` under the hoodâ€”while pretending to the program that it was just a normal `Find Next`!

### Where does `append.asm` store the directory index?
To do this, `APPEND` needs to know *which* directory in the APPEND list the current DTA belongs to. 

**Why a global variable won't work (Interleaved Searches):**
Imagine a complex program (like a file manager or a compiler) that wants to find all `.TXT` files, and for every `.TXT` file it finds, it searches for a corresponding `.BAK` file. 
1. The program calls `Find First (*.TXT)` and DOS sets up its state in **DTA A**.
2. It finds a text file. Now, while still in the middle of that search, it points DOS to a new memory address (**DTA B**) and calls `Find First (*.BAK)`. 
3. It loops through `Find Next` using **DTA B**.
4. Once finished, it switches back to **DTA A** and calls `Find Next` to get the next `.TXT` file.

Because `Find Next` calls for DTA A and DTA B are interleaved, `APPEND` cannot use a simple global `current_append_directory` variable. If it did, DTA A would accidentally resume searching in whatever directory DTA B left off in! 

Therefore, `APPEND` *must* store the "Append Path Index" (the number tracking which directory it is currently iterating through) **inside the DTA itself**, so that every unique search has its own independent tracker.

But the 21-byte reserved area of the DTA is completely full of critical DOS kernel pointers! 
In the authentic MS-DOS `append.asm`, Microsoft used extremely dangerous undocumented hacks:
1. **DTA Hijacking:** `append.asm` steals unused bits or overwrites the "search drive letter" byte in the DTA to store an index offset. 
2. **Global State Arrays:** In later versions, it maintained an internal fixed-size array mapping active DTA memory addresses to APPEND directory indices. If a program moved its DTA memory or did too many simultaneous searches, `APPEND` would crash or return incorrect files.

## 3. Implementing this in DOSBox-Staging

In DOSBox-Staging, the DOS kernel (`dos_files.cpp`) handles `DOS_FindFirst` and `DOS_FindNext` by populating a `DOS_DTA` class object. 
To implement `/X:ON` authentically, we would need to:
1. **Hook `DOS_FindFirst`:** Try the original path. If it fails, iterate through the APPEND paths until a match is found. 
2. **Store State:** We must inject a custom field into the `DOS_DTA` class (e.g., `uint8_t append_index`) to remember which APPEND directory this DTA is currently searching.
3. **Hook `DOS_FindNext`:** If `DOS_FindNext` returns false (end of directory), check the DTA's `append_index`. If it's valid, increment the index, pick the next APPEND directory, and seamlessly call `DOS_FindFirst` on that new directory using the original search mask, returning true to the program.

### Conclusion
Implementing `EXEC` hooking for `/X:ON` is very straightforward (just resolving the `.EXE`/`.COM` path before execution). However, merging directory searches requires deep modifications to the DOS kernel's `DOS_DTA` class and Find First/Next state machine. While entirely possible, it is significantly more complex than standard file hooking.

