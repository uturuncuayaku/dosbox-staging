# Printer Implementation Planning for DOSBox Staging

## Executive Summary

Printer support in DOSBox Staging can be implemented by expanding the existing Parallel Port (LPT) infrastructure. The codebase already has:
- Well-defined LPT port register structures (`src/hardware/lpt.h`)
- A proven hardware emulation pattern via LPT DAC (Digital-to-Analog Converters for audio)
- Minimal INT17 BIOS handler stubs in `src/ints/bios.cpp`
- Basic DOS device class `device_LPT1` in `src/dos/dos_devices.cpp`

This document outlines how to implement a complete printer pipeline with file output capability.

---

## Part 1: Current Architecture Analysis

### 1.1 Parallel Port (LPT) Infrastructure

**Location**: `src/hardware/lpt.h`

The port already defines three I/O registers:
```
Data Port (offset 0)   : 0x378 / 0x278 / 0x3BC - Write only - Data byte to printer
Status Port (offset 1) : +1 offset - Read only - Printer status flags
Control Port (offset 2): +2 offset - Write only - Handshaking signals
```

**Status Register Bits** (already defined):
```
Bit 7: BUSY (active low) - Printer is processing data
Bit 6: ACK (active low)  - Printer acknowledged receipt
Bit 5: PAPER_OUT (active high) - No paper in printer
Bit 4: SELECT_IN (active high) - Printer online
Bit 3: ERROR (active low) - Printer error condition
Bit 2: IRQ - Can trigger interrupt
Bits 0-1: Reserved
```

**Control Register Bits** (already defined):
```
Bit 0: STROBE (active low) - Pulse to signal data ready
Bit 1: AUTO_LF - Auto line-feed on CR
Bit 2: INITIALIZE (active low) - Reset printer
Bit 3: SELECT - Select printer
Bit 4: IRQ_ACK - Acknowledge interrupt
Bit 5: BIDI - Bidirectional mode (PS/2 printers)
Bits 6-7: Unused
```

### 1.2 LPT DAC Pattern (Model for Implementation)

**Location**: `src/hardware/audio/lpt_dac.{h,cpp}`

The LPT DAC implementation shows the mature pattern used in DOSBox Staging:

**Public Interface** (`lpt_dac.h`):
```cpp
void LPTDAC_Init(SectionProp& section);
void LPTDAC_Destroy();
void LPTDAC_NotifySettingUpdated(SectionProp& section, const std::string& prop_name);
void LPTDAC_NotifyLockMixer();
void LPTDAC_NotifyUnlockMixer();
void LPTDAC_AddConfigSection(Section* sec);
```

**Implementation Pattern**:
1. **Global singleton**: `static std::unique_ptr<LptDac> lpt_dac = {};`
2. **Port binding**: Registers IO port handlers for data/status/control
3. **Configuration**: Settings stored in `[speaker]` config section with choices
4. **Lifecycle**: Init/Destroy functions called during emulation startup/shutdown
5. **Timing**: TIMER_AddTickHandler for periodic operations
6. **Threading**: Mixer locks for thread-safe access

**Example - Disney Sound Source** (`src/hardware/audio/disney.cpp`):
```cpp
// Derived class from LptDac
class Disney final : public LptDac {
    void BindToPort(const io_port_t lpt_port) override;
    void ConfigureFilters(const FilterState state) override;
    AudioFrame Render() override;  // Called each tick
    
    void WriteData(const io_port_t, const io_val_t value, const io_width_t);
    uint8_t ReadStatus(const io_port_t, const io_width_t);
    void WriteControl(const io_port_t, const io_val_t value, const io_width_t);
};
```

### 1.3 Current BIOS INT17 Handler

**Location**: `src/ints/bios.cpp` (around line 485)

```cpp
static Bitu INT17_Handler(void) {
    LOG(LOG_BIOS,LOG_NORMAL)("INT17:Function %X",reg_ah);
    switch (reg_ah) {
    case 0x00:  /* PRINTER: Write Character */
        reg_ah=1;   /* Report a timeout */
        break;
    case 0x01:  /* PRINTER: Initialize port */
        break;
    case 0x02:  /* PRINTER: Get Status */
        reg_ah=0;   
        break;
    };
    return CBRET_NONE;
}
```

