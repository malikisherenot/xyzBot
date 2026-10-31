#pragma once
#include <Geode/Geode.hpp>
namespace xyz::vis {
void attach(GJBaseGameLayer* pl);
void detach();
void sampleTrail(GJBaseGameLayer* pl);
void refresh(GJBaseGameLayer* pl);
bool predictDeath(GJBaseGameLayer* pl, int look);
void updateLabels(GJBaseGameLayer* pl);
void hideLabels();
}
namespace xyz::labels {
struct Cfg { bool on = false; int anchor = 0; int font = 1; float opacity = 1.f; float scale = 0.7f; int r = 255, g = 255, b = 255; };
size_t count();
std::string const& id(size_t i);
std::string const& name(size_t i);
Cfg& cfg(size_t i);
std::string const& help(size_t i);
void save(size_t i);
void invalidate();
void setAllColor(int r, int g, int b);
}
