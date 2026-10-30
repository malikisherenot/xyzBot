#include "State.hpp"
#include <Geode/modify/GJBaseGameLayer.hpp>

using namespace geode::prelude;

class $modify(XyzAssistGL, GJBaseGameLayer) {
    void toggleFlipped(bool flipped, bool noEffects) {
        if (xyz::cfg().noMirror) {
            m_gameState.m_unkBool10 = flipped;
            return;
        }
        GJBaseGameLayer::toggleFlipped(flipped, noEffects);
    }
};
