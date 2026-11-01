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

// ---- Scarlet Utils port (own implementation): stabilizers, pad clicks, flip input on death ----
namespace xyz {
static void swiftClick(GJBaseGameLayer* gl, int p) {
    pressButton(gl, true, p);
    pressButton(gl, false, p);
}

void flipOnReset() {
    auto& c = cfg(); auto& r = rt();
    if (!c.flipDeath || r.flipPlayer == 0) { r.flipPlayer = 0; return; }
    for (int p = 0; p < 2; ++p) {
        bool who = (p == 0 ? (c.flipP1 && r.flipPlayer == 1) : (c.flipP2 && r.flipPlayer == 2)) || c.flipBoth;
        if (!who) continue;
        if (c.flipSwift) r.flipSwiftNow[p] = true;
        else {
            if (r.flipLogic[p]) r.flipPress[p] = true;
            r.flipLogic[p] = !r.flipLogic[p];
        }
    }
    r.flipPlayer = 0;
}

void assistTick(GJBaseGameLayer* gl) {
    auto& c = cfg(); auto& r = rt();
    r.clickedPad = false; r.optStart = false; r.optStop = false; r.optRing = false;
    for (int p = 0; p < 2; ++p) {
        auto pl = p == 0 ? gl->m_player1 : gl->m_player2;
        if (!pl || (p == 1 && !gl->m_gameState.m_isDualMode)) continue;

        if (r.flipPress[p]) { r.flipPress[p] = false; pressButton(gl, true, p); }
        if (r.flipSwiftNow[p]) { r.flipSwiftNow[p] = false; swiftClick(gl, p); }
        if (r.padClick[p]) { r.padClick[p] = false; swiftClick(gl, p); }

        // Auto straight fly: keep the ship level by pressing/releasing against its vertical speed.
        if (c.sFly && c.sFlyOn[p] && pl->m_isShip) {
            bool holding = r.gameHeld[p];
            float thr = c.sFlyThr[p];
            double vy = pl->m_yVelocity;
            bool up = pl->m_isUpsideDown;
            bool wantPress = !holding && ((!up && vy < -thr) || (up && vy > thr));
            bool wantRelease = holding && ((!up && vy > thr) || (up && vy < -thr));
            if (wantPress) pressButton(gl, true, p);
            else if (wantRelease) pressButton(gl, false, p);
        }

        // Auto straight UFO: tap whenever the UFO drifts past the target height while falling.
        if (c.sUfo && c.sUfoOn[p] && pl->m_isBird) {
            float y = pl->getPositionY();
            double vy = pl->m_yVelocity;
            bool up = pl->m_isUpsideDown;
            if ((!up && y < c.sUfoTarget[p] - c.sUfoThr[p] && vy <= 0) || (up && y > c.sUfoTarget[p] + c.sUfoThr[p] && vy >= 0))
                swiftClick(gl, p);
        }
    }
}
}

class $modify(XyzPadPlayer, PlayerObject) {
    void propellPlayer(float yVelocity, bool noEffects, int objectType) {
        auto& r = xyz::rt();
        if (objectType == 10 && xyz::cfg().clickGravPads && !r.clickedPad && !r.simulating) {
            r.padClick[m_isSecondPlayer ? 1 : 0] = true;
            r.clickedPad = true;
        }
        PlayerObject::propellPlayer(yVelocity, noEffects, objectType);
    }

    void bumpPlayer(float bumpMod, int objectType, bool noEffects, GameObject* object) {
        auto& r = xyz::rt();
        if ((objectType == 9 || objectType == 8 || objectType == 34) && xyz::cfg().clickPads && !r.clickedPad && !r.simulating) {
            r.padClick[m_isSecondPlayer ? 1 : 0] = true;
            r.clickedPad = true;
        }
        PlayerObject::bumpPlayer(bumpMod, objectType, noEffects, object);
    }
};
