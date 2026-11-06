// "Fix Rotation Bug", ported from Echo (GPL-3.0).
#include "Vis.hpp"
#include "../core/State.hpp"
#include <Geode/modify/PlayerObject.hpp>
#include <unordered_map>

using namespace geode::prelude;

namespace {
std::unordered_map<PlayerObject*, float> g_fix;
std::unordered_map<PlayerObject*, float> g_held;
std::unordered_map<PlayerObject*, float> g_left;

bool tiltMode(PlayerObject* p) { return p->m_isShip || p->m_isBird || p->m_isDart || p->m_isSwing; }
bool tiltApplies(PlayerObject* p) { return xyz::cfg().fixRotation && tiltMode(p) && !xyz::rt().simulating; }

float visualTilt(PlayerObject* p) {
    float now = p->getRotation();
    auto left = g_left.find(p);
    auto shown = g_fix.find(p);
    bool fresh = left == g_left.end() || left->second != now || shown == g_fix.end();
    return fresh ? now : shown->second;
}

template <class Gameplay, class Visual>
void turnBoth(PlayerObject* p, Gameplay&& gameplay, Visual&& visual) {
    float vis = visualTilt(p);
    gameplay();
    float real = p->getRotation();
    p->setRotation(vis);
    visual();
    g_fix[p] = p->getRotation();
    g_left[p] = real;
    p->setRotation(real);
}

void restoreOne(PlayerObject* p) {
    if (!p) return;
    auto held = g_held.find(p);
    if (held == g_held.end()) return;
    p->setRotation(held->second);
    g_held.erase(held);
}

void presentOne(PlayerObject* p) {
    if (!p || !tiltApplies(p) || !g_fix.contains(p)) return;
    float vis = visualTilt(p);
    g_held[p] = p->getRotation();
    p->setRotation(vis);
}
}

namespace xyz::vis {
void rotClear() { g_fix.clear(); g_held.clear(); g_left.clear(); }
void rotRestore(GJBaseGameLayer* gl) {
    if (!gl) return;
    restoreOne(gl->m_player1);
    restoreOne(gl->m_player2);
}
void rotPresent(GJBaseGameLayer* gl) {
    if (!gl || !cfg().fixRotation) return;
    presentOne(gl->m_player1);
    presentOne(gl->m_player2);
}
}

class $modify(XyzRotPlayer, PlayerObject) {
    void updateShipRotation(float dt) {
        restoreOne(this);
        if (!tiltApplies(this)) {
            g_fix.erase(this);
            PlayerObject::updateShipRotation(dt);
            return;
        }
        turnBoth(this, [this, dt] { PlayerObject::updateShipRotation(dt); },
            [this, dt] {
                auto pos = getPosition();
                auto back = pos - m_shipRotation;
                float moved = back.x * back.x + back.y * back.y;
                float need = dt * 1.25f + 0.01f;
                auto saved = m_shipRotation;
                if (moved > 1e-8f && moved < need) m_shipRotation = pos - back * std::sqrt(need / moved);
                PlayerObject::updateShipRotation(dt);
                m_shipRotation = saved;
            });
    }

#ifndef GEODE_IS_IOS
    void updateRotation(float dt, float rotation) {
        restoreOne(this);
        if (!tiltApplies(this)) {
            PlayerObject::updateRotation(dt, rotation);
            return;
        }
        auto turn = [this, dt, rotation] { PlayerObject::updateRotation(dt, rotation); };
        turnBoth(this, turn, turn);
    }
#endif

    void setPosition(CCPoint const& position) {
        if (!tiltApplies(this) || !g_fix.contains(this) || g_held.contains(this)) {
            PlayerObject::setPosition(position);
            return;
        }
        float real = getRotation();
        setRotation(visualTilt(this));
        PlayerObject::setPosition(position);
        setRotation(real);
    }
};
