// SPDX-FileCopyrightText:  2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#include "append.h"

#include <string>

#include "dos/dos_append.h"
#include "misc/messages.h"

void APPEND::Run()
{
	if (HelpRequested()) {
		WriteOut(MSG_Get("PROGRAM_APPEND_HELP_LONG"));
		return;
	}

	std::string args = {};
	cmd->GetStringRemain(args);

	// trim leading whitespace
	while (!args.empty() && args.front() == ' ') {
		args.erase(args.begin());
	}

	// "APPEND ;" clears the list
	if (args == ";") {
		dos_append::SetDirList("");
		return;
	}

	// no args means display current state
	if (args.empty()) {
		auto list = dos_append::GetDirList();
		if (list.empty()) {
			WriteOut(MSG_Get("PROGRAM_APPEND_NO_DIRS"));
			WriteOut("\n");
		} else {
			WriteOut("APPEND=%s\n", list.c_str());
		}
		return;
	}

	// set the new directory list
	dos_append::SetDirList(args);
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
}
