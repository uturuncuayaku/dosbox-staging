# Printer Implementation - Quick Reference & Code Examples

## Quick Navigation Guide

### Existing Code to Review

| Component | File | Key Lines | Purpose |
|-----------|------|-----------|---------|
| **LPT Port Definitions** | `src/hardware/lpt.h` | 1-87 | Register structures & port addresses |
| **LPT DAC Init Pattern** | `src/hardware/audio/lpt_dac.cpp` | 140-250 | Pattern for hardware init/destroy |
| **Disney Implementation** | `src/hardware/audio/disney.cpp` | 1-100 | Example of port binding |
| **INT17 Handler (Current)** | `src/ints/bios.cpp` | 485-500 | Stub implementation to enhance |
| **DOS Device LPT1** | `src/dos/dos_devices.cpp` | 300-340 | Device class to enhance |
| **Speaker Init** | `src/hardware/audio/speaker.cpp` | 30-65 | Pattern for module init integration |

---

## Code Snippets - Quick Start

### 1. Basic Printer Class Skeleton

```cpp
// file: src/hardware/printer/printer.h
#ifndef DOSBOX_PRINTER_H
#define DOSBOX_PRINTER_H

#include "config/setup.h"
#include "hardware/lpt.h"
#include "hardware/port.h"

#include <memory>
#include <string>
#include <vector>

enum class PrinterMode {
    None,
    File,
    Memory
};

class ParallelPortPrinter {
public:
    ParallelPortPrinter(std::string_view output_path);
    ~ParallelPortPrinter();
    
    // Core operations
    void WriteCharacter(uint8_t byte);
    void InitializePrinter();
    uint8_t GetStatus() const;
    
    // Configuration
    void BindToPort(const io_port_t lpt_port);
    void SetMode(PrinterMode mode);
    
    // Prevent copying
    ParallelPortPrinter(const ParallelPortPrinter&) = delete;
    ParallelPortPrinter& operator=(const ParallelPortPrinter&) = delete;

private:
    // Port handlers
    void HandleDataWrite(const io_port_t port, const io_val_t value);
    uint8_t HandleStatusRead(const io_port_t port);
    void HandleControlWrite(const io_port_t port, const io_val_t value);
    
    // Output operations
    void FlushBuffer();
    
    // State
    LptStatusRegister status_reg;
    LptControlRegister control_reg;
    LptControlRegister prev_control_reg;
    uint8_t data_reg = 0;
    
    std::vector<uint8_t> buffer;
    std::string output_file;
    PrinterMode mode = PrinterMode::None;
    
    // Port handlers (from hardware/port.h pattern)
    IO_WriteHandleObject data_write_handler;
    IO_ReadHandleObject status_read_handler;
    IO_WriteHandleObject control_write_handler;
    
    // Timing
    double last_byte_time_ms = 0.0;
    static constexpr int BUFFER_SIZE = 4096;
    static constexpr int FLUSH_TIMEOUT_MS = 100;
};

// Global management functions
void PRINTER_Init(SectionProp& section);
void PRINTER_Destroy();
void PRINTER_AddConfigSection(Section* sec);
void PRINTER_NotifySettingUpdated(SectionProp& section, 
                                   const std::string& prop_name);

#endif // DOSBOX_PRINTER_H
```

### 2. Basic Implementation Skeleton

