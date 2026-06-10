# Printer Implementation: Friction Analysis & Required Extensions

## Overview

This document identifies the architectural conflicts, dependencies, and required extensions for integrating the printer implementation with existing DOSBox systems. The current LPT DAC (audio) implementation and printer share infrastructure, requiring careful coordination.

---

## Part 1: Identified Friction Points

### 1.1 **INT17 BIOS Handler - Completely Decoupled**

**Location**: `src/ints/bios.cpp` (lines 487-501)

**Current State**:
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

**Friction Point**: Handler is stub implementation - always reports timeout. No interaction with actual printer device.

**Required Extension**:
```cpp
// Need to add:
// 1. Forward declare printer device accessor
// 2. Query BIOS data area for active LPT port
// 3. Route INT17 calls to printer instance
// 4. Return actual printer status

extern ParallelPortPrinter* GetPrinterInstance(uint8_t lpt_index);

static Bitu INT17_Handler(void) {
    LOG(LOG_BIOS,LOG_NORMAL)("INT17:Function %X (DX=%X)",reg_ah, reg_dx);
    
    // reg_dx = LPT index (0=LPT1, 1=LPT2, 2=LPT3)
    auto printer = GetPrinterInstance(reg_dx);
    if (!printer) {
        reg_ah = 0x01;  // Timeout - port not present
        return CBRET_NONE;
    }
    
    switch (reg_ah) {
    case 0x00:  // Write character (AL = character to print)
        printer->WriteCharacter(reg_al);
        reg_ah = printer->GetStatus();
        break;
    case 0x01:  // Initialize port
        printer->InitializePrinter();
        reg_ah = printer->GetStatus();
        break;
    case 0x02:  // Get status
        reg_ah = printer->GetStatus();
        break;
    };
    return CBRET_NONE;
}
```

**Complexity**: **HIGH** - Requires printer instance management, lifetime coordination

---

### 1.2 **device_LPT1 DOS Device - No Write Support**

**Location**: `src/dos/dos_devices.cpp` (lines 296-310)

**Current State**:
```cpp
class device_LPT1 final : public device_NUL {
public:
    device_LPT1() { SetName("LPT1"); }
    uint16_t GetInformation() override { return 0x80A0; }
    bool Read(uint8_t* /*data*/, uint16_t* /*size*/) override {
        DOS_SetError(DOSERR_ACCESS_DENIED);
        return false;
    }
    // NO Write() override - falls back to device_NUL
};
```

**Friction Point**: `Write()` is not overridden. Calls fall back to base class `device_NUL` which likely returns error.

**Required Extension**:
```cpp
class device_LPT1 final : public device_NUL {
public:
    device_LPT1() { SetName("LPT1"); }
    
    uint16_t GetInformation() override { return 0x80A0; }
    
    bool Read(uint8_t* /*data*/, uint16_t* /*size*/) override {
        DOS_SetError(DOSERR_ACCESS_DENIED);
        return false;
    }
    
    // NEW: Write support for DOS programs
    bool Write(uint8_t* data, uint16_t* size) override {
        auto printer = GetPrinterInstance(0);  // LPT1 = index 0
        if (!printer) {
            DOS_SetError(DOSERR_DEVICE_FAULT);
            return false;
        }
        
        for (uint16_t i = 0; i < *size; ++i) {
            printer->WriteCharacter(data[i]);
        }
        return true;
    }
};
```

**Complexity**: **MEDIUM** - Straightforward override, but needs printer accessor

---

### 1.3 **Port Detection & BIOS Initialization - Port Reading Conflict**

**Location**: `src/ints/bios.cpp` (lines 1437-1468)

**Current State**:
```cpp
/* detect parallel ports */
Bitu ppindex=0;
if ((IO_Read(0x378) != 0xff) || (IO_Read(0x379) != 0xff)) {
    // this is our LPT1
    mem_writew(BIOS_ADDRESS_LPT1, 0x378);
    ppindex++;
    // ... more detection logic
}
```

**Friction Point**: 
- BIOS initialization reads from ports (0x378, 0x379, 0x3BC) to detect physical presence
- LPT DAC has handlers installed on these ports by this point
- Printer device also registers handlers on same ports
- The `IO_Read()` will call the registered handler (either LPT DAC or Printer, depending on which initialized first)
- This returns data, not 0xff, causing port to be marked as present even if printer not configured

