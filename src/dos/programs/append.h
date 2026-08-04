// SPDX-FileCopyrightText:  2026 Antigravity
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_PROGRAM_APPEND_H
#define DOSBOX_PROGRAM_APPEND_H

#include <optional>

#include "dos/programs.h"

class APPEND final : public Program {
public:
	APPEND()
	{
		AddMessages();
		help_detail = {HELP_Filter::Common,
		               HELP_Category::File,
		               HELP_CmdType::Program,
		               "APPEND"};
	}
	void Run() override;

private:
	void AddMessages();
	void ShowCurrentState();
	void CommitDirectoryList(const std::string& validated_paths);
};

#endif // DOSBOX_PROGRAM_APPEND_H
