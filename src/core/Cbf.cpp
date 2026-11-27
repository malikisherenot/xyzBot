// CBF (click between steps): an input that falls inside a physics step splits the step there, like the CBF mod
// (MIT, https://github.com/ninXout/Click-Between-Frames, Android part based on Click on Steps by zmx).
// The inputs come from Engine.cpp (rt().split, filled once per tick by the recorder and by the macro player).
#include "State.hpp"
#include <Geode/modify/PlayerObject.hpp>
#include <algorithm>

using namespace geode::prelude;

namespace {
CCPoint g_p1Pos = {0.f, 0.f}, g_p2Pos = {0.f, 0.f};
float g_rotDelta = 0.f;
bool g_p1Split = false, g_p2Split = false;
bool g_inputStep = false; // this step has inputs inside it, P2 is updated from P1's update
bool g_mid = false;       // inside our own split loop

// PlayerObject::resetCollisionLog is inlined in the game, so it is written out like CBF does
void resetLog(PlayerObject* p) {
    p->m_collisionLogTop->removeAllObjects();
    p->m_collisionLogBottom->removeAllObjects();
    p->m_collisionLogLeft->removeAllObjects();
    p->m_collisionLogRight->removeAllObjects();
    p->m_lastCollisionLeft = -1;
    p->m_lastCollisionRight = -1;
    p->m_lastCollisionBottom = -1;
    p->m_lastCollisionTop = -1;
}

bool notBuffering(PlayerObject* p) {
    return p->m_isOnGround || (p->m_touchingRings && p->m_touchingRings->count()) || p->m_isDashing
        || p->m_isDart || p->m_isBird || p->m_isShip || p->m_isSwing;
}
}

class $modify(XyzCbfPlayer, PlayerObject) {
    void update(float dt) {
        auto& r = xyz::rt();
        auto gl = this->m_gameLayer;
        if (g_mid || !gl || !xyz::cfg().cbf) { PlayerObject::update(dt); return; }

        PlayerObject* p1 = gl->m_player1;
        PlayerObject* p2 = gl->m_player2;
        if (this != p1) {
            // while a step is split P2 is updated from P1's update
            if (!(g_inputStep && this == p2)) PlayerObject::update(dt);
            return;
        }
        if (r.split.empty() || !p2) {
            g_p1Split = g_p2Split = g_inputStep = false;
            PlayerObject::update(dt);
            return;
        }

        std::stable_sort(r.split.begin(), r.split.end(), [](xyz::SplitIn const& a, xyz::SplitIn const& b) { return a.f < b.f; });
        auto inputs = std::move(r.split);
        r.split.clear();

        bool dual = gl->m_gameState.m_isDualMode;
        bool p1Ground = this->m_isOnGround, p2Ground = p2->m_isOnGround;
        g_p1Pos = this->getPosition();
        g_p2Pos = p2->getPosition();
        g_p1Split = notBuffering(this);
        g_p2Split = dual && notBuffering(p2);
        g_inputStep = true;
        g_mid = true;

        float prev = 0.f;
        bool first = true;
        for (size_t k = 0; k <= inputs.size(); ++k) {
            bool end = k == inputs.size();
            float upto = end ? 1.f : std::clamp(inputs[k].f, 0.f, 1.f);
            float sub = dt * std::max(0.f, upto - prev);
            g_rotDelta = sub;

            if (g_p1Split) {
                PlayerObject::update(sub);
                if (!end) {
                    if (first && ((this->m_yVelocity < 0) ^ this->m_isUpsideDown)) this->m_isOnGround = p1Ground;
                    if (!this->m_isOnSlope || this->m_isDart) gl->checkCollisions(this, 0.f, true);
                    else gl->checkCollisions(this, dt, true);
                    PlayerObject::updateRotation(sub);
                    resetLog(this);
                }
            } else if (end) PlayerObject::update(dt); // not in a state that can split: the whole step runs after the input

            if (g_p2Split) {
                p2->update(sub);
                if (!end) {
                    if (first && ((p2->m_yVelocity < 0) ^ p2->m_isUpsideDown)) p2->m_isOnGround = p2Ground;
                    if (!p2->m_isOnSlope || p2->m_isDart) gl->checkCollisions(p2, 0.f, true);
                    else gl->checkCollisions(p2, dt, true);
                    p2->updateRotation(sub);
                    resetLog(p2);
                }
            } else if (end) p2->update(dt);

            if (!end) xyz::applySplitInput(gl, inputs[k]);
            prev = upto;
            first = false;
        }
        g_mid = false;
    }

    // called by the game right after the step: finish the rotation and put m_lastPosition back (move triggers, spider)
    void updateRotation(float t) {
        auto gl = this->m_gameLayer;
        if (gl && !g_mid && xyz::cfg().cbf) {
            if (this == gl->m_player1 && g_p1Split) {
                PlayerObject::updateRotation(g_rotDelta);
                this->m_lastPosition = g_p1Pos;
                return;
            }
            if (this == gl->m_player2 && g_p2Split) {
                PlayerObject::updateRotation(g_rotDelta);
                this->m_lastPosition = g_p2Pos;
                return;
            }
        }
        PlayerObject::updateRotation(t);
    }
};
