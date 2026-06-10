# LPT Port Byte Capture: Complete Technical Walkthrough

## Executive Summary

The DOSBox hardware I/O system intercepts **every port write** the game makes. You create a device handler that registers itself on a specific port. When a game writes to that port, your callback function is invoked **immediately** with the byte value.

This document shows exactly how bytes flow from game → DOSBox → your device → destination.

---

## Part 1: The Hardware Port Address Space

### LPT Port Layout (Physical I/O Addresses)

```
                    Parallel Port (LPT) Address Space
        
Logical  │  Data Port  │  Status Port  │  Control Port
Address  │   (offset 0)│   (offset 1)  │   (offset 2)
─────────┼─────────────┼───────────────┼────────────────
LPT1     │   0x378     │    0x379      │    0x37A
LPT2     │   0x278     │    0x279      │    0x27A
LPT3     │   0x3BC     │    0x3BD      │    0x3BE
```

**Key Point**: Each logical printer has **3 consecutive I/O ports** for data, status, and control signals.

---

## Part 2: Game Writes → DOSBox Interception Flow

### Game Code (DOS Assembly)

```asm
; Example: A game printing the character 'A' (0x41)
mov al, 'A'         ; AL = 0x41
mov dx, 0x378       ; DX = LPT1 data port
out dx, al          ; CPU emulator intercepts this!
```

### DOSBox Intercepts The Write

```cpp
// File: src/hardware/port.cpp (line ~30)

// CPU emulator calls:
void IO_WriteB(io_port_t port, uint8_t val)
{
    // port = 0x378 (data port)
    // val = 0x41 ('A')
    
    auto handler = io_write_byte_handler[port];  // Lookup registered handler
    
    if (handler) {
        handler(port, val, io_width_t::byte);     // ← YOUR CALLBACK IS CALLED HERE
    }
}
```

---

## Part 3: Your Device Handler Receives the Byte

### Callback Signature (Handler Contract)

```cpp
// File: src/hardware/port.h (line 40)
using io_write_f = std::function<void(io_port_t port, io_val_t val, io_width_t width)>;

// Your handler must accept:
// - port:  Which I/O port (0x378, 0x379, 0x37A, etc.)
// - val:   The byte value (0x41 in our example)
// - width: Data width (byte=1, word=2, dword=4) - usually byte for LPT
```

### Real Example: Covox WriteData Handler

**File**: `src/hardware/audio/covox.cpp` (lines 45-48)

```cpp
void Covox::WriteData(const io_port_t port,
                      const io_val_t data,
                      const io_width_t width)
{
    RenderUpToNow();           // Catch up audio rendering
    data_reg = check_cast<uint8_t>(data);  // ← BYTE IS STORED HERE
}
```

When the game writes to port 0x378, this function is called with the byte value.

---

## Part 4: Handler Registration (How DOSBox Knows To Call You)

### Installation Pattern (Used by All LPT Devices)

**File**: `src/hardware/audio/covox.cpp` (lines 12-23)

```cpp
void Covox::BindToPort(const io_port_t lpt_port)
{
    using namespace std::placeholders;

    // Create callback wrappers that bind "this" (your device instance)
    const auto write_data  = std::bind(&Covox::WriteData, this, _1, _2, _3);
    const auto read_status = std::bind(&Covox::ReadStatus, this, _1, _2);
    const auto write_control = std::bind(&Covox::WriteControl, this, _1, _2, _3);

    // Register all three port handlers with DOSBox
    BindHandlers(lpt_port, write_data, read_status, write_control);
    //          ↓
    //   0x378 → write_data handler
    //   0x379 → read_status handler
    //   0x37A → write_control handler
}
```

### Under The Hood: BindHandlers() Implementation

**File**: `src/hardware/audio/private/lpt_dac.cpp` (lines 58-68)

```cpp
void LptDac::BindHandlers(const io_port_t lpt_port, 
                          const io_write_f write_data,
                          const io_read_f read_status,
                          const io_write_f write_control)
{
    // Each handler object installs itself on its specific port
    data_write_handler.Install(lpt_port,                    // Port 0x378
                              write_data,                   // Your callback
                              io_width_t::byte);            // 8-bit handler

    const auto status_port = static_cast<io_port_t>(lpt_port + 1u);
    status_read_handler.Install(status_port,                // Port 0x379
                               read_status, 
                               io_width_t::byte);

    const auto control_port = static_cast<io_port_t>(lpt_port + 2u);
    control_write_handler.Install(control_port,             // Port 0x37A
                                 write_control,
                                 io_width_t::byte);
}
```

