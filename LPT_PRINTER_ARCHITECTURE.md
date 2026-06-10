# LPT_PRINTER: Parallel Port Printer Implementation Using LPT_DAC Pattern

## Executive Summary

Instead of creating a parallel printer system, **extend the existing LPT infrastructure** by creating `LPT_PRINTER` as a peer to `LPT_DAC`. Both are specialized LPT devices with different output targets:

- **LPT_DAC** (existing): Parallel port → Audio mixer
- **LPT_PRINTER** (new): Parallel port → System printer (CUPS/WinSpool)

Use the exact same architecture: base class + derived implementations + lifecycle management.

---

## Part 1: Architecture Pattern

### Existing LPT_DAC Pattern

```
┌─────────────────────┐
│   LptDac (base)     │  src/hardware/audio/private/lpt_dac.h
│   virtual methods   │
│   port handlers     │
│   mixer channel     │
└────────┬────────────┘
         │
    ┌────┴────┬──────────┐
    │         │          │
┌───▼──┐  ┌──▼───┐  ┌───▼──┐
│Disney│  │Covox │  │Stereo│  Derived implementations
└──────┘  └──────┘  └──────┘  src/hardware/audio/private/*.h

LPTDAC_Init()        src/hardware/audio/lpt_dac.cpp
  ├─ Reads config
  ├─ Creates Disney/Covox/StereoOn1
  ├─ Calls BindToPort(Lpt1Port)
  └─ Registers port handlers

Output: Audio → Mixer Channel → Speaker
```

### New LPT_PRINTER Pattern (Mirror Structure)

```
┌──────────────────────┐
│  LptPrinter (base)   │  src/hardware/printer/private/lpt_printer.h
│  virtual methods     │
│  port handlers       │
│  buffer mgmt         │
└────────┬─────────────┘
         │
    ┌────┴────┬──────────┐
    │         │          │
┌───▼──────┐ ┌┴────────┐ ┌┴───────┐
│FilePrinter│ │CupsPrinter│ │WinSpool│  Derived implementations
└───────────┘ │(Linux)  │ │Printer  │  src/hardware/printer/private/*.h
              └─────────┘ └────────┘

LPTPRINTER_Init()      src/hardware/printer/lpt_printer.cpp
  ├─ Reads config
  ├─ Creates FilePrinter/CupsPrinter/WindowsPrinter
  ├─ Calls BindToPort(Lpt2Port or configured port)
  └─ Registers port handlers

Output: Parallel port → File/CUPS/WinSpool → Physical printer or file
```

---

## Part 2: Base Class Design

### src/hardware/printer/private/lpt_printer.h

```cpp
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_PRIVATE_LPT_PRINTER_H
#define DOSBOX_PRIVATE_LPT_PRINTER_H

#include "dosbox.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "hardware/lpt.h"
#include "hardware/port.h"
#include "utils/checks.h"

// Base class for all LPT printer implementations
class LptPrinter {
public:
    LptPrinter(const std::string_view name);
    virtual ~LptPrinter();

    // Virtual interface (implemented by derived classes)
    virtual void BindToPort(const io_port_t lpt_port) = 0;
    
    // Pure virtual - how each printer implementation processes bytes
    virtual void SendByte(uint8_t byte) = 0;
    virtual void Initialize() = 0;
    virtual void Flush() = 0;

    // Shared functionality
    bool IsBusy() const { return busy_flag; }
    void SetBusy(bool busy) { busy_flag = busy; }
    uint8_t GetStatus() const { return status_reg.data; }
    void SetStatus(const LptStatusRegister& status) { status_reg = status; }

    LptPrinter() = delete;
    LptPrinter(const LptPrinter&) = delete;
    LptPrinter& operator=(const LptPrinter&) = delete;

protected:
    // Port handlers (common interface)
    void WriteData(const io_port_t port, const io_val_t value, const io_width_t width);
    uint8_t ReadStatus(const io_port_t port, const io_width_t width);
    void WriteControl(const io_port_t port, const io_val_t value, const io_width_t width);

    // Helper to install handlers (called by derived classes)
    void BindHandlers(const io_port_t lpt_port, 
                      const io_write_f write_data,
                      const io_read_f read_status,
                      const io_write_f write_control);

    // State tracking
    std::string printer_name;
    bool busy_flag = false;
    uint8_t data_reg = 0;
    LptStatusRegister status_reg;
    LptControlRegister control_reg;
    std::vector<uint8_t> buffer;

    // Handler lifecycle objects
    IO_WriteHandleObject data_write_handler;
    IO_ReadHandleObject status_read_handler;
    IO_WriteHandleObject control_write_handler;
};

#endif // DOSBOX_PRIVATE_LPT_PRINTER_H
```