**Execution Timeline**:
```
1. DOSBOX_Init() called
   ├─ IO_Init()
   ├─ ... (other init)
   └─ Eventually SPEAKER_Init() 
      └─ LPTDAC_Init()
         └─ lpt_dac->BindToPort(Lpt1Port)  ← Handlers installed on 0x378/0x379/0x37A

2. BIOS_Init() called (after speakers)
   ├─ Performs port detection
   └─ IO_Read(0x378) calls LPT DAC's status handler!  ← Returns non-0xff
      → BIOS marks LPT1 as present
```

**Required Extension**:

Option A (Preferred): **Let printer use different port**
```cpp
// In printer device initialization:
printer->BindToPort(Lpt2Port);  // Use 0x278 instead of 0x378
// BIOS detection will check 0x278/0x279 and find printer instead
```

Option B: **Coordinate with BIOS detection**
```cpp
// Printer needs to signal its presence to BIOS data area
// But this creates timing dependencies - what if BIOS init runs before printer init?

// During printer init:
mem_writew(BIOS_ADDRESS_LPT1, Lpt1Port);  // Register with BIOS
```

Option C: **Make BIOS detection aware of devices**
```cpp
// Add a "is this port claimed by a device" check
bool IsPortClaimedByDevice(io_port_t port);

// In BIOS detection:
if (!IsPortClaimedByDevice(0x378)) {
    if ((IO_Read(0x378) != 0xff) || (IO_Read(0x379) != 0xff)) {
        mem_writew(BIOS_ADDRESS_LPT1, 0x378);
    }
}
```

**Complexity**: **HIGH** - Requires coordination between initialization systems

---

### 1.4 **Configuration System - No Printer Section**

**Location**: `src/dosbox.cpp` (line 1090), `src/hardware/audio/speaker.cpp` (lines 56-67)

**Current State**:
- Configuration sections are registered in `DOSBOX_Config()` function
- Audio devices (LPT DAC, PC Speaker, Tandy, etc.) are in `[speaker]` section
- Each device has `AddConfigSection()`, `Init()`, `Destroy()`, and `NotifySettingUpdated()` pattern

**Friction Points**:
1. **Printer not in any config section** - No configuration support
2. **Multiple ports need separate configuration** - LPT1, LPT2, LPT3 might need independent settings
3. **Update handler missing** - Can't reconfigure printer at runtime

**Required Extension**:

```cpp
// File: src/hardware/printer/printer.h
void PRINTER_AddConfigSection(Section* sec);
void PRINTER_Init(SectionProp& section);
void PRINTER_Destroy();
void PRINTER_NotifySettingUpdated(SectionProp& section, const std::string& prop_name);

// In src/dosbox.cpp, add alongside SPEAKER_AddConfigSection():
PRINTER_AddConfigSection(control);

// Configuration example:
[printer]
; Enable printer on LPT port (none/lpt1/lpt2/lpt3)
printer=none
; Output file for printer data
printer_output=./printer.txt
; Optional: enable per-port settings
printer_lpt2=none
printer_lpt3=none
```

**Complexity**: **MEDIUM** - Standard pattern but needs multiple variants

---

### 1.5 **Lifecycle Management - Singleton vs. Multiple Instances**

**Location**: `src/hardware/audio/lpt_dac.cpp` (line 164)

**Current State**:
```cpp
static std::unique_ptr<LptDac> lpt_dac = {};  // SINGLETON

void LPTDAC_Init(SectionProp& section) {
    // ... 
    lpt_dac = std::make_unique<Disney>();   // Only ONE instance
    lpt_dac->BindToPort(Lpt1Port);           // Always LPT1
}
```

**Friction Point**: 
- LPT DAC is a **singleton** - only one device, always on LPT1
- Printer needs to support **multiple instances** - one per active LPT port
- Need to track instance pointers for retrieval by INT17 handler

**Required Extension**:

