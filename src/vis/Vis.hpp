#pragma once
#include <Geode/Geode.hpp>
namespace xyz::vis {
void attach(PlayLayer* pl);
void detach();
void sampleTrail(PlayLayer* pl);
void refresh(PlayLayer* pl);
bool predictDeath(PlayLayer* pl, int look);
void updateLabels(PlayLayer* pl);
}
namespace xyz::labels {
struct Cfg { bool on = false; int anchor = 0; int font = 1; float opacity = 1.f; float scale = 0.7f; };
size_t count();
std::string const& id(size_t i);
std::string const& name(size_t i);
Cfg& cfg(size_t i);
void save(size_t i);
void invalidate();
}