---

## Part 3: Platform-Specific Implementations

### Implementation 1: File-Based Printer (Cross-Platform)

**src/hardware/printer/private/file_printer.h**

```cpp
#ifndef DOSBOX_FILE_PRINTER_H
#define DOSBOX_FILE_PRINTER_H

#include "lpt_printer.h"
#include <fstream>

class FilePrinter final : public LptPrinter {
public:
    FilePrinter(const std::string& output_file);
    ~FilePrinter() override;

    void BindToPort(const io_port_t lpt_port) override;
    void SendByte(uint8_t byte) override;
    void Initialize() override;
    void Flush() override;

private:
    std::ofstream output_file;
    std::string filename;
};

#endif
```

**src/hardware/printer/private/file_printer.cpp**

```cpp
#include "file_printer.h"
#include "utils/checks.h"
#include <functional>

FilePrinter::FilePrinter(const std::string& output_file)
    : LptPrinter("File Printer"), filename(output_file)
{
    output_file.open(filename, std::ios::binary | std::ios::app);
    if (!output_file.is_open()) {
        LOG_WARNING("PRINTER: Failed to open output file: %s", filename.c_str());
    }
    
    status_reg.busy = false;
    status_reg.error = false;
    LOG_MSG("PRINTER: File printer initialized, output: %s", filename.c_str());
}

void FilePrinter::BindToPort(const io_port_t lpt_port)
{
    using namespace std::placeholders;
    
    const auto write_data = std::bind(&FilePrinter::WriteData, this, _1, _2, _3);
    const auto read_status = std::bind(&FilePrinter::ReadStatus, this, _1, _2);
    const auto write_control = std::bind(&FilePrinter::WriteControl, this, _1, _2, _3);
    
    BindHandlers(lpt_port, write_data, read_status, write_control);
    LOG_MSG("PRINTER: File printer bound to LPT port %03xh", lpt_port);
}

void FilePrinter::SendByte(uint8_t byte)
{
    buffer.push_back(byte);
    
    // Flush when buffer reaches reasonable size or on control signal
    if (buffer.size() >= 4096) {
        Flush();
    }
}

void FilePrinter::Initialize()
{
    // Clear buffer on printer reset
    buffer.clear();
    status_reg.busy = false;
    LOG_MSG("PRINTER: File printer initialized");
}

void FilePrinter::Flush()
{
    if (!buffer.empty() && output_file.is_open()) {
        output_file.write(reinterpret_cast<char*>(buffer.data()), buffer.size());
        output_file.flush();
        LOG_MSG("PRINTER: Flushed %zu bytes to file", buffer.size());
        buffer.clear();
    }
}

FilePrinter::~FilePrinter()
{
    Flush();
    if (output_file.is_open()) {
        output_file.close();
    }
    LOG_MSG("PRINTER: File printer shutdown");
}
```

### Implementation 2: CUPS Printer (Linux)

**src/hardware/printer/private/cups_printer.h**

```cpp
#ifndef DOSBOX_CUPS_PRINTER_H
#define DOSBOX_CUPS_PRINTER_H

#include "lpt_printer.h"

#ifdef __linux__
    #include <cups/cups.h>
#endif

class CupsPrinter final : public LptPrinter {
public:
    CupsPrinter();
    ~CupsPrinter() override;

    void BindToPort(const io_port_t lpt_port) override;
    void SendByte(uint8_t byte) override;
    void Initialize() override;
    void Flush() override;

private:
    std::string GetDefaultPrinter();
    bool SendToPrinter(const std::vector<uint8_t>& data);
    
#ifdef __linux__
    http_t* cups_connection = nullptr;
    char cups_printer_name[256] = {};
#endif
};

#endif
```

