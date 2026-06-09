# Printer Implementation - Architecture Diagrams

## System Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                     DOS APPLICATIONS                          │
│  (Print commands via INT 17h, FILE I/O, or Direct Port I/O)  │
└────────────────────────┬────────────────────────────────────┘
                         │
        ┌────────────────┼────────────────┐
        │                │                │
        ▼                ▼                ▼
    ┌────────┐      ┌─────────┐      ┌──────────┐
    │ INT17h │      │ DOS     │      │ Direct   │
    │ BIOS   │      │ Device  │      │ Port I/O │
    │Handler │      │ (LPT1)  │      │(0x378-0x37A)│
    └───┬────┘      └────┬────┘      └────┬─────┘
        │                │                │
        └────────────────┼────────────────┘
                         │
        ┌────────────────▼────────────────┐
        │   Port Handler Registration      │
        │ (IO_WriteHandleObject, etc.)     │
        └────────────────┬────────────────┘
                         │
        ┌────────────────▼────────────────────────┐
        │    Parallel Port Printer Instance        │
        │  (ParallelPortPrinter class)             │
        │                                          │
        │  ┌────────────────────────────────────┐ │
        │  │ Port Operations:                   │ │
        │  │ • Data Register (0x378) write      │ │
        │  │ • Status Register (0x379) read    │ │
        │  │ • Control Register (0x37A) write  │ │
        │  └────────────────────────────────────┘ │
        │                                          │
        │  ┌────────────────────────────────────┐ │
        │  │ Handshaking Logic:                 │ │
        │  │ • STROBE pulse detection           │ │
        │  │ • BUSY/ACK signal management       │ │
        │  │ • Status register updates          │ │
        │  └────────────────────────────────────┘ │
        │                                          │
        │  ┌────────────────────────────────────┐ │
        │  │ Buffer Management:                 │ │
        │  │ • Accumulate bytes                 │ │
        │  │ • Track STROBE pulses              │ │
        │  │ • Manage timeouts                  │ │
        │  └────────────────────────────────────┘ │
        └────────────────┬────────────────────────┘
                         │
        ┌────────────────▼────────────────┐
        │    Output Destination Handler     │
        │  (Configurable Mode)              │
        └────────────────┬────────────────┘
                         │
        ┌────────────────┼────────────────┬─────────────────┐
        │                │                │                 │
        ▼                ▼                ▼                 ▼
    ┌────────┐      ┌─────────┐    ┌──────────┐      ┌────────────┐
    │  File  │      │  Memory │    │  Device  │      │ Future:    │
    │Output  │      │  Buffer │    │  Printer │      │ PDF/Print  │
    └────────┘      └─────────┘    └──────────┘      │ Services   │
                                                       └────────────┘
```

## Control Flow - Writing a Character

```
DOS Program calls INT 17h (AH=0x00, AL=character)
        │
        ▼
INT17_Handler() in src/ints/bios.cpp
        │
        ├─→ Get LPT port address from BIOS data area (0x408)
        │
        ├─→ Call printer_instance->WriteCharacter(reg_al)
        │       │
        │       ├─→ Store byte in buffer
        │       │
        │       ├─→ Set BUSY flag in status register
        │       │
        │       ├─→ Check for STROBE pulse on control register
        │       │   (Bit 0 transition from 1 to 0 to 1)
        │       │
        │       └─→ If STROBE detected:
        │           ├─→ Send byte to output destination
        │           ├─→ Pulse ACK signal (brief)
        │           ├─→ Clear BUSY flag
        │           └─→ Update status register
        │
        ├─→ Get current status: reg_ah = printer_instance->GetStatus()
        │
        └─→ Return to DOS program with status in AH

Result: DOS program receives printer status (0=OK, non-zero=error)
```

## Port Operations Timeline

```
Timeline:
─────────────────────────────────────────────────────────────

Data Port Write:  [0x378] ← 0x41 ('A')
                   │
                   └──► Stored in printer buffer

Control Port Write: [0x37A] ← 0xFE (STROBE=0, rest=1)
                     │
                     └──► STROBE pulse detected ─┐
                                                  │
Control Port Write: [0x37A] ← 0xFF (STROBE=1)   │
                     │                            │
                     └──► Pulse ends ─────────────┤
                                                  │
                                    Handle byte ──┘
                                    Send to output
                                    Set ACK signal

