// SPDX-FileCopyrightText:  2026 Antigravity
// SPDX-License-Identifier: GPL-2.0-or-later

#include "append.h"

#include <string>

#include "dos/dos.h"
#include "dos/dos_append.h"
#include "misc/messages.h"

void APPEND::Run()
{


	if (HelpRequested()) {

		WriteOut(MSG_Get("PROGRAM_APPEND_HELP_LONG"));
		return;
	}

	// Strip known MS-DOS APPEND switches silently
	cmd->FindExistRemoveAll("/X");
	cmd->FindExistRemoveAll("/X:ON");
	cmd->FindExistRemoveAll("/X:OFF");
	cmd->FindExistRemoveAll("/E");
	cmd->FindExistRemoveAll("/PATH:ON");
	cmd->FindExistRemoveAll("/PATH:OFF");

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

	// Parse, validate, and convert to absolute paths
	std::string cleaned_paths = "";
	size_t start              = 0;
	size_t end                = args.find(';');
	while (start != std::string::npos) {
		std::string token = args.substr(start,
		                                end == std::string::npos
		                                        ? std::string::npos
		                                        : end - start);

		// Trim leading spaces and quotes
		size_t first = token.find_first_not_of(" \"");
		if (first != std::string::npos) {
			size_t last = token.find_last_not_of(" \"");
			token       = token.substr(first, (last - first + 1));

			if (!token.empty()) {
				char fullname[DOS_PATHLENGTH];
				uint8_t drive;
				if (!DOS_MakeName(token.c_str(), fullname, &drive) ||
				    !Drives[drive] ||
				    !Drives[drive]->TestDir(fullname)) {
					WriteOut(MSG_Get("PROGRAM_APPEND_INVALID_PATH"));
					return;
				}

				std::string final_path = std::string(1, 'A' + drive) + ":\\";
				if (fullname[0] == '\\') {
					final_path += (fullname + 1);
				} else {
					final_path += fullname;
				}

				if (!cleaned_paths.empty()) {
					cleaned_paths += ";";
				}
				cleaned_paths += final_path;
			}
		}

		if (end == std::string::npos) {
			break;
		}
		start = end + 1;
		end   = args.find(';', start);
	}

	// set the new directory list

	dos_append::SetDirList(cleaned_paths);
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