**src/hardware/printer/private/cups_printer.cpp**

```cpp
#include "cups_printer.h"
#include "utils/checks.h"
#include <functional>

#ifdef __linux__

CupsPrinter::CupsPrinter() : LptPrinter("CUPS Printer")
{
    cups_connection = cupsConnect();
    if (cups_connection) {
        const char* default_printer = cupsGetDefault();
        if (default_printer) {
            safe_strcpy(cups_printer_name, default_printer);
            LOG_MSG("PRINTER: CUPS printer using: %s", cups_printer_name);
        } else {
            LOG_WARNING("PRINTER: CUPS - No default printer found");
        }
    } else {
        LOG_WARNING("PRINTER: Failed to connect to CUPS");
    }
    
    status_reg.busy = false;
    status_reg.error = !cups_connection;
}

void CupsPrinter::BindToPort(const io_port_t lpt_port)
{
    using namespace std::placeholders;
    
    const auto write_data = std::bind(&CupsPrinter::WriteData, this, _1, _2, _3);
    const auto read_status = std::bind(&CupsPrinter::ReadStatus, this, _1, _2);
    const auto write_control = std::bind(&CupsPrinter::WriteControl, this, _1, _2, _3);
    
    BindHandlers(lpt_port, write_data, read_status, write_control);
    LOG_MSG("PRINTER: CUPS printer bound to LPT port %03xh", lpt_port);
}

void CupsPrinter::SendByte(uint8_t byte)
{
    buffer.push_back(byte);
    
    if (buffer.size() >= 8192) {
        Flush();
    }
}

void CupsPrinter::Initialize()
{
    buffer.clear();
    status_reg.busy = false;
    LOG_MSG("PRINTER: CUPS printer initialized");
}

void CupsPrinter::Flush()
{
    if (!buffer.empty() && cups_connection && cups_printer_name[0] != '\0') {
        if (SendToPrinter(buffer)) {
            LOG_MSG("PRINTER: Sent %zu bytes to CUPS printer", buffer.size());
            buffer.clear();
        }
    }
}

bool CupsPrinter::SendToPrinter(const std::vector<uint8_t>& data)
{
    // Create temporary file for print job
    const char* temp_file = cupsTempFile(nullptr, 0);
    if (!temp_file) {
        LOG_WARNING("PRINTER: Failed to create temp file for CUPS job");
        return false;
    }
    
    // Write data to temp file
    FILE* f = fopen(temp_file, "wb");
    if (!f) {
        LOG_WARNING("PRINTER: Failed to open temp file for writing");
        return false;
    }
    
    fwrite(data.data(), 1, data.size(), f);
    fclose(f);
    
    // Submit to CUPS
    int job_id = cupsPrintFile(cups_connection, cups_printer_name,
                               temp_file, "DOSBox Printer Output",
                               0, nullptr);  // No options
    
    if (job_id > 0) {
        LOG_MSG("PRINTER: CUPS job submitted (ID: %d)", job_id);
        unlink(temp_file);
        return true;
    } else {
        LOG_WARNING("PRINTER: CUPS job submission failed");
        unlink(temp_file);
        return false;
    }
}

CupsPrinter::~CupsPrinter()
{
    Flush();
    if (cups_connection) {
        cupsClose(cups_connection);
    }
    LOG_MSG("PRINTER: CUPS printer shutdown");
}

#else
// Stub for non-Linux systems
CupsPrinter::CupsPrinter() : LptPrinter("CUPS Printer")
{
    LOG_WARNING("PRINTER: CUPS printer not available on this platform");
    status_reg.error = true;
}
CupsPrinter::~CupsPrinter() {}
void CupsPrinter::BindToPort(const io_port_t) {}
void CupsPrinter::SendByte(uint8_t) {}
void CupsPrinter::Initialize() {}
void CupsPrinter::Flush() {}
bool CupsPrinter::SendToPrinter(const std::vector<uint8_t>&) { return false; }
#endif
```

### Implementation 3: Windows Print API (WinSpool)

