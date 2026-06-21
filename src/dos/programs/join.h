#ifndef DOSBOX_PROGRAM_JOIN_H
#define DOSBOX_PROGRAM_JOIN_H

#include "dos/programs.h"

class JOIN final : public Program {
public:
	JOIN()
	{
		help_detail = {HELP_Filter::All,
		               HELP_Category::Misc,
		               HELP_CmdType::Program,
		               "JOIN"};
	}
	void Run() override;
};

#endif
