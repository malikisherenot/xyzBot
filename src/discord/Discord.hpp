// Discord Rich Presence for PC (Windows and macOS). Talks to the running Discord client over its local IPC pipe, no library needed.
// Create an application on https://discord.com/developers/applications, copy its Application ID into kDiscordClientId below
// (or into the saved value "discord-app-id"), and upload a picture named "xyzbot" under Rich Presence > Art Assets if you want an icon.
#pragma once
#include <string>

namespace xyz::discord {

inline constexpr char kDiscordClientId[] = "1574305629047095336"; // the Application ID of xyzBot

bool on();
void setOn(bool v);          // starts or stops the presence
std::string status();        // for the menu
void onTick();               // called by the engine every physics tick, cheap

}