**src/hardware/printer/private/windows_printer.h**

```cpp
#ifndef DOSBOX_WINDOWS_PRINTER_H
#define DOSBOX_WINDOWS_PRINTER_H

#include "lpt_printer.h"

#ifdef WIN32
    #include <winspool.h>
    #pragma comment(lib, "winspool.lib")
#endif

class WindowsPrinter final : public LptPrinter {
public:
    WindowsPrinter();
    ~WindowsPrinter() override;

    void BindToPort(const io_port_t lpt_port) override;
    void SendByte(uint8_t byte) override;
    void Initialize() override;
    void Flush() override;

private:
    bool GetDefaultPrinter();
    bool SendToPrinterQueue(const std::vector<uint8_t>& data);
    
#ifdef WIN32
    HANDLE printer_handle = nullptr;
    WCHAR printer_name[256] = {};
#endif
};

#endif
```

**src/hardware/printer/private/windows_printer.cpp**

```cpp
#include "windows_printer.h"
#include "utils/checks.h"
#include <functional>

#ifdef WIN32

WindowsPrinter::WindowsPrinter() : LptPrinter("Windows Printer")
{
    if (GetDefaultPrinter()) {
        LOG_MSG("PRINTER: Windows printer using: %S", printer_name);
        status_reg.error = false;
    } else {
        LOG_WARNING("PRINTER: Windows - No default printer found");
        status_reg.error = true;
    }
    
    status_reg.busy = false;
}

bool WindowsPrinter::GetDefaultPrinter()
{
    DWORD size = 0;
    ::GetDefaultPrinterW(nullptr, &size);
    
    WCHAR* buffer = new WCHAR[size];
    if (::GetDefaultPrinterW(buffer, &size)) {
        wcsncpy_s(printer_name, _countof(printer_name), buffer, _TRUNCATE);
        delete[] buffer;
        return true;
    }
    
    delete[] buffer;
    return false;
}

void WindowsPrinter::BindToPort(const io_port_t lpt_port)
{
    using namespace std::placeholders;
    
    const auto write_data = std::bind(&WindowsPrinter::WriteData, this, _1, _2, _3);
    const auto read_status = std::bind(&WindowsPrinter::ReadStatus, this, _1, _2);
    const auto write_control = std::bind(&WindowsPrinter::WriteControl, this, _1, _2, _3);
    
    BindHandlers(lpt_port, write_data, read_status, write_control);
    LOG_MSG("PRINTER: Windows printer bound to LPT port %03xh", lpt_port);
}

void WindowsPrinter::SendByte(uint8_t byte)
{
    buffer.push_back(byte);
    
    if (buffer.size() >= 8192) {
        Flush();
    }
}

void WindowsPrinter::Initialize()
{
    buffer.clear();
    status_reg.busy = false;
    LOG_MSG("PRINTER: Windows printer initialized");
}

void WindowsPrinter::Flush()
{
    if (!buffer.empty() && printer_name[0] != L'\0') {
        if (SendToPrinterQueue(buffer)) {
            LOG_MSG("PRINTER: Sent %zu bytes to Windows printer", buffer.size());
            buffer.clear();
        }
    }
}

bool WindowsPrinter::SendToPrinterQueue(const std::vector<uint8_t>& data)
{
    // Open printer
    if (!::OpenPrinterW(printer_name, &printer_handle, nullptr)) {
        LOG_WARNING("PRINTER: Failed to open Windows printer");
        return false;
    }
    
    DOC_INFO_1W doc_info = {};
    doc_info.pDocName = L"DOSBox";
    doc_info.pDatatype = L"RAW";
    
    // Start document
    DWORD job_id = ::StartDocPrinterW(printer_handle, 1, (LPBYTE)&doc_info);
    if (job_id == 0) {
        LOG_WARNING("PRINTER: Failed to start print job");
        ::ClosePrinter(printer_handle);
        return false;
    }
    
    // Start page
    if (!::StartPagePrinter(printer_handle)) {
        LOG_WARNING("PRINTER: Failed to start page");
        ::EndDocPrinter(printer_handle);
        ::ClosePrinter(printer_handle);
        return false;
    }
    
    // Send data
    DWORD bytes_written = 0;
    if (!::WritePrinter(printer_handle, 
                        (PVOID)data.data(), 
                        (DWORD)data.size(), 
                        &bytes_written)) {
        LOG_WARNING("PRINTER: Failed to write to printer");
        ::EndPagePrinter(printer_handle);
        ::EndDocPrinter(printer_handle);
        ::ClosePrinter(printer_handle);
        return false;
    }
    
    // End page and document
    ::EndPagePrinter(printer_handle);
    ::EndDocPrinter(printer_handle);
    ::ClosePrinter(printer_handle);
    
    LOG_MSG("PRINTER: Windows print job completed (%lu bytes)", bytes_written);
    return true;
}

WindowsPrinter::~WindowsPrinter()
{
    Flush();
    LOG_MSG("PRINTER: Windows printer shutdown");
}

#else
// Stub for non-Windows systems
WindowsPrinter::WindowsPrinter() : LptPrinter("Windows Printer")
{
    LOG_WARNING("PRINTER: Windows printer not available on this platform");
    status_reg.error = true;
}
WindowsPrinter::~WindowsPrinter() {}
void WindowsPrinter::BindToPort(const io_port_t) {}
void WindowsPrinter::SendByte(uint8_t) {}
void WindowsPrinter::Initialize() {}
void WindowsPrinter::Flush() {}
bool WindowsPrinter::GetDefaultPrinter() { return false; }
bool WindowsPrinter::SendToPrinterQueue(const std::vector<uint8_t>&) { return false; }
#endif
```