```cpp
// file: src/hardware/printer/printer.cpp
#include "hardware/printer/printer.h"

#include "config/setup.h"
#include "hardware/pic.h"
#include "misc/support.h"

#include <cstdio>
#include <cstring>

static std::unique_ptr<ParallelPortPrinter> printer_instance = nullptr;

// ========== ParallelPortPrinter Implementation ==========

ParallelPortPrinter::ParallelPortPrinter(std::string_view output_path)
    : output_file(output_path)
{
    LOG_MSG("PRINTER: Initializing parallel port printer");
    LOG_MSG("PRINTER: Output file: %s", output_file.c_str());
    
    // Initialize status register
    status_reg.busy = false;
    status_reg.ack = false;
    status_reg.paper_out = false;
    status_reg.select_in = true;
    status_reg.error = false;
    status_reg.irq = false;
    
    buffer.reserve(BUFFER_SIZE);
}

ParallelPortPrinter::~ParallelPortPrinter()
{
    LOG_MSG("PRINTER: Shutting down");
    
    // Flush remaining data
    if (!buffer.empty()) {
        FlushBuffer();
    }
    
    // Uninstall port handlers
    data_write_handler.Uninstall();
    status_read_handler.Uninstall();
    control_write_handler.Uninstall();
}

void ParallelPortPrinter::BindToPort(const io_port_t lpt_port)
{
    using namespace std::placeholders;
    
    LOG_MSG("PRINTER: Binding to LPT port 0x%X", lpt_port);
    
    // Register port handlers
    const auto write_data = std::bind(&ParallelPortPrinter::HandleDataWrite, 
                                      this, _1, _2);
    const auto read_status = std::bind(&ParallelPortPrinter::HandleStatusRead, 
                                       this, _1);
    const auto write_control = std::bind(&ParallelPortPrinter::HandleControlWrite, 
                                         this, _1, _2);
    
    // Install handlers for data (offset 0), status (offset 1), control (offset 2)
    data_write_handler.Install(lpt_port, write_data, io_width_t::byte);
    status_read_handler.Install(static_cast<io_port_t>(lpt_port + 1), 
                                read_status, io_width_t::byte);
    control_write_handler.Install(static_cast<io_port_t>(lpt_port + 2), 
                                  write_control, io_width_t::byte);
}

void ParallelPortPrinter::HandleDataWrite(const io_port_t port, 
                                          const io_val_t value)
{
    data_reg = static_cast<uint8_t>(value);
    LOG(LOG_PARALLEL, LOG_NORMAL)("PRINTER: Data write 0x%02X (%c)", 
                                   data_reg, 
                                   (data_reg >= 32 && data_reg < 127) ? data_reg : '.');
}

uint8_t ParallelPortPrinter::HandleStatusRead(const io_port_t port)
{
    LOG(LOG_PARALLEL, LOG_NORMAL)("PRINTER: Status read = 0x%02X", 
                                   status_reg.data);
    return status_reg.data;
}

void ParallelPortPrinter::HandleControlWrite(const io_port_t port, 
                                             const io_val_t value)
{
    LptControlRegister new_control;
    new_control.data = static_cast<uint8_t>(value);
    
    LOG(LOG_PARALLEL, LOG_NORMAL)("PRINTER: Control write 0x%02X (STROBE=%d)", 
                                   new_control.data, new_control.strobe);
    
    // Detect STROBE pulse (1→0 transition)
    if (prev_control_reg.strobe && !new_control.strobe) {
        LOG(LOG_PARALLEL, LOG_VERBOSE)("PRINTER: STROBE pulse detected");
        
        // Add byte to buffer
        buffer.push_back(data_reg);
        
        // Set BUSY signal
        status_reg.busy = true;
        
        // Simulate brief ACK pulse
        status_reg.ack = true;
        
        // Check if buffer needs flushing
        if (buffer.size() >= BUFFER_SIZE) {
            FlushBuffer();
        }
    }
    
    // INITIALIZE signal (active low)
    if (!new_control.initialize && prev_control_reg.initialize) {
        LOG(LOG_PARALLEL, LOG_VERBOSE)("PRINTER: Initialize signal detected");
        InitializePrinter();
    }
    
    // Store current state for next cycle
    prev_control_reg = new_control;
    control_reg = new_control;
}

void ParallelPortPrinter::WriteCharacter(uint8_t byte)
{
    HandleDataWrite(0, byte);
    
    // When called via INT17h, we simulate strobe internally
    status_reg.busy = true;
    buffer.push_back(byte);
    
    if (buffer.size() >= BUFFER_SIZE) {
        FlushBuffer();
    }
    
    status_reg.busy = false;
}

void ParallelPortPrinter::InitializePrinter()
{
    LOG_MSG("PRINTER: Initialize (reset)");
    
    // Flush any buffered data
    if (!buffer.empty()) {
        FlushBuffer();
    }
    
    // Reset status
    status_reg.busy = false;
    status_reg.ack = false;
    status_reg.error = false;
}

uint8_t ParallelPortPrinter::GetStatus() const
{
    return status_reg.data;
}

void ParallelPortPrinter::SetMode(PrinterMode mode)
{
    this->mode = mode;
    LOG_MSG("PRINTER: Mode set to %d", static_cast<int>(mode));
}

void ParallelPortPrinter::FlushBuffer()
{
    if (buffer.empty()) {
        return;
    }
    
    if (mode == PrinterMode::File) {
        FILE* f = fopen(output_file.c_str(), "ab");  // Append binary
        if (f) {
            fwrite(buffer.data(), 1, buffer.size(), f);
            fclose(f);
            LOG(LOG_PARALLEL, LOG_VERBOSE)("PRINTER: Flushed %zu bytes to file", 
                                           buffer.size());
        } else {
            LOG_WARNING("PRINTER: Failed to open output file: %s", 
                       output_file.c_str());
        }
    } else if (mode == PrinterMode::Memory) {
        // In-memory buffer - could log or store elsewhere
        LOG(LOG_PARALLEL, LOG_VERBOSE)("PRINTER: Buffered %zu bytes in memory", 
                                       buffer.size());
    }
    
    buffer.clear();
}

// ========== Configuration & Lifecycle ==========

static void init_printer_settings(SectionProp& section)
{
    using enum Property::Changeable::Value;
    
    auto pstring = section.AddString("printer", WhenIdle, "none");
    pstring->SetHelp(
        "Enable printer emulation ('none' by default). Possible values:\n"
        "\n"
        "  none/off:   Don't emulate a printer (default).\n"
        "  file:       Output to file specified in printer_output.\n"
        "  memory:     Keep output in memory buffer.\n");
    pstring->SetValues({"none", "file", "memory", "off"});
    
    auto pstring_output = section.AddString("printer_output", WhenIdle, 
                                            "printer_output.txt");
    pstring_output->SetHelp("File path for printer output when printer=file.");
}

void PRINTER_Init(SectionProp& section)
{
    const std::string printer_choice = section.GetString("printer");
    
    if (printer_choice == "none" || printer_choice == "off") {
        LOG_MSG("PRINTER: Disabled");
        return;
    }
    
    // Get output file path
    const auto output_path = section.GetString("printer_output");
    
    // Create printer instance
    printer_instance = std::make_unique<ParallelPortPrinter>(output_path);
    
    // Set mode
    PrinterMode mode = PrinterMode::None;
    if (printer_choice == "file") {
        mode = PrinterMode::File;
    } else if (printer_choice == "memory") {
        mode = PrinterMode::Memory;
    }
    printer_instance->SetMode(mode);
    
    // Bind to LPT1 (primary port)
    printer_instance->BindToPort(Lpt1Port);
    
    LOG_MSG("PRINTER: Initialized successfully");
}

void PRINTER_Destroy()
{
    if (printer_instance) {
        printer_instance.reset();
        LOG_MSG("PRINTER: Destroyed");
    }
}

void PRINTER_AddConfigSection(Section* sec)
{
    assert(sec);
    const auto section = static_cast<SectionProp*>(sec);
    init_printer_settings(*section);
}

void PRINTER_NotifySettingUpdated(SectionProp& section, 
                                   const std::string& prop_name)
{
    if (prop_name == "printer" || prop_name == "printer_output") {
        PRINTER_Destroy();
        PRINTER_Init(section);
    }
}
```

