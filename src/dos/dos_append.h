// SPDX-FileCopyrightText:  2026 Antigravity
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_DOS_APPEND_H
#define DOSBOX_DOS_APPEND_H

#include <string>

namespace dos_append {

void Init();
bool IsEnabled();
bool IsResolving();
bool ResolveName(const char* name, std::string& out_path);
void SetDirList(const std::string& new_list);
std::string GetDirList();
bool MultiplexHandler();



} // namespace dos_append

#endif // DOSBOX_DOS_APPEND_H