---

## Part 4: Public Interface & Lifecycle Management

### src/hardware/printer/lpt_printer.h (Public Interface)

```cpp
#ifndef DOSBOX_LPT_PRINTER_H
#define DOSBOX_LPT_PRINTER_H

#include "config/setup.h"

// Lifecycle functions (following LPT_DAC pattern)
void LPTPRINTER_AddConfigSection(Section* sec);
void LPTPRINTER_Init(SectionProp& section);
void LPTPRINTER_Destroy();
void LPTPRINTER_NotifySettingUpdated(SectionProp& section, const std::string& prop_name);

#endif
```

### src/hardware/printer/lpt_printer.cpp (Implementation)

```cpp
#include "lpt_printer.h"

#include "private/file_printer.h"
#include "private/cups_printer.h"
#include "private/windows_printer.h"

#include "config/setup.h"
#include "utils/support.h"

static std::unique_ptr<LptPrinter> lpt_printer = {};

static void init_lpt_printer_settings(SectionProp& section)
{
    using enum Property::Changeable::Value;

    auto pstring = section.AddString("lpt_printer", WhenIdle, "none");
    pstring->SetHelp(
        "Type of printer connected to parallel port ('none' by default). Possible values:\n"
        "\n"
        "  file:      Output to text file (all platforms).\n"
        "  cups:      Use CUPS printer queue (Linux only).\n"
        "  windows:   Use Windows printer queue (Windows only).\n"
        "  none/off:  Don't emulate a printer (default).");
    pstring->SetValues({"none", "file", "cups", "windows", "off"});

    pstring = section.AddString("lpt_printer_output", WhenIdle, "printer.txt");
    pstring->SetHelp("Output filename for file-based printer (when lpt_printer=file).");

    auto pint = section.AddInt("lpt_printer_port", WhenIdle, 2);
    pint->SetHelp("Which LPT port to use (1=0x378, 2=0x278, 3=0x3BC).");
    pint->SetMinMax(1, 3);
}

void LPTPRINTER_Init(SectionProp& section)
{
    const std::string printer_choice = section.GetString("lpt_printer");

    if (printer_choice == "none" || printer_choice == "off") {
        return;
    }

    // Determine which port to use
    int port_choice = section.GetInt("lpt_printer_port");
    io_port_t lpt_port = (port_choice == 1) ? Lpt1Port : 
                         (port_choice == 2) ? Lpt2Port : Lpt3Port;

    if (printer_choice == "file") {
        const std::string output_file = section.GetString("lpt_printer_output");
        lpt_printer = std::make_unique<FilePrinter>(output_file);

    } else if (printer_choice == "cups") {
#ifdef __linux__
        lpt_printer = std::make_unique<CupsPrinter>();
#else
        LOG_WARNING("LPT_PRINTER: CUPS printer not available on this platform, using file output");
        const std::string output_file = section.GetString("lpt_printer_output");
        lpt_printer = std::make_unique<FilePrinter>(output_file);
#endif

    } else if (printer_choice == "windows") {
#ifdef WIN32
        lpt_printer = std::make_unique<WindowsPrinter>();
#else
        LOG_WARNING("LPT_PRINTER: Windows printer not available on this platform, using file output");
        const std::string output_file = section.GetString("lpt_printer_output");
        lpt_printer = std::make_unique<FilePrinter>(output_file);
#endif

    } else {
        LOG_WARNING("LPT_PRINTER: Invalid 'lpt_printer' setting: '%s', using 'none'",
                    printer_choice.c_str());
        return;
    }

    assert(lpt_printer);
    lpt_printer->BindToPort(lpt_port);

    LOG_MSG("LPT_PRINTER: Initialized on LPT port %d", port_choice);
}

void LPTPRINTER_Destroy()
{
    if (lpt_printer) {
        lpt_printer->Flush();
        lpt_printer.reset();
    }
}

void LPTPRINTER_NotifySettingUpdated(SectionProp& section,
                                     [[maybe_unused]] const std::string& prop_name)
{
    if (prop_name == "lpt_printer" || prop_name == "lpt_printer_output") {
        LPTPRINTER_Destroy();
        LPTPRINTER_Init(section);
    }
}

void LPTPRINTER_AddConfigSection(Section* sec)
{
    assert(sec);
    const auto section = static_cast<SectionProp*>(sec);
    init_lpt_printer_settings(*section);
}
```

