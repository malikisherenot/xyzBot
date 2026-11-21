#pragma once
#include <cstdint>
#include <string>

namespace xyz::logger {
void event(std::string const& text);
void input(uint32_t tick, int button, bool down, bool p2, char const* source);
void position(uint32_t tick);
}
