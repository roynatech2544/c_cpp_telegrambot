#include "handler.h"
#include <csignal>
#include <cstdlib>

static exit_handler_t fn;

void installExitHandler(exit_handler_t fn_) {
	fn = fn_;
	std::signal(SIGINT, fn);
	std::signal(SIGTERM, fn);
	static auto fnV = []() { fn(0); };
	std::atexit(fnV);
}