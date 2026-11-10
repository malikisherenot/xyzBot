#pragma once
#include <string>
#include <vector>

namespace xyz::updater {
enum class State { Idle, Checking, UpToDate, Available, Downloading, Done, Failed };
State state();
std::string const& message();
std::string const& latest();
std::vector<std::string> const& notes();
void check(bool manual);
void download();
void autoCheck();
void restart();
inline char const* releasesUrl() { return "https://github.com/malikisherenot/xyzBot/releases"; }
}
