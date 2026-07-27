> [!NOTE]
> **Historical snapshot - may not match current implementation.** See [append_consolidated_report.md](../append_consolidated_report.md) for the current authoritative specification.

# MS-DOS 4.0 `APPEND.ASM` Analysis Report

This report analyzes the official Microsoft open-source release of MS-DOS 4.0's `APPEND.ASM` utility to establish a ground-truth reference for the DOSBox Staging APPEND implementation.

## 1. Multiplex Subfunctions (`INT 2Fh`, `AH = B7h`)

When a program triggers `INT 2Fh` with `AH = B7h`, it is communicating directly with APPEND. The specific command is passed in `AL`.

### Multiplex Subfunctions Table

| Subfunction (`AL`) | Name | MS-DOS 4.0 Behavior | Implementation Notes for DOSBox |
| :--- | :--- | :--- | :--- |
| **`00h`** | `are_you_there` | Returns `AL = 0xFF` if resident. | **Implemented.** Returns `0xFF`. |
| **`01h`** | `old_dir_ptr` | Aborts APPEND (legacy APPEND 1.0 check). | **Not Implemented.** (Falls through to stub). |
| **`02h`** | `get_app_version`| Returns `AX = FFFFh`. | **Requires Fix.** We currently return `dos.version`. We must change this to return `FFFFh`. This is a legacy check telling callers "I am MS-DOS APPEND, not the older IBM PC Network APPEND." It does not provide the detailed DOS version. |
| **`03h`** | `tv_vector` | Synchronizes with IBM TopView. | **Not Implemented.** TopView was an IBM text-mode multitasking environment from 1985 (a precursor to Windows). DOSBox does not emulate TopView's synchronization vectors, so we can ignore this. |
| **`04h`** | `dir_ptr` | Returns a Far Pointer (`ES:DI`) to the APPEND directory list. | **Not Implemented.** To implement this, we would need to allocate space in the DOS Swappable Data Area (SDA) or use `DOS_GetMemory` to store the C++ `dir_list` string inside simulated conventional memory, and return that 16-bit segment:offset address. |
| **`06h`** | `get_state` | Returns APPEND `mode_flags` in `BX`. | **Requires Fix.** We should stub this to return `BX = 0` (or `BX = 0x0001` for enabled) to prevent uninitialized registers from leaking to the caller. |
| **`07h`** | `set_state` | Sets APPEND `mode_flags` from `BX`. | **Requires Fix.** We should safely intercept and ignore this so legacy apps don't crash. |
| **`10h`** | `DOS_version` | Returns DOS version in `DL`/`DH`. | **Requires Fix.** This is the *real* version check. We must implement this to return `DL = dos.version.major` and `DH = dos.version.minor`. |
| **`11h`** | `true_name` | Forces APPEND to write absolute paths into the caller's buffer. | **Implemented.** (Handled in `MultiplexHandler` and `DOS_Canonicalize`). |

---

## 2. Mode Flags (`mode_flags`) Truth Table

The original APPEND TSR managed its configuration through a 16-bit word called `mode_flags`. When calling subfunctions `06h` (Get State) or `07h` (Set State), these bits are passed in the `BX` register.

| Bitmask (Hex) | Name | Description |
| :--- | :--- | :--- |
| **`8000h`** | `X_mode` | `/X` switch active (execute file searches). |
| **`4000h`** | `E_mode` | `/E` switch active (store APPEND in DOS environment). |
| **`2000h`** | `Path_mode` | Allow directory paths in the requested filename. |
| **`1000h`** | `Drive_mode` | Allow drive letters in the requested filename. |
| **`0001h`** | `Enabled` | Overall APPEND enabled/disabled flag. |

*(All other bits are unused/reserved by MS-DOS 4.0)*

---

## 3. Assembly Logic Breakdown (`INT 2Fh`)

Below is the side-by-side comparison of the actual MS-DOS 4.0 Assembly code for the Multiplex handler (`do_appends`) and what the code is fundamentally doing.

