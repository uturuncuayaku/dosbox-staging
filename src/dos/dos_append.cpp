// SPDX-FileCopyrightText:  2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "dos_append.h"

#include <string>

#include "dos.h"
#include "cpu/registers.h"
#include "dos/dos_system.h"

namespace dos_append {

static std::string dir_list     = {};
static bool enabled             = false;
static bool currently_resolving = false;

void Init()
{
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

void SetDirList(const std::string& new_list)
{
	dir_list = new_list;
	enabled  = !dir_list.empty();
}

std::string GetDirList()
{
	return dir_list;
}

// pull just the filename off the end of a path
static std::string ExtractBasename(const std::string& path)
{
	auto pos = path.find_last_of("\\/");
	if (pos == std::string::npos) {
		return path;
	}
	return path.substr(pos + 1);
}

// strip trailing slash so dir + "\\" + name concatenates cleanly
static std::string TrimTrailingSeparator(const std::string& dir)
{
	if (dir.empty()) {
		return dir;
	}
	if (dir.back() == '\\' || dir.back() == '/') {
		return dir.substr(0, dir.size() - 1);
	}
	return dir;
}

bool ResolveName(const char* name, std::string& out_path)
{
	if (!enabled || currently_resolving) {
		return false;
	}

	// Bypass APPEND if the path contains a drive letter or directory separators
	if (strchr(name, ':') != nullptr || strchr(name, '\\') != nullptr || strchr(name, '/') != nullptr) {
		return false;
	}

	auto basename = ExtractBasename(name);
	if (basename.empty()) {
		return false;
	}

	currently_resolving = true;

	// walk the semicolon-delimited list in order
	std::string remaining = dir_list;
	while (!remaining.empty()) {
		std::string dir = {};
		auto sep = remaining.find(';');
		if (sep != std::string::npos) {
			dir       = remaining.substr(0, sep);
			remaining = remaining.substr(sep + 1);
		} else {
			dir       = remaining;
			remaining = {};
		}

		if (dir.empty()) {
			continue;
		}

		dir = TrimTrailingSeparator(dir);
		auto candidate = dir + "\\" + basename;

		if (DOS_FileExists(candidate.c_str())) {
			out_path            = candidate;
			currently_resolving = false;
			return true;
		}
	}

	currently_resolving = false;
	return false;
}

// INT 2Fh AH=B7h handler
bool MultiplexHandler()
{
	if (reg_ah != 0xB7) {
		return false;
	}

	switch (reg_al) {
	case 0x00:
		// installation check: FFh = installed
		reg_al = 0xFF;
		return true;
	default:
		// IMPLEMENTATION BOUNDARY B702: everything else is stubbed
		return false;
	}
}

} // namespace dos_append