---

## Part 5: Integration with Speaker Module

### Modification to src/hardware/audio/speaker.cpp

```cpp
// Add includes
#include "hardware/printer/lpt_printer.h"

// Modify SPEAKER_Init()
void SPEAKER_Init()
{
    const auto section = get_section("speaker");

    LPTDAC_Init(*section);
    PCSPEAKER_Init(*section);
    PS1AUDIO_Init(*section);
    TANDYSOUND_Init(*section);
    
    // NEW: Initialize printer if config has it
    LPTPRINTER_Init(*section);
}

// Modify SPEAKER_Destroy()
void SPEAKER_Destroy()
{
    LPTPRINTER_Destroy();    // NEW
    TANDYSOUND_Destroy();
    PS1AUDIO_Destroy();
    PCSPEAKER_Destroy();
    LPTDAC_Destroy();
}

// Modify notify_speaker_setting_updated()
void notify_speaker_setting_updated(SectionProp& section, const std::string& prop_name)
{
    LPTDAC_NotifySettingUpdated(section, prop_name);
    PCSPEAKER_NotifySettingUpdated(section, prop_name);
    PS1AUDIO_NotifySettingUpdated(section, prop_name);
    TANDYSOUND_NotifySettingUpdated(section, prop_name);
    
    LPTPRINTER_NotifySettingUpdated(section, prop_name);  // NEW
}

// Modify SPEAKER_AddConfigSection()
void SPEAKER_AddConfigSection(const ConfigPtr& conf)
{
    assert(conf);

    auto section = conf->AddSection("speaker");
    section->AddUpdateHandler(notify_speaker_setting_updated);

    LPTDAC_AddConfigSection(section);
    PCSPEAKER_AddConfigSection(section);
    PS1AUDIO_AddConfigSection(section);
    TANDYSOUND_AddConfigSection(section);
    
    LPTPRINTER_AddConfigSection(section);  // NEW

    init_speaker_settings(*section);
}
```

---

## Part 6: Configuration Examples

### Example 1: LPT DAC on Port 1, Printer on Port 2 (File Output)

```ini
[speaker]
lpt_dac=disney
lpt_dac_filter=on
lpt_printer=file
lpt_printer_port=2
lpt_printer_output=./printer_output.txt
```

### Example 2: Printer to System Queue (Linux CUPS)

