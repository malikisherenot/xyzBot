// Lua scripting for xyzBot. Scripts are plain .lua files in geode-persistent/<mod>/scripts, they run in a sandbox
// (no io, os, require, load, debug) and react to events. See the example script the mod writes on first start.
#pragma once
#include <filesystem>
#include <string>
#include <vector>
#include <Geode/Geode.hpp>

namespace xyz::lua {

std::filesystem::path dir();
std::vector<std::string> files();            // names of the .lua files, sorted
bool enabled(std::string const& name);
void setEnabled(std::string const& name, bool on);
void reload();                               // reads every enabled script again
std::string status();                        // one line for the menu
std::vector<std::string> output();    // latest log lines and errors of the scripts

// events, called by the engine
void onTick(GJBaseGameLayer* gl);                        // once per physics tick, also presses queued clicks
void onInput(int button, bool down, bool player1);       // a real input of the player
void onDeath();
void onReset(int attempt);                               // a new attempt started

}
