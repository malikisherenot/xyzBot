// TPS optimisation: at high TPS one frame runs many ticks, but only the last one is ever drawn.
// While Loop.cpp is running the earlier ticks of a frame (rt().subTick) the display-only work is skipped.
// Physics, triggers and inputs are never touched.
#include "State.hpp"
#include <Geode/modify/PlayLayer.hpp>

using namespace geode::prelude;

namespace {
bool onSubTick(PlayLayer* pl) { return xyz::rt().subTick && !xyz::rt().simulating && xyz::isActive(pl); }
}

class $modify(XyzPerfPL, PlayLayer) {
    // progress bar and percentage label: pure display
    void updateProgressbar() {
        if (xyz::cfg().fastSub && onSubTick(this)) return;
        PlayLayer::updateProgressbar();
    }

    // object visibility / colour refresh: expensive on big levels; off by default (experimental)
    void updateVisibility(float dt) {
        if (xyz::cfg().skipVis && onSubTick(this)) return;
        PlayLayer::updateVisibility(dt);
    }
};