---

### 3. Enhanced INT17 BIOS Handler

```cpp
// Modification to: src/ints/bios.cpp - Replace INT17_Handler

static Bitu INT17_Handler(void) {
    LOG(LOG_BIOS, LOG_NORMAL)("INT17: Function 0x%02X on port %d", 
                              reg_ah, reg_dx);
    
    // Get the LPT port address from BIOS data area
    // DX contains logical port number (0=LPT1, 1=LPT2, 2=LPT3)
    uint16_t port = real_readw(0x40, 0x08 + (reg_dx * 2));  // 0x408, 0x40A, 0x40C
    
    if (port == 0) {
        LOG(LOG_BIOS, LOG_NORMAL)("INT17: LPT%d not available", reg_dx + 1);
        reg_ah = 1;  // Timeout error
        return CBRET_NONE;
    }
    
    // Forward to printer if available
    if (!printer_instance) {
        // Printer not initialized
        reg_ah = 1;  // Timeout error
        return CBRET_NONE;
    }
    
    switch (reg_ah) {
    case 0x00: {  // PRINTER: Write Character
        // AL contains character to print
        printer_instance->WriteCharacter(reg_al);
        
        // Return status in AH
        reg_ah = printer_instance->GetStatus();
        break;
    }
    case 0x01: {  // PRINTER: Initialize port
        printer_instance->InitializePrinter();
        reg_ah = printer_instance->GetStatus();
        break;
    }
    case 0x02: {  // PRINTER: Get Status
        reg_ah = printer_instance->GetStatus();
        break;
    }
    case 0x03: {  // PRINTER: Get port address (extended)
        // Return port address in AX
        reg_ax = port;
        break;
    }
    default:
        LOG(LOG_BIOS, LOG_WARN)("INT17: Unsupported function 0x%02X", reg_ah);
        reg_ah = 1;  // Timeout/error
    }
    
    return CBRET_NONE;
}
```

