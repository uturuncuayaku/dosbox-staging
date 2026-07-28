// LOCAL DEBUG TRACE IMPLEMENTATION
// Saved globally outside the repo to prevent accidental commits.

#ifndef NDEBUG

#include <string>
#include <cstdint>

class StateTrace {
	static uint32_t next_trace_id;
	static int depth;
	uint32_t id;
	std::string func_name;
	std::string rationale;
	void* timer_start;
public:
	StateTrace(const char* func, const char* rationale);
	~StateTrace();
	void State(const char* key, const std::string& value);
	void Action(const char* description);
	void Decision(const char* choice, const char* reason);
	void Result(const char* result);
	void Error(const char* error, const std::string& details);
};

uint32_t StateTrace::next_trace_id = 0;
int StateTrace::depth              = 0;

StateTrace::StateTrace(const char* func, const char* rationale)
        : func_name(func), rationale(rationale) {
}

StateTrace::~StateTrace() {
}

void StateTrace::State(const char* key, const std::string& value) {}
void StateTrace::Action(const char* description) {}
void StateTrace::Decision(const char* choice, const char* reason) {}
void StateTrace::Result(const char* result) {}
void StateTrace::Error(const char* error, const std::string& details) {}

#endif
