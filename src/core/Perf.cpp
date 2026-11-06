#include "State.hpp"
#include <Geode/modify/PlayLayer.hpp>

using namespace geode::prelude;

namespace {
bool onSubTick(PlayLayer* pl) { return xyz::rt().subTick && !xyz::rt().simulating && xyz::isActive(pl); }
}

class $modify(XyzPerfPL, PlayLayer) {
    void updateProgressbar() {
        if (xyz::cfg().fastSub && onSubTick(this)) return;
        PlayLayer::updateProgressbar();
    }

    void updateVisibility(float dt) {
        if (xyz::cfg().skipVis && onSubTick(this)) return;
        PlayLayer::updateVisibility(dt);
    }
};