---

### 4. Enhanced DOS Device LPT1 Write

```cpp
// Modification to: src/dos/dos_devices.cpp - device_LPT1 class

class device_LPT1 final : public device_NUL {
public:
    device_LPT1() { SetName("LPT1"); }
    
    uint16_t GetInformation() override { return 0x80A0; }
    
    bool Read(uint8_t* /*data*/, uint16_t* /*size*/) override {
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
    
    bool Seek(uint32_t* /*pos*/, uint32_t /*type*/) override {
        return true;  // Printers don't seek
    }
    
    void Close() override {
        // Could flush here if needed
    }
    
    // Keep other methods from device_NUL...
};
```

---

### 5. Integration into SPEAKER_Init

```cpp
// Modification to: src/hardware/audio/speaker.cpp

// In SPEAKER_AddConfigSection():
void SPEAKER_AddConfigSection(const ConfigPtr& conf)
{
    assert(conf);

    // Create [speaker] section
    auto section = conf->AddSection("speaker");
    section->AddUpdateHandler(notify_speaker_setting_updated);

    LPTDAC_AddConfigSection(section);
    PCSPEAKER_AddConfigSection(section);
    PS1AUDIO_AddConfigSection(section);
    TANDYSOUND_AddConfigSection(section);
    // PRINTER_AddConfigSection(section);  // [NEW] Add this if using [speaker]

    init_speaker_settings(*section);
}

// OR create separate [printer] section:
void PRINTER_AddConfigSection(const ConfigPtr& conf)
{
    assert(conf);
    
    auto section = conf->AddSection("printer");
    section->AddUpdateHandler(PRINTER_NotifySettingUpdated);
    
    PRINTER_AddConfigSection(section);
}
```

---

### 6. Initialization in dosbox.cpp

```cpp
// Additions to: src/dosbox.cpp

// Add includes
#include "hardware/printer/printer.h"

// In DOSBOX_Init() or similar initialization function:
void Initialize_Emulation() {
    // ... existing initialization ...
    
    // Initialize printer
    PRINTER_Init(get_section("printer"));  // After config is loaded
    
    // ... rest of initialization ...
}

// In cleanup function:
void Deinit_Emulation() {
    // ... existing cleanup ...
    
    PRINTER_Destroy();
    
    // ... rest of cleanup ...
}
```

---

## Testing Code Examples

### Test 1: Simple BASIC Program

```basic
' test_printer.bas
10 PRINT "Testing DOSBox Printer"
20 LPRINT "This should appear in printer output"
30 LPRINT "Line 2"
40 LPRINT ""
50 LPRINT "Testing complete"
60 END
```

### Test 2: Assembly Test (inline with DEBUG.COM)

```
a 100
mov ax, 0x1700    ; INT 17h function 00h (write)
mov al, 'A'       ; Character 'A'
mov dx, 0         ; LPT1 (0=LPT1, 1=LPT2, 2=LPT3)
int 17            ; Call BIOS printer interrupt
mov ah, 0x02      ; Function 02h (get status)
int 17            ; Call BIOS
mov ax, 0x4c00    ; Exit
int 21
```