```cpp
// src/hardware/printer/printer.h
class PrinterManager {
private:
    static constexpr size_t MaxPrinters = 3;
    std::array<std::unique_ptr<ParallelPortPrinter>, MaxPrinters> printers;
    
public:
    ParallelPortPrinter* GetPrinter(uint8_t lpt_index);
    void SetPrinter(uint8_t lpt_index, std::unique_ptr<ParallelPortPrinter> printer);
};

// Global instance
static PrinterManager printer_manager;

// src/hardware/printer/printer.cpp
void PRINTER_Init(SectionProp& section) {
    // Check if LPT1 printer enabled
    bool enable_lpt1 = section.GetBool("printer_enable_lpt1");
    if (enable_lpt1) {
        auto printer = std::make_unique<ParallelPortPrinter>("lpt1_output.txt");
        printer->BindToPort(Lpt1Port);
        printer_manager.SetPrinter(0, std::move(printer));
    }
    
    // Check if LPT2 printer enabled
    bool enable_lpt2 = section.GetBool("printer_enable_lpt2");
    if (enable_lpt2) {
        auto printer = std::make_unique<ParallelPortPrinter>("lpt2_output.txt");
        printer->BindToPort(Lpt2Port);
        printer_manager.SetPrinter(1, std::move(printer));
    }
    
    // ... etc for LPT3
}

extern ParallelPortPrinter* GetPrinterInstance(uint8_t lpt_index) {
    return printer_manager.GetPrinter(lpt_index);
}
```

**Complexity**: **MEDIUM** - Requires array-based tracking

---

### 1.6 **Thread Safety & Mixer Locking**

**Location**: `src/hardware/audio/lpt_dac.cpp` (lines 204, 219, 250-252)

**Current State**:
```cpp
void LPTDAC_Init(SectionProp& section) {
    // ...
    MIXER_LockMixerThread();  // ← Lock mixer
    lpt_dac = std::make_unique<Disney>();
    // ...
    TIMER_AddTickHandler(lpt_dac_callback);
    MIXER_UnlockMixerThread();  // ← Unlock
}

void LPTDAC_Destroy() {
    if (lpt_dac) {
        MIXER_LockMixerThread();
        TIMER_DelTickHandler(lpt_dac_callback);
        lpt_dac.reset();
        MIXER_UnlockMixerThread();
    }
}
```

**Friction Point**: 
- LPT DAC uses mixer locks because it feeds audio to mixer
- Printer doesn't use mixer, but port handlers are called from CPU thread
- INT17 and port handlers could be called concurrently with initialization

**Required Extension**:

```cpp
// Printer needs thread-safe instance access
static std::mutex printer_mutex;

ParallelPortPrinter* GetPrinterInstance(uint8_t lpt_index) {
    std::lock_guard<std::mutex> lock(printer_mutex);
    return printer_manager.GetPrinter(lpt_index);
}

// Port handlers need protection
void Printer::WriteData(const io_port_t port, const io_val_t value, const io_width_t width) {
    std::lock_guard<std::mutex> lock(printer_mutex);
    data_reg = check_cast<uint8_t>(value);
}

// Initialization doesn't need mixer lock (no mixer interaction)
void PRINTER_Init(SectionProp& section) {
    std::lock_guard<std::mutex> lock(printer_mutex);
    // Initialize printers
}
```

**Complexity**: **MEDIUM** - Requires mutex protection for data structures

---

### 1.7 **Port Handler Conflict Resolution**

**Location**: `src/hardware/port.h` (lines 124-160), `src/hardware/port_containers.cpp`

**Current State**:
- Only ONE handler can be registered per port
- If handler already registered, `IO_RegisterWriteHandler` overwrites it

**Friction Point**:
```cpp
// Both LPT DAC and Printer try to register on same port
lpt_dac->BindToPort(Lpt1Port);      // Registers handlers on 0x378, 0x379, 0x37A
printer->BindToPort(Lpt1Port);      // OVERWRITES the LPT DAC handlers!
```

**Solution**:
- **Use different ports**: LPT DAC on 0x378, Printer on 0x278 or 0x3BC
- Configuration determines which port each device uses

**Required Extension**:
```cpp
// Configuration must specify ports:
[speaker]
lpt_dac=disney
lpt_dac_port=lpt1     ; Can be lpt1, lpt2, lpt3

[printer]
enabled=true
lpt_port=lpt2         ; Different port to avoid conflict
```

**Complexity**: **LOW** - Just use configuration to assign different ports

---

### 1.8 **Timeout Simulation**

