// SPDX-FileCopyrightText:  2026 Antigravity
// SPDX-License-Identifier: GPL-2.0-or-later

#include "dos_append.h"

#include <string>
#include <string_view>
#include <ranges>

#include "dos.h"
#include "cpu/registers.h"
#include "dos/dos_system.h"
#include "hardware/memory.h"

namespace dos_append {

static std::string dir_list     = {};
static bool enabled             = false;
static bool currently_resolving = false;

// DOS memory mirror for subfunction 04h (dir_ptr).
// APPEND paths can be up to 128 bytes in MS-DOS (8 directories × ~15 chars).
// We allocate 16 paragraphs (256 bytes) to be safe.
static constexpr uint16_t kDirlistDosPages = 16;
static constexpr size_t kDirlistDosMaxBytes = kDirlistDosPages * 16;
static uint16_t dirlist_dos_segment = 0;

// Pushes the current C++ dir_list into the emulated DOS memory block
// so that programs calling INT 2Fh AX=B704h get a valid far pointer.
static void syncDirListToDos()
{
	if (dirlist_dos_segment == 0) {
		return; // Not yet initialized (called from tests before Init)
	}

	const PhysPt dos_addr = static_cast<PhysPt>(dirlist_dos_segment) << 4;

	// Zero-fill first so stale data never leaks
	for (size_t i = 0; i < kDirlistDosMaxBytes; ++i) {
		mem_writeb(dos_addr + static_cast<PhysPt>(i), 0);
	}

	// Copy the string (clamped to max size, always null-terminated)
	const size_t copy_len = std::min(dir_list.size(), kDirlistDosMaxBytes - 1);
	if (copy_len > 0) {
		MEM_BlockWrite(dos_addr, dir_list.c_str(), copy_len);
	}
	// Null terminator is already in place from the zero-fill
}



void Init()
{
	// Allocate a block in emulated DOS memory for the directory list
	// so subfunction 04h can return a valid far pointer.
	dirlist_dos_segment = DOS_GetMemory(kDirlistDosPages);
	syncDirListToDos();

	DOS_AddMultiplexHandler(MultiplexHandler);
}

bool IsEnabled()
{
	return enabled;
}

bool IsResolving()
{
	return currently_resolving;
}

void SetDirList(const std::string& new_list, const char* /*reason*/)
{
	dir_list = new_list;
	enabled  = !dir_list.empty();
	syncDirListToDos();
}

std::string GetDirList()
{
	return dir_list;
}

static std::string extractBasename(const std::string& path)
{
	auto pos = path.find_last_of("\\/");
	if (pos == std::string::npos) {
		return path;
	}
	return path.substr(pos + 1);
}

static bool shouldBypassAppend(const char* name)
{
	if (strchr(name, ':') != nullptr || strchr(name, '\\') != nullptr || strchr(name, '/') != nullptr) {
		return true;
	}
	return false;
}

static bool checkCandidate(std::string_view dir, const std::string& basename, std::string& out_path)
{
	std::string_view trimmed_dir = dir;
	if (!trimmed_dir.empty() && (trimmed_dir.back() == '\\' || trimmed_dir.back() == '/')) {
		trimmed_dir.remove_suffix(1);
	}

	auto candidate = std::string(trimmed_dir) + "\\" + basename;

	bool exists = DOS_FileExists(candidate.c_str());
	
	if (exists) {
		out_path = candidate;
		return true;
	} else {
		return false;
	}
}

bool ResolveName(const char* name, std::string& out_path)
{
	if (!enabled) {
		return false;
	}

	if (currently_resolving) {
		return false;
	}

	if (shouldBypassAppend(name)) {
		return false;
	}

	auto basename = extractBasename(name);
	if (basename.empty()) {
		return false;
	}

	currently_resolving = true;

	std::string_view sv = dir_list;
	
	for (const auto& subrange : sv | std::views::split(';')) {
		std::string_view dir(subrange.begin(), subrange.end());
		
		if (dir.empty()) {
			continue;
		}

		if (checkCandidate(dir, basename, out_path)) {
			currently_resolving = false;
			return true;
		}
	}

	currently_resolving = false;

	return false;
}

bool MultiplexHandler()
{
	if (reg_ah != 0xB7) {
		return false;
	}

	switch (reg_al) {
	case 0x00:
		reg_al = 0xFF;
		return true;
	case 0x02:
		// MS-DOS 4.0 APPEND.ASM returns AX=FFFFh here.
		// This signals "I am MS-DOS APPEND, not IBM PC Network APPEND."
		reg_ax = 0xFFFF;
		return true;
	case 0x04:
		// Return far pointer (ES:DI) to the directory list in DOS memory.
		SegSet16(es, dirlist_dos_segment);
		reg_di = 0x0000;
		return true;
	case 0x06:
		// Get APPEND state flags in BX.
		// We report Enabled (0x0001) when the dir list is non-empty.
		reg_bx = enabled ? 0x0001 : 0x0000;
		return true;
	case 0x07:
		// Set APPEND state flags from BX.
		// We accept the call silently so legacy apps don't crash,
		// but we only honor the Enabled bit (0x0001) for now.
		// Honor the Enabled bit: if caller clears it, disable APPEND
		if (!(reg_bx & 0x0001) && enabled) {
			enabled = false;
		} else if ((reg_bx & 0x0001) && !dir_list.empty()) {
			enabled = true;
		}
		return true;
	case 0x10:
		// MS-DOS 4.0 APPEND.ASM: detailed version check.
		// Returns AX=mode_flags, BX=0, CX=0, DL=major, DH=minor.
		reg_ax = enabled ? 0x0001 : 0x0000; // mode_flags
		reg_bx = 0x0000;
		reg_cx = 0x0000;
		reg_dl = dos.version.major;
		reg_dh = dos.version.minor;
		return true;
	default:
		return false;
	}
}

} // namespace dos_append