### The Handler Objects Themselves

**File**: `src/hardware/port.h` (lines 81-98)

```cpp
class IO_WriteHandleObject {
public:
    void Install(io_port_t port,
                 io_write_f handler,           // Your callback
                 io_width_t max_width,
                 io_port_t range = 1);
    
    void Uninstall();
    ~IO_WriteHandleObject();  // ← Automatically uninstalls on destruction
};
```

These objects are **stored as members** of your device class, so they persist for the device's entire lifetime.

---

## Part 5: The Handshaking Protocol (STROBE Signal)

Modern printers use a handshaking protocol with the STROBE signal:

### Protocol Sequence

```
Timeline:
─────────────────────────────────────────────────

Game          DOSBox Printer           Destination
─────         ──────────────────       ────────────

                                   
1. out 0x378  ──data_byte─→  WriteData()  captures byte
   out 0x37A  ──control──→   WriteControl() STROBE pulse detected
              
2. Game waits for printer ready...
   
3. in 0x379   ←──status──    ReadStatus() reports BUSY flag
   
4. When printer finishes:
              ──ACK signal→  Game continues
```

### Disney Sound Source Example: WriteControl Detects STROBE

**File**: `src/hardware/audio/disney.cpp` (lines 96-107)

```cpp
void Disney::WriteControl(const io_port_t port,
                          const io_val_t value,
                          const io_width_t width)
{
    RenderUpToNow();
    
    const auto new_control = LptControlRegister{check_cast<uint8_t>(value)};
    
    // Detect rising edge of SELECT signal (acts as STROBE for Disney)
    if (!control_reg.select && new_control.select)  // ← Edge detection
        if (!IsFifoFull())
            fifo.emplace(data_reg);  // ← QUEUE THE BYTE FOR AUDIO OUTPUT
    
    control_reg.data = new_control.data;
}
```

**Key Point**: The WriteData handler gets the byte, but WriteControl detects the STROBE signal that says "process it now".

---

## Part 6: Where Bytes End Up (The "Drop Point")

### Covox Path: Byte → Audio Sample → Mixer

```
WriteData(port, value)
    ↓
data_reg = value              // Byte stored
    ↓
[Every 44,100th of a second]
    ↓
Render()                      // Called by mixer callback
    ↓
const float sample = lut_u8to16[data_reg];  // ← BYTE CONVERTED TO AUDIO
    ↓
return {sample, sample};      // ← SENT TO MIXER (audio output)
```

**File**: `src/hardware/audio/covox.cpp` (lines 41-43)

```cpp
AudioFrame Covox::Render()
{
    const float sample = lut_u8to16[data_reg];
    return {sample, sample};  // ← BYTES END UP IN AUDIO SYSTEM
}
```

### Disney Path: Byte → FIFO Queue → Audio Output

```
WriteData(port, value)
    ↓
data_reg = value              // Byte stored
    ↓
WriteControl detects STROBE
    ↓
fifo.emplace(data_reg)        // ← BYTE QUEUED TO FIFO
    ↓
[7,000 times per second - Disney's clock rate]
    ↓
Render()
    ↓
const float sample = lut_u8to16[fifo.front()];  // ← BYTE FROM QUEUE
fifo.pop();                                      // ← DEQUEUED
    ↓
return {sample, sample};      // ← SENT TO AUDIO SYSTEM
```

**File**: `src/hardware/audio/disney.cpp` (lines 68-74)

```cpp
AudioFrame Disney::Render()
{
    assert(fifo.size());
    const float sample = lut_u8to16[fifo.front()];
    if (fifo.size() > 1)
        fifo.pop();
    return {sample, sample};
}
```

---

## Part 7: Creating a Printer Device Without Affecting LPT DAC

### Strategy: Use Separate LPT Port

Since LPT DAC binds to **LPT1 (0x378)**, your printer can use **LPT2 (0x278)** or **LPT3 (0x3BC)**:

```cpp
// File: src/hardware/lpt.h
enum LptPorts : io_port_t {
    Lpt1Port = 0x378,  // ← Taken by LPT DAC
    Lpt2Port = 0x278,  // ← Available for printer
    Lpt3Port = 0x3bc,  // ← Available for printer
};
```

