// SPDX-FileCopyrightText:  2026 Antigravity
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DOSBOX_DOS_APPEND_H
#define DOSBOX_DOS_APPEND_H

#include <optional>
#include <string>
#include <string_view>

namespace dos_append {

void Init();
bool IsEnabled();
bool IsResolving();
bool find_absolute_path(const char* target_path, std::string& absolute_path);
std::optional<std::string> ValidateDirectories(std::string_view args);
void SetDirectories(const std::string& new_list);
std::string GetDirectories();
bool MultiplexHandler();
void SetFlags(bool env, bool path_on, bool exec);
bool IsEnvOn();
bool IsPathOverrideOn();
bool IsExecOn();
} // namespace dos_append

#endif // DOSBOX_DOS_APPEND_H
