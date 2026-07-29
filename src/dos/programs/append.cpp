// SPDX-FileCopyrightText:  2026 Antigravity
// SPDX-License-Identifier: GPL-2.0-or-later

#include "append.h"

#include <cassert>
#include <optional>
#include <string>

#include "dos/dos_append.h"
#include "misc/messages.h"

void APPEND::ShowCurrentState()
{
	auto list = dos_append::GetDirectories();
	if (list.empty()) {
		WriteOut(MSG_Get("PROGRAM_APPEND_NO_DIRS"));
		WriteOut("\n");
	} else {
		WriteOut("APPEND=%s\n", list.c_str());
	}
}

void APPEND::CommitDirectoryList(const std::string& validated_paths)
{
	dos_append::SetDirectories(validated_paths);
	assert(dos_append::GetDirectories() == validated_paths);
}

void APPEND::Run()
{
	if (HelpRequested()) {
		WriteOut(MSG_Get("PROGRAM_APPEND_HELP_LONG"));
		return;
	}

	std::string args;
	cmd->GetStringRemain(args);

	// 1. Clear directory list (e.g. APPEND ;)
	if (args == ";") {
		dos_append::SetDirectories("");
		return;
	}

	// 2. No directory arguments passed (show status)
	if (args.empty()) {
		ShowCurrentState();
		return;
	}

	// 3. Directory arguments passed (validate and commit)
	const auto validated_paths = dos_append::ValidateDirectories(args);
	if (!validated_paths.has_value()) {
		WriteOut(MSG_Get("PROGRAM_APPEND_INVALID_PATH"));
		return;
	}

	CommitDirectoryList(*validated_paths);
}

void APPEND::AddMessages()
{
	MSG_Add("PROGRAM_APPEND_HELP_LONG",
	        "Set directories for file searching.\n"
	        "\n"
	        "Usage:\n"
	        "  [color=light-green]append[reset] [color=light-cyan]DIR[reset][[;[color=light-cyan]DIR[reset]]...]\n"
	        "  [color=light-green]append[reset]\n"
	        "  [color=light-green]append[reset] ;\n"
	        "\n"
	        "Parameters:\n"
	        "  [color=light-cyan]DIR[reset]  directory to add to the search list\n"
	        "  ;    clear the directory list\n"
	        "\n"
	        "Notes:\n"
	        "  When a program tries to open a file that isn't found in the current\n"
	        "  directory, the directories in the APPEND list are searched in order.\n"
	        "  Running APPEND with no arguments shows the current list.\n"
	        "\n"
	        "Examples:\n"
	        "  [color=light-green]append[reset] [color=light-cyan]C:\\DATA[reset]            ; search C:\\DATA for files\n"
	        "  [color=light-green]append[reset] [color=light-cyan]C:\\ONE[reset];[color=light-cyan]D:\\TWO[reset]     ; search C:\\ONE then D:\\TWO\n"
	        "  [color=light-green]append[reset]                    ; display current list\n"
	        "  [color=light-green]append[reset] ;                  ; clear the list\n");

	MSG_Add("PROGRAM_APPEND_NO_DIRS", "No APPEND directories.");
	MSG_Add("PROGRAM_APPEND_INVALID_PATH", "Invalid path\n");
}
