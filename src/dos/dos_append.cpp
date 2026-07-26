// SPDX-FileCopyrightText:  2026 Antigravity
// SPDX-License-Identifier: GPL-2.0-or-later

#include "dos_append.h"

#include <ranges>
#include <string>
#include <string_view>

#include "cpu/registers.h"
#include "dos.h"
#include "dos/dos_system.h"
#include "hardware/memory.h"
#include "shell/shell.h"

namespace dos_append {

// ANONYMOUS NAMESPACE: Internal variables and helper functions
// dosbox-staging prefers this over 'static' for translation-unit local linkage.
namespace {

std::string directories = {}; // Empty at initialization
bool active_mode        = false; // No paths are set at initialization
bool in_recursion       = false; // Prevent nested calls
bool env_mode           = false; // APPEND is not active in environment variables at initialization
bool path_mode          = true;  // /PATH:ON by default in MS-DOS 4.0+
bool exec_mode          = false; // APPEND is not active in executable search paths at initialization

// RAII Recursion Guard: Automatically sets flag to true on creation and resets
// to false on destruction.
struct [[nodiscard]] RecursionGuard {
	bool& flag;
	explicit RecursionGuard(bool& f) noexcept : flag(f)
	{
		flag = true;
	}
	~RecursionGuard() noexcept
	{
		flag = false;
	}
};

// DOS memory mirror for subfunction 04h (dir_ptr).
// APPEND paths can be up to 128 bytes in MS-DOS (8 directories × ~15 chars).
// We allocate 16 paragraphs (256 bytes) to be safe (0.04% of MS-DOS
// conventional memory).
constexpr uint16_t K_DIR_LIST_DOS_PAGES  = 16;
constexpr size_t K_DIR_LIST_DOS_MAXBYTES = K_DIR_LIST_DOS_PAGES * 16;
uint16_t dirlist_dos_segment             = 0;

// Helper for find_absolute_path(): Extracts the leaf filename from a path (e.g.
// "C:\DIR\FILE.TXT" -> "FILE.TXT").
std::string_view extract_filename(std::string_view path)
{
	auto pos = path.find_last_of("\\/");
	if (pos == std::string_view::npos) {
		return path;
	}
	return path.substr(pos + 1);
}

// Helper for find_absolute_path(): Checks if APPEND directory searching should
// be skipped.
bool skip_search(std::string_view target_path)
{
	if (target_path.find_first_of(":\\/") != std::string_view::npos) {
		return !path_mode;
	}
	return false;
}

// Helper for find_absolute_path(): Tests if combining 'search_path' and
// 'filename' exists on disk, populating 'absolute_path'.
bool find(std::string_view search_path, std::string_view filename,
          std::string& absolute_path)
{
	std::string_view trimmed = search_path;
	if (!trimmed.empty() && (trimmed.back() == '\\' || trimmed.back() == '/')) {
		trimmed.remove_suffix(1);
	}

	auto candidate = std::string(trimmed) + "\\" + std::string(filename);

	if (DOS_FileExists(candidate.c_str())) {
		absolute_path = candidate;
		return true;
	}
	return false;
}

// Pushes the current C++ dir_list into the emulated DOS memory block
// so that programs calling INT 2Fh AX=B704h get a valid far pointer.
void sync_directories()
{
	// Uninitialized segment check: prevents memory write before Init() allocates DOS RAM (e.g. isolated unit tests).
	if (dirlist_dos_segment == 0) {
		return;
	}

	const PhysPt dos_addr = static_cast<PhysPt>(dirlist_dos_segment) << 4;

	// Zero-fill first so stale data never leaks
	for (size_t i = 0; i < K_DIR_LIST_DOS_MAXBYTES; ++i) {
		mem_writeb(dos_addr + static_cast<PhysPt>(i), 0);
	}

	// Copy the string (clamped to max size, always null-terminated)
	const size_t copy_len = std::min(directories.size(),
	                                 K_DIR_LIST_DOS_MAXBYTES - 1);
	if (copy_len > 0) {
		MEM_BlockWrite(dos_addr, directories.c_str(), copy_len);
	}
	// Null terminator is already in place from the zero-fill
}
} // end of anonymous namespace

// Start of API
void Init()
{
	// Allocate a block in emulated DOS memory for the directory list
	// so subfunction 04h can return a valid far pointer.
	dirlist_dos_segment = DOS_GetMemory(K_DIR_LIST_DOS_PAGES);
	sync_directories();

	DOS_AddMultiplexHandler(MultiplexHandler);
}

bool IsEnabled()
{
	return active_mode;
}

bool IsEnvOn()
{
	return env_mode;
}

bool IsPathOverrideOn()
{
	return path_mode;
}

bool IsExecOn()
{
	return exec_mode;
}

void SetFlags(bool env, bool path_on, bool exec)
{
	env_mode  = env;
	path_mode = path_on;
	exec_mode = exec;
}

bool IsResolving()
{
	return in_recursion;
}

// Single Central Validation Authority: Validates semicolon-separated directory lists against mounted DOS drives.
std::optional<std::string> ValidateDirectories(std::string_view args)
{
	// Trim leading whitespace and '=' (e.g. "APPEND = C:\DATA")
	while (!args.empty() && (args.front() == ' ' || args.front() == '=')) {
		args.remove_prefix(1);
	}

	std::string validated_paths = {};
	validated_paths.reserve(DOS_PATHLENGTH);

	size_t start = 0;
	while (start < args.size()) {
		size_t end = args.find(';', start);
		std::string_view token = args.substr(start, (end == std::string_view::npos) ? std::string_view::npos : end - start);

		size_t first = token.find_first_not_of(" \"");
		if (first != std::string_view::npos) {
			size_t last = token.find_last_not_of(" \"");
			token       = token.substr(first, last - first + 1);

			if (!token.empty()) {
				std::string token_str(token);
				char fullname[DOS_PATHLENGTH];
				uint8_t drive;
				if (!DOS_MakeName(token_str.c_str(), fullname, &drive) ||
				    !Drives[drive] || !Drives[drive]->TestDir(fullname)) {
					return std::nullopt;
				}

				std::string final_path = std::string(1, 'A' + drive) + ":\\";
				if (fullname[0] == '\\') {
					final_path += (fullname + 1);
				} else {
					final_path += fullname;
				}

				if (!validated_paths.empty()) {
					validated_paths += ';';
				}
				validated_paths += final_path;
			}
		}

		if (end == std::string_view::npos) break;
		start = end + 1;
	}
	return validated_paths;
}

void SetDirectories(const std::string& new_list)
{
	directories = new_list;             // Store pre-validated, clean paths
	active_mode = !directories.empty(); // Enable resolution if non-empty
	sync_directories();                 // Sync to DOS RAM far pointer
}

std::string GetDirectories()
{
	if (env_mode) {
		if (auto shell = DOS_GetFirstShell()) {
			if (auto envvar = shell->psp->GetEnvironmentValue("APPEND")) {
				return *envvar;
			}
		}
		return "";
	}
	return directories;
}

// Attempts to find 'target_path' within the APPEND directory list, storing the
// result in 'absolute_path'.
bool find_absolute_path(const char* target_path, std::string& absolute_path)
{
	if (!active_mode || in_recursion) {
		return false;
	}

	if (skip_search(target_path)) {
		return false;
	}

	const auto filename = extract_filename(target_path);
	if (filename.empty()) {
		return false;
	}

	const RecursionGuard guard(in_recursion);

	const auto current_list = GetDirectories();

	// Iterate through each semicolon-separated directory in the APPEND list.
	for (const auto& subrange :
	     std::string_view(current_list) | std::views::split(';')) {
		std::string_view search_path(subrange.begin(), subrange.end());

		if (!search_path.empty() &&
		    find(search_path, filename, absolute_path)) {
			return true;
		}
	}

	return false;
}

bool MultiplexHandler()
{
	// Ignore multiplex interrupts not targeted at APPEND (INT 2Fh, AH=B7h).
	if (reg_ah != 0xB7) {
		return false;
	}

	switch (reg_al) {
	// MS-DOS 3.3+ Installation Check: returns AL=0xFF to signal that APPEND
	// is installed in memory.
	case 0x00: reg_al = 0xFF; return true;
	case 0x02:
		// MS-DOS 4.0 APPEND.ASM returns AX=FFFFh here.
		// This signals "I am MS-DOS APPEND, not IBM PC Network APPEND."
		reg_ax = 0xFFFF;
		return true;
	case 0x04:
		// Return far pointer ES:DI (segment:0000) to directory list in
		// DOS memory.
		SegSet16(es, dirlist_dos_segment);
		reg_di = 0x0000;
		return true;
	case 0x06:
		// Get APPEND state flags in BX.
		// We report Enabled (0x0001) when the dir list is non-empty.
		reg_bx = active_mode ? 0x0001 : 0x0000;
		return true;
	case 0x07:
		// Set APPEND state flags from BX.
		// We accept the call silently so legacy apps don't crash,
		// but we only honor the Enabled bit (0x0001) for now.
		// Honor the Enabled bit: if caller clears it, disable APPEND
		if (!(reg_bx & 0x0001) && active_mode) {
			active_mode = false;
		} else if ((reg_bx & 0x0001) && !directories.empty()) {
			active_mode = true;
		}
		return true;
	case 0x10:
		// MS-DOS 4.0 APPEND.ASM: detailed version check.
		// Returns AX=mode_flags, BX=0, CX=0, DL=major, DH=minor.
		reg_ax = active_mode ? 0x0001 : 0x0000; // mode_flags
		reg_bx = 0x0000;
		reg_cx = 0x0000;
		reg_dl = dos.version.major;
		reg_dh = dos.version.minor;
		return true;
	default: return false;
	}
} // end of MultiplexHandler

} // end of namespace dos_append