**Current State**: Minimal stubs that don't actually interact with hardware.

### 1.4 DOS Device Class

**Location**: `src/dos/dos_devices.cpp` (around line 300)

```cpp
class device_LPT1 final : public device_NUL {
public:
    device_LPT1() { SetName("LPT1"); }
    uint16_t GetInformation() override { return 0x80A0; }
    bool Read(uint8_t* /*data*/, uint16_t* /*size*/) override {
        DOS_SetError(DOSERR_ACCESS_DENIED);
        return false;
    }
};
```

**Current State**: Minimal implementation, doesn't support actual writes.

---

## Part 2: Implementation Areas

### Area 1: New Hardware Printer Module

**Location**: `src/hardware/printer/`

Create parallel structure to LPT DAC:

**Files to Create**:
- `src/hardware/printer/printer.h` - Public interface
- `src/hardware/printer/printer.cpp` - Implementation
- `src/hardware/printer/private/printer_impl.h` - Private base class (if needed)

**Key Components**:

#### 1.1 Base Printer Class
```cpp
class ParallelPortPrinter {
public:
    ParallelPortPrinter(std::string_view output_path);
    virtual ~ParallelPortPrinter();
    
    // Core operations
    void WriteCharacter(uint8_t byte);      // Called on STROBE pulse
    void InitializePrinter();               // Called on INITIALIZE signal
    uint8_t GetStatus();                    // Read status register
    
    // Port binding
    void BindToPort(const io_port_t lpt_port);
    
    // Configuration
    void SetMode(PrinterMode mode);         // File, device, memory buffer
    void SetOutputPath(const std::string& path);
    void SetAutoLineFeed(bool enabled);
    
private:
    // Port handlers
    void HandleDataWrite(const io_port_t, io_val_t value);
    uint8_t HandleStatusRead(const io_port_t);
    void HandleControlWrite(const io_port_t, io_val_t value);
    
    // Output operations
    void FlushBuffer();
    void WriteToFile(const std::string& data);
    
    // State tracking
    LptStatusRegister status_reg;
    LptControlRegister control_reg;
    uint8_t data_reg;
    
    std::vector<uint8_t> buffer;
    std::string output_file;
    std::unique_ptr<FILE> file_handle;
    
    IO_WriteHandleObject data_write_handler;
    IO_ReadHandleObject status_read_handler;
    IO_WriteHandleObject control_write_handler;
};
```

#### 1.2 Configuration
```cpp
enum class PrinterMode {
    None,           // Disabled
    File,           // Output to file
    PrintToFile,    // Format as print file
    Memory          // Internal buffer
};
```

---

### Area 2: Enhanced BIOS INT17 Handler

**Location**: `src/ints/bios.cpp` - INT17_Handler function

**Changes Required**:

```cpp
static Bitu INT17_Handler(void) {
    LOG(LOG_BIOS,LOG_NORMAL)("INT17:Function %X",reg_ah);
    
    uint16_t port = real_readw(0x40, reg_dx * 2);  // Get LPT port address
    if (!port || !printer_instance) {
        // Port doesn't exist or printer not initialized
        return CBRET_NONE;
    }
    
    switch (reg_ah) {
    case 0x00: {  /* PRINTER: Write Character */
        // AL contains character to print
        printer_instance->WriteCharacter(reg_al);
        
        // Update AH with status
        reg_ah = printer_instance->GetStatus();
        break;
    }
    case 0x01: {  /* PRINTER: Initialize port */
        printer_instance->InitializePrinter();
        reg_ah = printer_instance->GetStatus();
        break;
    }
    case 0x02: {  /* PRINTER: Get Status */
        reg_ah = printer_instance->GetStatus();
        break;
    }
    case 0x03: {  /* PRINTER: Return parallel port address (enhanced) */
        // Return the address in DX
        // DL returns: 0=1st LPT, 1=2nd LPT, 2=3rd LPT
        reg_ax = port;
        break;
    }
    };
    return CBRET_NONE;
}
```