| MS-DOS 4.0 Assembly Code (`APPEND.ASM`) | Functional Breakdown / Explanation |
| :--- | :--- |
| `cmp al,are_you_there` <br> `jne ck1` <br> `mov al,-1` <br> `iret` | **1. Installation Check (`AL=00h`)** <br> Checks if `AL == 0`. If true, sets `AL = 0xFF` (-1) and returns to caller to indicate APPEND is active. |
| `ck1:` <br> `cmp al,dir_ptr` <br> `jne ck2` <br> `les di,dword ptr dirlst_offset` <br> `iret` | **2. Get Directory Pointer (`AL=04h`)** <br> Checks if `AL == 4`. If true, loads the segment and offset (`ES:DI`) of the internal string containing the APPEND paths and returns. |
| `ck2:` <br> `cmp al,get_app_version` <br> `jne ck3` <br> `mov ax,-1` <br> `iret` | **3. Legacy Version Check (`AL=02h`)** <br> Checks if `AL == 2`. If true, sets `AX = 0xFFFF` (-1). This specifically signals to older applications that this is the MS-DOS built-in APPEND, not the IBM Network APPEND. |
| `ck5:` <br> `cmp al,DOS_version` <br> `jne ck6` <br> `mov ax,mode_flags` <br> `xor bx,bx` <br> `xor cx,cx` <br> `mov dl,byte ptr version_loc` <br> `mov dh,byte ptr version_loc+1` <br> `iret` | **4. Detailed DOS Version Check (`AL=10h`)** <br> Checks if `AL == 10h`. If true, returns `AX = mode_flags`, clears `BX` and `CX`, and returns the major version in `DL` and minor version in `DH`. |
| `ck6:` <br> `cmp al,get_state` <br> `jne ck7` <br> `mov bx,mode_flags` <br> `iret` | **5. Get State (`AL=06h`)** <br> Checks if `AL == 6`. If true, copies the internal `mode_flags` word into `BX` and returns to caller. |
| `ck7:` <br> `cmp al,set_state` <br> `jne ck8` <br> `mov mode_flags,bx` <br> `iret` | **6. Set State (`AL=07h`)** <br> Checks if `AL == 7`. If true, copies the user-provided `BX` register into the internal `mode_flags` word, modifying APPEND's configuration. |

---

## 4. Intercepted DOS APIs (`INT 21h`)

APPEND operates by hooking `INT 21h` and monitoring specific file-related operations. Below is a comparison of what MS-DOS 4.0 intercepts versus what DOSBox Staging currently implements in our C++ layer (`src/dos/dos_append.cpp`).

| Hex | Constant Name | Description | DOSBox-Staging Implementation Status |
| :--- | :--- | :--- | :--- |
| **`0Fh`** | `FCB_opn` | Open File via FCB | **Implemented.** (Handled in `DOS_FCB::Open`). |
| **`11h`** | `FCB_sch1` | Find First File via FCB | **Implemented.** (Handled in `DOS_FindFirstFCB`). |
| **`23h`** | `file_sz` | Get File Size via FCB | **Not Implemented.** (We do not hook `DOS_FCB::GetSize` yet). |
| **`3Dh`** | `handle_opn` | Open File via Handle | **Implemented.** (Handled in `DOS_OpenFile`). |
| **`4Bh`** | `exec_proc` | Execute Program (EXEC) | **Implemented.** (Handled in `DOS_Execute`). |
| **`4Eh`** | `handle_fnd1`| Find First File via Handle | **Implemented.** (Handled in `DOS_FindFirst`). |
| **`57h`** | `dat_tim` | Get/Set File Date and Time | **Not Implemented.** (We do not hook `DOS_GetFileDate`). |
| **`6Ch`** | `ext_handle_opn`| Extended Open/Create (DOS 4.0+) | **Implemented.** (Handled in `DOS_OpenFileExtended`). |