Status Port Read:  [0x379] → 0xF0 (BUSY=1,ACK=1,...)
                   │
                   └──► DOS program sees busy signal

[Later, after processing simulates complete...]

Status Port Read:  [0x379] → 0xB0 (BUSY=0,ACK=0,...)
                   │
                   └──► DOS program sees ready signal
```

## Data Structure: Parallel Port Registers

```
Parallel Port Base Address: 0x378 (LPT1), 0x278 (LPT2), 0x3BC (LPT3)

┌─────────────────────────────────────────────────────────────┐
│ Data Register (0x378)  - Write Only                          │
├─────────────────────────────────────────────────────────────┤
│ Bit 7 6 5 4 3 2 1 0                                          │
│     D7 D6 D5 D4 D3 D2 D1 D0  (Data bits to printer)          │
│                                                              │
│ Example: 0x41 = 'A' = 01000001 in binary                     │
└─────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────┐
│ Status Register (0x379) - Read Only                          │
├─────────────────────────────────────────────────────────────┤
│ Bit 7 6 5 4 3 2 1 0                                          │
│     | B A O S E R R                                          │
│     │ │ │ │ │ │ │ └─ Reserved                               │
│     │ │ │ │ │ │ └──── Reserved                              │
│     │ │ │ │ │ └────── Error (active LOW)                   │
│     │ │ │ │ └──────── Select In (active HIGH)              │
│     │ │ │ └────────── Paper Out (active HIGH)              │
│     │ │ └──────────── ACK (active LOW - brief pulse)       │
│     │ └───────────── IRQ (interrupt possible)              │
│     └─────────────── BUSY (active LOW)                     │
│                                                              │
│ Example: 0xB0 = 10110000 = BUSY=1, ACK=0, PaperOut=1, ...  │
└─────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────┐
│ Control Register (0x37A) - Write Only                        │
├─────────────────────────────────────────────────────────────┤
│ Bit 7 6 5 4 3 2 1 0                                          │
│     X X B I S I A S                                          │
│     │ │ │ │ │ │ │ └─ Strobe (active LOW - pulse)           │
│     │ │ │ │ │ │ └──── Auto Line Feed (if CR)               │
│     │ │ │ │ │ └────── Init (active LOW - reset)            │
│     │ │ │ │ └──────── Select                               │
│     │ │ │ └────────── IRQ Acknowledge                      │
│     │ │ └──────────── Bidirectional (PS/2 mode)            │
│     └─ └─────────────── Unused                             │
│                                                              │
│ Example: 0xFC = 11111100 = STROBE=0 (pulse), rest=1        │
│          Then  = 0xFE = 11111110 = STROBE=1 (pulse ends)   │
└─────────────────────────────────────────────────────────────┘
```

## Class Hierarchy for Implementation

```
┌─────────────────────────────┐
│  ParallelPortPrinter (new)   │
├─────────────────────────────┤
│ Public Methods:              │
│  • WriteCharacter(byte)      │
│  • InitializePrinter()       │
│  • GetStatus()               │
│  • BindToPort(io_port_t)     │
│  • SetMode(PrinterMode)      │
│  • SetOutputPath(string)     │
├─────────────────────────────┤
│ Private Methods:             │
│  • HandleDataWrite()         │
│  • HandleStatusRead()        │
│  • HandleControlWrite()      │
│  • FlushBuffer()             │
│  • WriteToFile()             │
├─────────────────────────────┤
│ Member Variables:            │
│  • status_reg                │
│  • control_reg               │
│  • data_reg                  │
│  • buffer[]                  │
│  • output_file               │
│  • file_handle               │
│  • port_handlers             │
│  • timeout tracking          │
└─────────────────────────────┘

Related Classes (Existing):
└─ LptStatusRegister (union with bit_view fields)
└─ LptControlRegister (union with bit_view fields)
└─ device_LPT1 (derived from device_NUL)
```

## File Output Flow

```
Application writes to printer
    │
    ├──→ INT 17h / INT 21h
    │
    ├──→ Port writes (0x378, 0x37A)
    │       │
    │       └─→ Detected by port handlers
    │
    ├──→ Bytes accumulated in buffer
    │
    ├──→ On STROBE pulse, byte formatted
    │
    ├──→ Buffer accumulated (e.g., every 64 bytes or 100ms)
    │
    └──→ FlushBuffer() called
            │
            ├──→ Open output file (or append to existing)
            │
            ├──→ Write accumulated bytes
            │
            └──→ Close file / sync to disk

