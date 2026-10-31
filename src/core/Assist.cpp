#include "State.hpp"
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>

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

class $modify(XyzLegacyPlayer, PlayerObject) {
    void update(float dt) {
        auto& c = xyz::cfg();
        if (!c.legacyShip || !m_isShip || !m_isUpsideDown) { PlayerObject::update(dt); return; }
        double before = m_yVelocity;
        PlayerObject::update(dt);
        if (!m_isOnGround) {
            double dv = m_yVelocity - before;
            m_yVelocity += dv * (std::clamp(c.legacyShipPct, 0, 100) / 100.0);
        }
    }
};