**Key Logic**:
- Query BIOS data area for LPT port address (0x408, 0x40A, 0x40C for LPT1-3)
- Delegate to printer hardware instance
- Update status register based on printer state
- Handle timeouts gracefully

---

### Area 3: Enhanced DOS Device Class

**Location**: `src/dos/dos_devices.cpp` - device_LPT1 class

**Changes Required**:

```cpp
class device_LPT1 final : public device_NUL {
public:
    device_LPT1() { SetName("LPT1"); }
    
    uint16_t GetInformation() override { return 0x80A0; }
    
    bool Read(uint8_t* data, uint16_t* size) override {
        // LPT is write-only for printers, but could read status
        DOS_SetError(DOSERR_ACCESS_DENIED);
        return false;
    }
    
    bool Write(uint8_t* data, uint16_t* size) override {
        // Forward writes to printer
        if (!printer_instance) {
            DOS_SetError(DOSERR_DEVICE_FAULT);
            return false;
        }
        
        for (uint16_t i = 0; i < *size; ++i) {
            printer_instance->WriteCharacter(data[i]);
        }
        return true;
    }
    
    uint16_t GetInformation() override { return 0x80A0; }
    
    // Other required overrides...
};
```

**Key Operations**:
- Write operation forwards bytes to printer
- Read returns error (printers are output-only)
- Supports `MODE` and `PRINT` DOS commands

---

### Area 4: Configuration System

**Location**: New section in configuration setup

**Pattern** (following LPT DAC model):

```cpp
// In a new file or in speaker.cpp area:
static void init_printer_settings(SectionProp& section)
{
    auto pstring = section.AddString("printer", WhenIdle, "none");
    pstring->SetHelp(
        "Enable printer emulation ('none' by default). Possible values:\n"
        "\n"
        "  none/off:   Don't emulate a printer (default).\n"
        "  file:       Output to file specified in printer_output.\n"
        "  memory:     Keep output in memory buffer.\n");
    pstring->SetValues({"none", "file", "memory", "off"});

    auto pstring_output = section.AddString("printer_output", WhenIdle, "printer.txt");
    pstring_output->SetHelp("File path for printer output.");
}

void PRINTER_Init(SectionProp& section)
{
    const std::string printer_choice = section.GetString("printer");
    
    if (printer_choice == "none" || printer_choice == "off") {
        return;
    }
    
    // Create printer instance
    auto output_path = section.GetString("printer_output");
    printer_instance = std::make_unique<ParallelPortPrinter>(output_path);
    
    // Bind to LPT1 port
    printer_instance->BindToPort(Lpt1Port);
}

void PRINTER_Destroy()
{
    if (printer_instance) {
        printer_instance.reset();
    }
}
```

**Configuration File Example**:
```ini
[printer]
printer=file
printer_output=./dosbox_output/printer.txt
```

---

## Part 3: Data Flow Architecture

### 3.1 Write Operation Flow

```
DOS Program
    ↓
INT 17h / BIOS Call (or direct port write)
    ↓
INT17_Handler / Port Handler
    ↓
printer_instance->WriteCharacter(byte)
    ↓
Buffer byte + check STROBE signal
    ↓
If STROBE pulse detected:
  - Send byte to output destination
  - Update BUSY flag
  - Pulse ACK signal
    ↓
Update status register
    ↓
Return status to caller
```

### 3.2 Output Destination Strategy

**Recommended for Testing** (No Physical Printer):
1. **File Output** (Primary): Write raw bytes to timestamped file
   - Simple to implement and verify
   - Allows post-processing and analysis
   - Can generate statistics

2. **In-Memory Buffer**: Keep data in circular buffer
   - Query output via debug commands
   - Minimal disk I/O
   - Useful for testing

3. **Future Extensions**: Print-to-PDF, formatted output, network printing

---

## Part 4: Critical Implementation Details

### 4.1 Timing and Handshaking

The parallel port uses a strobe-based protocol:

1. **Host writes data** to port 0x378
2. **Host pulses STROBE** (bit 0 of control register) - active low pulse
3. **Printer receives data** and sets BUSY signal
4. **Printer processes** data
5. **Printer clears BUSY** and briefly pulses ACK
6. **Host reads status** to confirm completion

