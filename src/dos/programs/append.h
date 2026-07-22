// SPDX-FileCopyrightText:  2026 The DOSBox Staging Team
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_PROGRAM_APPEND_H
#define DOSBOX_PROGRAM_APPEND_H

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
};

#endif // DOSBOX_PROGRAM_APPEND_H