**Location**: `src/ints/bios.h` (lines 63-67)

**Current State**:
```cpp
#define BIOS_LPT1_TIMEOUT               0x478
#define BIOS_LPT2_TIMEOUT               0x479
#define BIOS_LPT3_TIMEOUT               0x47a
```

**Friction Point**:
- BIOS has timeout registers per LPT port
- Printer needs to implement realistic timeout behavior
- INT17 handler should check timeout values

**Required Extension**:

```cpp
// In ParallelPortPrinter:
class ParallelPortPrinter {
private:
    uint32_t timeout_ms = 0;
    uint32_t last_byte_time = 0;
    
public:
    bool IsTimeout() {
        if (timeout_ms == 0) return false;  // No timeout
        
        auto now = PIC_FullIndex();
        return (now - last_byte_time) > timeout_ms;
    }
    
    uint8_t GetStatus() {
        status_reg.busy = IsTimeout() ? 1 : 0;
        return status_reg.data;
    }
};

// In INT17 handler:
if (printer->IsTimeout()) {
    reg_ah |= 0x01;  // Set timeout bit
}
```

**Complexity**: **MEDIUM** - Requires timing integration with PIC system

---

## Part 2: Extension Requirements Summary

### 2.1 **ParallelPortPrinter Class Extensions**

Required methods beyond basic byte capture:

```cpp
class ParallelPortPrinter {
    // ===== EXISTING (from byte capture document) =====
    void WriteData(const io_port_t, const io_val_t, const io_width_t);
    uint8_t ReadStatus(const io_port_t, const io_width_t);
    void WriteControl(const io_port_t, const io_val_t, const io_width_t);
    void BindToPort(const io_port_t lpt_port);
    void FlushBuffer();
    
    // ===== NEW REQUIRED EXTENSIONS =====
    
    // For INT17 support
    void InitializePrinter();           // INT17 AH=01
    uint8_t GetStatus();                // Return status register for INT17 AH=02
    void WriteCharacter(uint8_t byte);  // INT17 AH=00
    
    // For timeout handling
    bool IsTimeout();
    void SetTimeoutValue(uint8_t timeout_ms);
    
    // For lifecycle management
    bool IsInitialized() const;
    
    // For configuration
    void SetOutputFile(const std::string& filename);
    void SetMode(PrinterMode mode);
    
    // For status tracking
    uint32_t GetBytesSent() const;
    uint32_t GetTimeout() const;
};
```

---

### 2.2 **System Integration Points**

```cpp
// ===== BIOS INT17 Handler =====
// File: src/ints/bios.cpp
// Add extern declaration
extern ParallelPortPrinter* GetPrinterInstance(uint8_t lpt_index);

// Modify INT17_Handler() to route to printer

// ===== DOS Device =====
// File: src/dos/dos_devices.cpp
// Add Write() override to device_LPT1

// ===== Printer Manager =====
// File: src/hardware/printer/printer_manager.h (NEW)
class PrinterManager {
    std::array<std::unique_ptr<ParallelPortPrinter>, 3> printers;
    std::mutex access_lock;
};

// ===== Initialization =====
// File: src/dosbox.cpp
// Add PRINTER_AddConfigSection(control)

// ===== Speaker Integration =====
// File: src/hardware/audio/speaker.cpp
// Optional: add PRINTER_Init/Destroy calls if using [speaker] section

// ===== Configuration =====
// File: src/hardware/printer/printer.cpp (NEW)
void PRINTER_AddConfigSection(Section* sec);
void PRINTER_Init(SectionProp& section);
void PRINTER_Destroy();
void PRINTER_NotifySettingUpdated(SectionProp& section, const std::string& prop_name);
```

---

## Part 3: Recommended Implementation Order

### Phase 1: Non-Conflicting Foundation
1. ✅ Create `ParallelPortPrinter` class (basic byte capture)
2. ✅ Add WriteCharacter(), InitializePrinter(), GetStatus() methods
3. ✅ Create PrinterManager for instance tracking
4. Implement thread-safe accessor function

### Phase 2: Configuration & Lifecycle
5. Create PRINTER configuration section
6. Add PRINTER_Init/Destroy functions
7. Register configuration section in DOSBOX_Config()
8. Add initialization calls to SPEAKER_Init or appropriate location

