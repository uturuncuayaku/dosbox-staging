---
name: append-asm-reference
description: Use this skill to cross-reference the C++ APPEND implementation against the original MS-DOS 4.0 assembly code (appendasm.txt). Trigger this when reviewing APPEND accuracy, checking TopView barriers, or validating INT 21h intercepts.
---

# APPEND Assembly Reference Guide

The goal of DOSBox-Staging's APPEND command is to achieve 1:1 parity with MS-DOS 4.0. To do this, you must verify our C++ code against the raw MS-DOS assembly reference located in `documentation/reference/appendasm.txt`.

When tasked with reviewing the APPEND implementation, use `grep_search` on `appendasm.txt` to verify the following critical MS-DOS behaviors:

## 1. INT 21h Intercepts
The original MS-DOS APPEND hooked the following INT 21h calls. Verify our `DOS_21Handler` intercept correctly delegates these:
*   `0Fh` - FCB Open (`FCB_opn`)
*   `11h` - FCB Search First (`FCB_sch1`)
*   `23h` - FCB File Size (`file_sz`)
*   `3Dh` - Handle Open (`handle_opn`)
*   `4Bh` - EXEC / Load Program (`exec_proc`)
*   `4Eh` - Handle Find First (`handle_fnd1`)
*   `57h` - Get/Set File Date/Time (`dat_tim`)
*   `6Ch` - Extended Open (`ext_handle_opn`)

## 2. Extended vs Environmental Mode (`/X` and `/E`)
*   Search `appendasm.txt` for `X_mode` and `E_mode`. 
*   Verify that our C++ implementation correctly isolates `SEARCH`, `FIND`, and `EXEC` intercepts so they ONLY trigger if `/X` is active.

## 3. The TopView Barrier
*   Search `appendasm.txt` for `tv_flag` and `TV_TRUE`.
*   The original Microsoft code had specific hacks for IBM TopView multitasking. If the `tv_flag` was set, it bypassed standard APPEND processing for `EXEC` (4Bh). 
*   Ensure our C++ implementation either respects this barrier or explicitly documents why DOSBox-Staging does not need TopView isolation.

## 4. Error Handling
*   Search `appendasm.txt` for `expected_ext_error`.
*   Notice how MS-DOS APPEND forces `handle_file_not_found` (02h) or `handle_path_not_found` (03h) when searching fails. Verify our `CALLBACK_SCF(true)` calls inject the exact same error codes into `reg_ax`.
