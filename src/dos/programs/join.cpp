#include "join.h"
#include <string>
#include "shell/shell.h"
#include "utils/string_utils.h"

void JOIN::Run()
{
	std::string tmp = "";
	cmd->GetStringRemain(tmp);

	char args[CMD_MAXLINE];
	safe_strcpy(args, tmp.c_str());

	auto shell = DOS_GetFirstShell();
	assert(shell);

	shell->CMD_JOIN(args);
}