### Phase 3: Integration with Existing Systems
9. Modify device_LPT1 to add Write() override
10. Enhance INT17 handler to route to printer
11. Add timeout handling

### Phase 4: Testing & Refinement
12. Test INT17 calls
13. Test DOS device writes
14. Test multiple LPT ports
15. Verify no conflicts with LPT DAC

---

## Part 4: Conflict Resolution Strategy

### Recommended Approach: Port-Based Separation

```
Configuration:
[speaker]
lpt_dac=disney      ; Uses LPT1 (0x378)

[printer]
enabled=true
port=lpt2           ; Uses LPT2 (0x278)
output=printer.txt
```

**Advantages**:
- Zero handler conflicts (different ports)
- Easy configuration
- Supports multiple LPT devices
- BIOS detection works naturally

**Disadvantages**:
- Games expecting LPT1 printer need config change
- Requires user education

### Alternative: Multiplexing Handler

```cpp
// Single handler that dispatches based on device priority
class LptPortMultiplexer {
    LptDac* audio_device;
    ParallelPortPrinter* printer_device;
    
    void WriteData(const io_port_t port, const io_val_t value, const io_width_t width) {
        if (audio_device) audio_device->WriteData(port, value, width);
        if (printer_device) printer_device->WriteData(port, value, width);
    }
};
```

**Disadvantages**: 
- Complex state management
- Hard to debug
- Not recommended

---

## Part 5: Critical Dependencies

### Initialization Order (CRITICAL)

```cpp
1. IO_Init()                    // Port system initialized
2. SPEAKER_Init()               // LPT DAC may bind to port
   └─ LPTDAC_Init()             // If enabled
3. PRINTER_Init()               // Printer binds to different port (ideally)
4. BIOS_Init()                  // Port detection runs
   ├─ Reads from ports
   └─ Updates BIOS data area with port addresses
5. DOS_Init()                   // DOS devices created (including device_LPT1)
```

**Must Ensure**: Printer initialized BEFORE or AFTER LPT DAC completes, on different port.

### Configuration Timing

```
Configuration File Load
    ↓
[speaker] section parsed → LPTDAC_NotifySettingUpdated()
[printer] section parsed → PRINTER_NotifySettingUpdated()
    ↓
Initialization phase begins
```

---

## Part 6: Testing Checklist

- [ ] Printer initialized without conflicts with LPT DAC
- [ ] INT17 AH=00 (write char) works
- [ ] INT17 AH=01 (init) works  
- [ ] INT17 AH=02 (get status) works
- [ ] DOS `TYPE > LPT1:` command works
- [ ] DOS `PRINT` command works (if implemented)
- [ ] Multiple LPT ports work independently
- [ ] Timeout values respected
- [ ] Output file correctly written
- [ ] No data corruption or dropped bytes
- [ ] Thread safety under concurrent access
- [ ] Configuration reload works

---

## Summary Table: Friction Points vs. Complexity

| Friction Point | Location | Complexity | Priority | Solution |
|---|---|---|---|---|
| INT17 stub handler | bios.cpp:487 | HIGH | Critical | Route to printer manager |
| No Write() in device_LPT1 | dos_devices.cpp:296 | MEDIUM | Critical | Add override |
| Port detection conflict | bios.cpp:1437 | HIGH | High | Use different ports |
| No config section | dosbox.cpp | MEDIUM | High | Add PRINTER_AddConfigSection |
| Singleton vs. multiple | lpt_dac.cpp | MEDIUM | Medium | Implement PrinterManager |
| Thread safety | lpt_dac.cpp | MEDIUM | Medium | Add mutex protection |
| Port handler conflict | port.h | LOW | Medium | Configuration-based assignment |
| Timeout simulation | bios.h:63 | MEDIUM | Low | Integrate with PIC timing |

---

## Conclusion

The printer implementation is compatible with DOSBox's existing architecture but requires:

1. **Integration points** in INT17 handler and device_LPT1
2. **Configuration system** for printer settings
3. **Port-based separation** from LPT DAC to avoid handler conflicts
4. **Manager pattern** for multiple printer instances
5. **Thread safety** with mutex protection
6. **Careful initialization ordering** to prevent conflicts

Following the recommended implementation order and port separation strategy will minimize friction while maintaining compatibility with existing systems.