### Test 3: Turbo Pascal Test

```pascal
PROGRAM PrintTest;
BEGIN
  WriteLn('Testing printer from Turbo Pascal');
  Assign(lst, 'LPT1');
  Rewrite(lst);
  WriteLn(lst, 'Output to LPT1');
  WriteLn(lst, 'Second line');
  Close(lst);
  WriteLn('Done');
END.
```

---

## CMakeLists.txt Integration

Add printer source files to build:

```cmake
# In src/hardware/CMakeLists.txt, add:

set(PRINTER_SOURCES
    printer/printer.h
    printer/printer.cpp
)

target_sources(dosbox PRIVATE
    ${AUDIO_SOURCES}
    ${PRINTER_SOURCES}  # NEW
    # ... other sources
)
```

---

## Configuration File Example

```ini
# dosbox.conf

[cpu]
core=auto
type=auto

[speaker]
lpt_dac=none
rate=22050

[printer]
printer=file
printer_output=./printer_output.txt

[dos]
xms=true
ems=true
```

---

## Logging Macros Usage

Already defined in codebase:

```cpp
LOG_MSG("PRINTER: Message");                    // Always shown
LOG(LOG_PARALLEL, LOG_NORMAL)("Status: 0x%02X", status);
LOG(LOG_PARALLEL, LOG_VERBOSE)("Debug info");
LOG_WARNING("PRINTER: Warning message");
LOG_ERROR("PRINTER: Error message");
```

Note: May need to define `LOG_PARALLEL` in `misc/debug.h` if not present.

---

## Key Patterns from Codebase to Follow

### Pattern 1: Port Handler Registration
```cpp
void BindHandlers(const io_port_t lpt_port, 
                  const io_write_f write_data,
                  const io_read_f read_status,
                  const io_write_f write_control)
{
    data_write_handler.Install(lpt_port, write_data, io_width_t::byte);
    status_read_handler.Install(lpt_port + 1, read_status, io_width_t::byte);
    control_write_handler.Install(lpt_port + 2, write_control, io_width_t::byte);
}
```

### Pattern 2: Lambda Binding
```cpp
using namespace std::placeholders;
const auto write_data = std::bind(&Disney::WriteData, this, _1, _2, _3);
data_write_handler.Install(lpt_port, write_data, io_width_t::byte);
```

### Pattern 3: Configuration Setting
```cpp
auto pstring = section.AddString("setting_name", WhenIdle, "default_value");
pstring->SetHelp("Help text");
pstring->SetValues({"option1", "option2", "option3"});
```

### Pattern 4: Lifecycle Management
```cpp
static std::unique_ptr<MyDevice> device_instance = nullptr;

void Init(SectionProp& section) {
    device_instance = std::make_unique<MyDevice>();
    device_instance->Initialize();
}

void Destroy() {
    if (device_instance) {
        device_instance.reset();
    }
}
```

---

## Header Dependencies

Standard includes needed:
```cpp
#include "config/setup.h"       // Configuration
#include "hardware/lpt.h"       // LPT register definitions
#include "hardware/port.h"      // IO_WriteHandleObject, etc.
#include "misc/support.h"       // LOG macros
#include "utils/checks.h"       // CHECK_NARROWING()

#include <cstdio>               // FILE, fopen, fwrite
#include <string>               // std::string
#include <vector>               // std::vector
#include <memory>               // std::unique_ptr
#include <functional>           // std::bind, std::function
```

---

## Next Steps Checklist

- [ ] Create `src/hardware/printer/` directory
- [ ] Create `printer.h` and `printer.cpp`
- [ ] Implement ParallelPortPrinter class
- [ ] Modify `src/ints/bios.cpp` INT17 handler
- [ ] Modify `src/dos/dos_devices.cpp` device_LPT1
- [ ] Add configuration section
- [ ] Update CMakeLists.txt
- [ ] Compile and test
- [ ] Test with BASIC, assembly, Pascal programs
- [ ] Verify file output
- [ ] Add documentation