### Complete Printer Handler Example

```cpp
// File: src/hardware/printer/printer.h

#ifndef DOSBOX_PRINTER_H
#define DOSBOX_PRINTER_H

#include "dosbox.h"
#include "hardware/lpt.h"
#include "hardware/port.h"

#include <fstream>
#include <vector>

class ParallelPortPrinter {
public:
    ParallelPortPrinter(const std::string& output_file);
    ~ParallelPortPrinter();
    
    // Called from initialization to bind handlers
    void BindToPort(const io_port_t lpt_port);
    
private:
    // Handlers called by DOSBox when game writes to ports
    void WriteData(const io_port_t port, const io_val_t value, const io_width_t width);
    uint8_t ReadStatus(const io_port_t port, const io_width_t width);
    void WriteControl(const io_port_t port, const io_val_t value, const io_width_t width);
    
    // Helper to write bytes to output file
    void FlushBuffer();
    
    // Port handler objects (persist for device lifetime)
    IO_WriteHandleObject data_write_handler;
    IO_ReadHandleObject status_read_handler;
    IO_WriteHandleObject control_write_handler;
    
    // Printer state
    uint8_t data_reg = 0;
    LptStatusRegister status_reg;
    LptControlRegister control_reg;
    
    // Output file
    std::string output_filename;
    std::ofstream output_file;
    std::vector<uint8_t> buffer;
};

#endif
```

### Implementation: Capturing Bytes to File

```cpp
// File: src/hardware/printer/printer.cpp

#include "printer.h"
#include "utils/checks.h"

CHECK_NARROWING();

ParallelPortPrinter::ParallelPortPrinter(const std::string& output_file)
    : output_filename(output_file)
{
    output_file.open(output_filename, std::ios::binary | std::ios::app);
    LOG_MSG("Printer: Initializing with output file: %s", output_filename.c_str());
    
    // Set printer to ready state
    status_reg.busy = false;
    status_reg.error = false;
}

void ParallelPortPrinter::BindToPort(const io_port_t lpt_port)
{
    using namespace std::placeholders;
    
    // Bind this device's methods as handlers
    const auto write_data = std::bind(&ParallelPortPrinter::WriteData, this, _1, _2, _3);
    const auto read_status = std::bind(&ParallelPortPrinter::ReadStatus, this, _1, _2);
    const auto write_control = std::bind(&ParallelPortPrinter::WriteControl, this, _1, _2, _3);
    
    // Register on three consecutive ports
    data_write_handler.Install(lpt_port, write_data, io_width_t::byte);
    status_read_handler.Install(lpt_port + 1, read_status, io_width_t::byte);
    control_write_handler.Install(lpt_port + 2, write_control, io_width_t::byte);
    
    LOG_MSG("Printer: Bound to LPT port %03xh", lpt_port);
}

void ParallelPortPrinter::WriteData(const io_port_t port,
                                   const io_val_t value,
                                   const io_width_t width)
{
    // Game wrote a byte to the data port
    data_reg = check_cast<uint8_t>(value);
    
    LOG_MSG("Printer: Received byte 0x%02X ('%c')", 
            data_reg, 
            (data_reg >= 32 && data_reg < 127) ? data_reg : '?');
}

uint8_t ParallelPortPrinter::ReadStatus(const io_port_t port,
                                       const io_width_t width)
{
    // Game reads status - report that printer is ready
    return status_reg.data;
}

void ParallelPortPrinter::WriteControl(const io_port_t port,
                                      const io_val_t value,
                                      const io_width_t width)
{
    const auto new_control = LptControlRegister{check_cast<uint8_t>(value)};
    
    // Detect STROBE pulse (active-low, so we detect transition from 1→0)
    if (control_reg.strobe && !new_control.strobe) {  // ← STROBE pulse detected
        // This is when the printer actually receives the byte
        buffer.push_back(data_reg);
        
        LOG_MSG("Printer: Queued byte, buffer size: %zu", buffer.size());
        
        // Flush if buffer gets large enough
        if (buffer.size() >= 256) {
            FlushBuffer();
        }
    }
    
    control_reg.data = new_control.data;
}

void ParallelPortPrinter::FlushBuffer()
{
    if (!buffer.empty()) {
        output_file.write(reinterpret_cast<char*>(buffer.data()), buffer.size());
        output_file.flush();
        
        LOG_MSG("Printer: Flushed %zu bytes to file", buffer.size());
        buffer.clear();
    }
}

ParallelPortPrinter::~ParallelPortPrinter()
{
    FlushBuffer();
    if (output_file.is_open()) {
        output_file.close();
    }
    
    // Handlers automatically uninstall via RAII
    LOG_MSG("Printer: Shutdown complete");
}
```