**Implementation Requirement**:
- Track STROBE pulses (transitions from 1→0→1)
- Respect minimum pulse width (typically >0.5µs real time)
- Update status flags realistically (BUSY delays, ACK pulses)
- Simulate realistic timeouts

### 4.2 Status Register Management

```cpp
void UpdateStatusRegister() {
    // Simulate printer readiness
    status_reg.busy = (buffer_has_data && !finished_processing);
    status_reg.ack = (last_byte_processed);  // Brief pulse
    status_reg.paper_out = false;  // Simulated printer always has paper
    status_reg.select_in = true;   // Printer always online
    status_reg.error = (error_condition);
    status_reg.irq = (can_interrupt);
}
```

### 4.3 Buffer Management

**Strategy**:
- Maintain circular buffer to accumulate bytes between flushes
- Flush on:
  - Explicit INITIALIZE signal
  - Buffer full (e.g., 4KB)
  - Timeout (e.g., 100ms between bytes)
  - Application calls FLUSH operation

**Rationale**: Prevents thousands of small disk writes

### 4.4 Multiple LPT Ports

Support LPT1 (0x378), LPT2 (0x278), LPT3 (0x3BC):
- Query BIOS data area for active ports
- Create separate printer instance per active port
- Allow configuration of which port to use

---

## Part 5: Implementation Checklist

### Phase 1: Core Infrastructure
- [ ] Create `src/hardware/printer/printer.{h,cpp}`
- [ ] Define ParallelPortPrinter base class
- [ ] Implement port handler registration
- [ ] Implement file output mechanism

### Phase 2: Integration
- [ ] Add printer instance to global initialization
- [ ] Enhance INT17 BIOS handler
- [ ] Add printer configuration section
- [ ] Update device_LPT1 Write method
- [ ] Add PRINTER_Init/Destroy to dosbox.cpp

### Phase 3: Features
- [ ] Support multiple LPT ports
- [ ] Implement status register simulation
- [ ] Add timeout handling
- [ ] Add buffer flushing strategies
- [ ] Add debug/logging output

### Phase 4: Testing & Refinement
- [ ] Test with simple BASIC printer output program
- [ ] Test with Turbo Pascal printer example
- [ ] Verify file output correctness
- [ ] Test timeout scenarios
- [ ] Test INITIALIZE signal behavior

---

## Part 6: Expected Output Format

### File Output Example

When a DOS program prints "Hello, DOSBox!", the printer.txt file would contain:
```
Hello, DOSBox!
```

Raw bytes are preserved, so escape sequences (ESC/P for Epson, etc.) will be in the file as-is, allowing:
- Analysis of print commands
- Conversion to other formats
- Debugging of printer driver behavior

---

## Part 7: Comparison with DosBox-X Approach

DosBox-X has a more comprehensive printing implementation with:
- Support for actual Windows printer drivers
- ESC/P printer emulation
- PDF generation
- Network printing

**For DOSBox-staging**, we recommend starting simpler:
- File output only (no driver integration)
- Raw byte output (no ESC/P interpretation)
- Focus on INT17 compatibility and port I/O

This achieves 80% of functionality with 20% of complexity, suitable for testing without physical hardware.

---

## Part 8: Configuration Example

```ini
# dosbox.conf
[printer]
# Enable printer output to file
printer=file
# Where to write printer output
printer_output=./printer_output.txt

# Example: Multiple ports
[printer2]
printer=file
printer_output=./printer_output_lpt2.txt
```

---

## References

1. **Parallel Port Protocol**: https://wiki.osdev.org/Parallel_port
2. **IBM BIOS INT 17h**: https://en.wikipedia.org/wiki/INT_17h
3. **LPT DAC Implementation**: Excellent example of port binding pattern
4. **DOSBox-X Printer**: Reference for comprehensive implementation
5. **Centronics Protocol**: Standard printer interface documentation

---

## Next Steps

1. Review this plan with the team
2. Decide on file output vs. other output methods
3. Create initial printer.h/printer.cpp skeleton
4. Implement file writing mechanism
5. Connect INT17 handler
6. Test with simple DOS printer programs