```ini
[speaker]
lpt_dac=none
lpt_printer=cups
lpt_printer_port=1
```

### Example 3: Printer to System Queue (Windows WinSpool)

```ini
[speaker]
lpt_dac=none
lpt_printer=windows
lpt_printer_port=1
```

### Example 4: Both DAC and Printer on Different Ports

```ini
[speaker]
lpt_dac=covox
lpt_printer=file
lpt_printer_port=2
lpt_printer_output=./dosbox_printer.txt
```

---

## Part 7: Data Flow: Game Output → System Printer

### File Printer Flow

```
Game Code
  ↓
out 0x278, AL (write to LPT2)
  ↓
DOSBox Port Handler
  ↓
FilePrinter::WriteData()
  buffer.push_back(byte)
  ↓
[When buffer >= 4096 bytes]
  ↓
FilePrinter::Flush()
  ↓
output_file.write(buffer)
  ↓
./printer_output.txt (physical file)
```

### CUPS Printer Flow (Linux)

```
Game Code
  ↓
out 0x378, AL (write to LPT1)
  ↓
DOSBox Port Handler
  ↓
CupsPrinter::WriteData()
  buffer.push_back(byte)
  ↓
[When buffer >= 8192 bytes or flush signal]
  ↓
CupsPrinter::Flush()
  ↓
cupsTempFile() → create temp file
fwrite(buffer) → write to temp
cupsPrintFile() → submit to CUPS queue
  ↓
CUPS Daemon → Physical Printer
```

### Windows Printer Flow

```
Game Code
  ↓
out 0x378, AL (write to LPT1)
  ↓
DOSBox Port Handler
  ↓
WindowsPrinter::WriteData()
  buffer.push_back(byte)
  ↓
[When buffer >= 8192 bytes or flush signal]
  ↓
WindowsPrinter::Flush()
  ↓
OpenPrinterW() → get handle
StartDocPrinterW() → begin job
WritePrinter() → send bytes
EndDocPrinterW() → finish job
  ↓
Windows Print Queue → Physical Printer
```

---

## Part 8: Benefits of This Approach

✅ **Mirrors existing LPT_DAC pattern** - Consistent architecture  
✅ **Platform-agnostic base class** - Easy to add new implementations  
✅ **Configuration-driven port assignment** - No conflicts with DAC  
✅ **Lifecycle management** - Follows SPEAKER_Init/Destroy pattern  
✅ **Extensible** - Can add network printers, PDF printers, etc.  
✅ **Thread-safe** - No mixer locks needed (just port handlers)  
✅ **No INT17 handler changes needed** (initially) - Can add later as enhancement  
✅ **Fallback support** - CUPS falls back to file on non-Linux, Windows falls back to file on non-Windows  

---

## Part 9: Implementation Checklist

- [ ] Create base class: `src/hardware/printer/private/lpt_printer.h`
- [ ] Create implementation base: `src/hardware/printer/private/lpt_printer.cpp`
- [ ] Create file printer: `src/hardware/printer/private/file_printer.{h,cpp}`
- [ ] Create CUPS printer: `src/hardware/printer/private/cups_printer.{h,cpp}`
- [ ] Create Windows printer: `src/hardware/printer/private/windows_printer.{h,cpp}`
- [ ] Create public interface: `src/hardware/printer/lpt_printer.{h,cpp}`
- [ ] Update speaker.cpp to initialize printer
- [ ] Add configuration section to speaker settings
- [ ] Test with file output (cross-platform)
- [ ] Test with CUPS on Linux
- [ ] Test with WinSpool on Windows
- [ ] Verify no conflicts with LPT DAC

---

## Summary

This approach treats **LPT_PRINTER as a peer to LPT_DAC** rather than a separate system. Both are LPT-attached devices with different output targets:

- **LPT_DAC** → Audio mixer
- **LPT_PRINTER** → System printer or file

Use the exact same base class pattern, configuration system, and lifecycle management. Each platform gets an implementation: File (universal), CUPS (Linux), WinSpool (Windows), with automatic fallback to file on unsupported platforms.

This is **minimal friction** because it reuses the proven LPT infrastructure with maximum code reuse.