---

## Part 8: Complete Data Flow Diagram

```
DOS Game Code                    DOSBox Hardware Layer
─────────────────────────────    ─────────────────────────────────────────

mov al, 'A'                      
out 0x378, al                    CPU emulator calls: IO_WriteB(0x378, 0x41)
                                         ↓
                                 Lookup: io_write_byte_handler[0x378]
                                         ↓
                                 Call handler: write_data(0x378, 0x41, byte)
                                         ↓
                                 ┌─────────────────────────────┐
                                 │ Your WriteData Handler      │
                                 │ data_reg = 0x41             │
                                 └─────────────────────────────┘

mov al, STROBE_CONTROL           out 0x37A, al                
                                 CPU emulator calls: IO_WriteB(0x37A, control)
                                         ↓
                                 Lookup: io_write_byte_handler[0x37A]
                                         ↓
                                 Call handler: write_control(0x37A, ..., byte)
                                         ↓
                                 ┌──────────────────────────────────┐
                                 │ Your WriteControl Handler        │
                                 │ Detect STROBE pulse              │
                                 │ buffer.push_back(0x41) ← BYTE!  │
                                 └──────────────────────────────────┘
                                         ↓
                                 Write to output file

in 0x379, al                     CPU emulator calls: IO_ReadB(0x379)
                                         ↓
                                 Lookup: io_read_byte_handler[0x379]
                                         ↓
                                 Call handler: read_status(0x379, byte)
                                         ↓
                                 ┌──────────────────────────────┐
                                 │ Your ReadStatus Handler      │
                                 │ return status_reg.data       │
                                 └──────────────────────────────┘
                                         ↓
                                 Return status to game
```

---

## Part 9: Avoiding Conflicts with LPT DAC

### Current State
```
Port 0x378 (LPT1 Data)    → LPT DAC (Disney/Covox/StereoOn1)
Port 0x379 (LPT1 Status)  → LPT DAC
Port 0x37A (LPT1 Control) → LPT DAC
```

### With Printer on LPT2
```
Port 0x378 (LPT1 Data)    → LPT DAC (Disney/Covox/StereoOn1)
Port 0x379 (LPT1 Status)  → LPT DAC
Port 0x37A (LPT1 Control) → LPT DAC

Port 0x278 (LPT2 Data)    → Parallel Port Printer
Port 0x279 (LPT2 Status)  → Parallel Port Printer
Port 0x27A (LPT2 Control) → Parallel Port Printer
```

**No conflict!** Each device registers its own handlers on different ports.

---

## Part 10: Key Takeaways

1. **Every port write intercepts to your callback**: When a game does `out 0x378, al`, your `WriteData()` is called with `al`'s value

2. **Handler lifecycle is automatic**: The `IO_WriteHandleObject` members persist for your device's lifetime and auto-uninstall on destruction

3. **Three consecutive ports, three handlers**: Data (write), Status (read), Control (write)

4. **STROBE signal timing**: The game writes data to port 0x378, then pulses the STROBE signal on port 0x37A. You detect the pulse in `WriteControl()` and actually process the byte there

5. **Multiple devices coexist**: LPT DAC uses 0x378, your printer can use 0x278 or 0x3BC with zero conflict

6. **Bytes end up where you send them**: Covox→audio mixer, Disney→FIFO queue, your printer→file buffer

---

## Example Initialization

In `src/dosbox.cpp` or wherever devices are initialized:

```cpp
static std::unique_ptr<ParallelPortPrinter> printer_device;

void Initialize_Printer()
{
    printer_device = std::make_unique<ParallelPortPrinter>("printer_output.txt");
    printer_device->BindToPort(Lpt2Port);  // Use LPT2 (0x278)
    
    LOG_MSG("Printer initialized on LPT2");
}

void Shutdown_Printer()
{
    if (printer_device) {
        printer_device.reset();
    }
}
```

This is exactly the pattern used for LPT DAC in `src/hardware/audio/lpt_dac.cpp`.