Result: printer_output.txt contains raw byte sequence
```

## Configuration Integration Point

```
config/setup.h
    │
    └──→ Creates [speaker] section
        (LPT DAC already there)
        │
        ├──→ lpt_dac = disney|covox|ston1|none
        │
        └──→ lpt_dac_filter = on|off
                │
                └──→ SPEAKER_Init() called at boot
                    │
                    ├──→ LPTDAC_Init()
                    │
                    └──→ PRINTER_Init() [NEW]
                        │
                        └──→ Creates printer instance
                            Binds to LPT1 port
```

Alternatively, new [printer] section:

```
[printer]
    printer = none|file|memory
    printer_output = ./printer_output.txt
    printer_timeout_ms = 5000
```

## State Transitions

```
                    ┌─── IDLE (ready for input)
                    │
DOS writes data ────┤
    │               │
    └─→ BUFFERED ───┘  (data accumulated, waiting for STROBE)
    │       │
    │       └─→ Detects STROBE pulse
    │           │
    │           ├─→ Sets BUSY signal
    │           │
    │           ├─→ Sends data to output
    │           │
    │           ├─→ Pulses ACK signal
    │           │
    │           └─→ Clears BUSY, returns to IDLE
    │
    └─→ ERROR (on fault)
            │
            └─→ Sets error flag in status register
                Application can retry or abort
```

## Key Integration Points in Codebase

```
dosbox.cpp
    ├─→ SPEAKER_Init() [existing]
    │   └─→ LPTDAC_Init()
    │   └─→ PRINTER_Init() [NEW]
    │
    └─→ SPEAKER_Destroy() [existing]
        └─→ LPTDAC_Destroy()
        └─→ PRINTER_Destroy() [NEW]

bios.cpp
    └─→ INT17_Handler() [modified]
        └─→ printer_instance->WriteCharacter()
        └─→ printer_instance->GetStatus()

dos_devices.cpp
    └─→ device_LPT1::Write() [modified]
        └─→ printer_instance->WriteCharacter()

hardware/lpt.h [existing]
    └─→ LptStatusRegister, LptControlRegister
    └─→ Lpt1Port, Lpt2Port, Lpt3Port constants
```

## Testing Strategy Without Physical Printer

```
Test 1: File Output Verification
┌───────────────────────────────────┐
│ BASIC Program:                    │
│ LPRINT "Hello, DOSBox!"           │
│ LPRINT "Test 2"                   │
└───────────────────────────────────┘
        │
        └──→ Generates printer_output.txt
            containing: "Hello, DOSBox!\nTest 2\n"

Test 2: Port I/O Test
┌───────────────────────────────────┐
│ Assembly:                         │
│ MOV AL, 'A'                       │
│ MOV DX, 0x378                     │
│ OUT DX, AL        ; Write data    │
│ MOV AL, 0xFE      ; STROBE=0     │
│ MOV DX, 0x37A                    │
│ OUT DX, AL        ; Pulse STROBE │
│ MOV AL, 0xFF      ; STROBE=1     │
│ OUT DX, AL        ; End pulse     │
│ MOV DX, 0x379                    │
│ IN AL, DX         ; Read status  │
└───────────────────────────────────┘
        │
        └──→ Byte 'A' (0x41) written to file

Test 3: Status Register Check
┌───────────────────────────────────┐
│ Turbo Pascal:                     │
│ OutPort($37A, $FF);               │
│ status := InPort($379);           │
│ Ready := (status AND $80) = 0;    │
└───────────────────────────────────┘
        │
        └──→ Verify BUSY bit reflects state
```

---

## Summary of Key Decisions

| Decision | Rationale |
|----------|-----------|
| **File Output** | No physical printer; allows verification and testing |
| **Raw Bytes** | Support any printer control language (ESC/P, etc.) |
| **Follow LPT DAC Pattern** | Proven architecture in codebase |
| **INT17h + Port I/O** | Both interfaces widely used by DOS apps |
| **LPT1 Primary** | Most common port; others can be added later |
| **Configurable** | Users choose output destination |
| **Buffering** | Reduces disk I/O; improves performance |
| **Status Simulation** | Realistic timing prevents app hangs |
