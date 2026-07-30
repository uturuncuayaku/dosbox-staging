// SPDX-FileCopyrightText:  2026 Antigravity
// SPDX-License-Identifier: GPL-2.0-or-later

#include "append.h"

#include <cassert>
#include <optional>
#include <string>

#include "dos/dos_append.h"
#include "misc/messages.h"
#include "shell/shell.h"

namespace {

struct AppendOptions {
	std::optional<bool> exec{};

	bool changed() const
	{
		return exec.has_value();
	}
};

// Parse /X, /X:ON/OFF options from the command line
AppendOptions parse_options(CommandLine* cmd)
{
	AppendOptions options;

	const bool x_on   = cmd->FindExistRemoveAll("/X:ON");
	const bool x_bare = cmd->FindExistRemoveAll("/X");
	const bool x_off  = cmd->FindExistRemoveAll("/X:OFF");

	if (x_on || x_bare) {
		options.exec = true;
	} else if (x_off) {
		options.exec = false;
	}

	return options;
}

// Apply any option changes, keeping existing values if no option was given
void apply_option_updates(const AppendOptions& options)
{
	if (!options.changed()) {
		return;
	}
	bool finalExec = options.exec.value_or(dos_append::IsExecOn());
	dos_append::SetFlags(dos_append::IsEnvOn(), dos_append::IsPathOverrideOn(), finalExec);
}

} // namespace

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

	// Parse options from the command line
	const AppendOptions options = parse_options(cmd);

	// Reject unrecognized switches before processing anything
	std::string invalid_switch;
	if (cmd->FindStringBegin("/", invalid_switch)) {
		invalid_switch = "/" + invalid_switch;
		WriteOut(MSG_Get("SHELL_ILLEGAL_SWITCH"), invalid_switch.c_str());
		return;
	}

	std::string args;
	cmd->GetStringRemain(args);

	// 1. Clear directory list (e.g. APPEND ;)
	if (args == ";") {
		apply_option_updates(options);
		dos_append::SetDirectories("");
		return;
	}

	// 2. No directory arguments passed (show status or update flags)
	if (args.empty()) {
		if (options.changed()) {
			apply_option_updates(options);
		} else {
			ShowCurrentState();
		}
		return;
	}

	// 3. Directory arguments passed (validate and commit)
	const auto validated_paths = dos_append::ValidateDirectories(args);
	if (!validated_paths.has_value()) {
		WriteOut(MSG_Get("PROGRAM_APPEND_INVALID_PATH"));
		return;
	}

	apply_option_updates(options);
	CommitDirectoryList(*validated_paths);
}

void APPEND::AddMessages()
{
	MSG_Add("PROGRAM_APPEND_HELP_LONG",
	        "Set directories for file searching.\n"
	        "\n"
	        "Usage:\n"
	        "  [color=light-green]append[reset] [color=light-cyan]DIR[reset][[;[color=light-cyan]DIR[reset]]...]\n"
	        "  [color=light-green]append[reset] [/X[:ON|:OFF]]\n"
	        "  [color=light-green]append[reset] ;\n"
	        "\n"
	        "Parameters:\n"
	        "  [color=light-cyan]DIR[reset]  directory to add to the search list\n"
	        "  /X:ON  enable executable search in APPEND directories\n"
	        "  /X:OFF disable executable search in APPEND directories\n"
	        "  ;    clear the directory list\n"
	        "\n"
	        "Notes:\n"
	        "  When a program tries to open a file that isn't found in the current\n"
	        "  directory, the directories in the APPEND list are searched in order.\n"
	        "  Running APPEND with no arguments shows the current list.\n"
	        "\n"
	        "Examples:\n"
	        "  [color=light-green]append[reset] [color=light-cyan]C:\\DATA[reset]            ; search C:\\DATA for files\n"
	        "  [color=light-green]append[reset] /X:ON              ; enable executable search\n"
	        "  [color=light-green]append[reset] ;                  ; clear the list\n");

	MSG_Add("PROGRAM_APPEND_NO_DIRS", "No APPEND directories.");
	MSG_Add("PROGRAM_APPEND_INVALID_PATH", "Invalid path\n");
}
