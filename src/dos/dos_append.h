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
void SetDirList(const std::string& new_list, const char* reason = "Update APPEND directory search paths");
std::string GetDirList();
	bool MultiplexHandler();

#ifndef NDEBUG
	class AppendTraceScope {
		static uint32_t next_trace_id;
		static int depth;
		uint32_t id;
		std::string func_name;
		std::string rationale;
		void* timer_start; // Type erasure for chrono to keep header clean
	public:
		AppendTraceScope(const char* func, const char* rationale);
		~AppendTraceScope();
		void State(const char* key, const std::string& value);
		void Action(const char* description);
		void Decision(const char* choice, const char* reason);
		void Result(const char* result);
		void Error(const char* error, const std::string& details);
	};
#else
	class AppendTraceScope {
	public:
		AppendTraceScope(const char*, const char*) {}
		void State(const char*, const std::string&) {}
		void Action(const char*) {}
		void Decision(const char*, const char*) {}
		void Result(const char*) {}
		void Error(const char*, const std::string&) {}
	};
#endif

} // namespace dos_append

#endif // DOSBOX_DOS_APPEND_H
