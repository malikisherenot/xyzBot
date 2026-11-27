// Clickbot: plays a click sound for every input of a macro (or of the player) and can mix the clicks into rendered videos.
// Clickpacks are folders or .zip files in geode-persistent/<mod>/clickpacks. Layout (same as ZCB and most packs):
//   clicks/ releases/ softclicks/ softreleases/ hardclicks/ hardreleases/ microclicks/ microreleases/
//   optionally player1/ and player2/ with the same folders inside (without them both players share the sounds).
#pragma once
#include <filesystem>
#include <string>
#include <vector>

namespace xyz::clickbot {

std::filesystem::path dir();
std::vector<std::string> packs();          // folders and .zip files in dir()
std::string current();                     // selected pack
void select(std::string const& name);
void reload();                             // scans the folder again and loads the selected pack
std::string status();

bool on();            void setOn(bool v);
bool liveClicks();    void setLiveClicks(bool v);   // also click on the player's own presses
bool mixRender();     void setMixRender(bool v);    // mix the clicks into rendered videos
int volume();         void setVolume(int percent);  // 0..300

// events from the engine, button 1 only (jump); real = pressed by the player and not by a macro
void onInput(int button, bool down, bool player1, bool real);
void onTick();
// rendering: collects the clicks of a render and builds the audio track (stereo, 44100 Hz, interleaved)
void renderBegin();
std::vector<float> renderTrack(double videoSeconds);

}
