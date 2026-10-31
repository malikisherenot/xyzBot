#include "Vis.hpp"
#include "../core/State.hpp"
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/modify/EffectGameObject.hpp>
#include <Geode/modify/HardStreak.hpp>
#include <deque>

using namespace geode::prelude;

namespace xyz::vis {
namespace {
CCDrawNode* g_hb = nullptr;
CCDrawNode* g_tr = nullptr;
PlayerObject* g_fake[2] = {nullptr, nullptr};
std::deque<std::array<CCRect, 2>> g_trail[2];

ccColor4F col(float r, float g, float b, float a = 1.f) { return {r, g, b, a}; }

void poly(CCDrawNode* n, CCRect const& r, ccColor4F fill, float w, ccColor4F border) {
    CCPoint v[4] = {{r.getMinX(), r.getMinY()}, {r.getMaxX(), r.getMinY()}, {r.getMaxX(), r.getMaxY()}, {r.getMinX(), r.getMaxY()}};
    n->drawPolygon(v, 4, fill, w, border);
}

void obox(CCDrawNode* n, GameObject* o, ccColor4F c, float w) {
    auto b = o->getOrientedBox();
    if (b) n->drawPolygon(b->m_corners.data(), 4, {c.r, c.g, c.b, c.a * 0.15f}, w, c);
    else poly(n, o->getObjectRect(), {c.r, c.g, c.b, c.a * 0.15f}, w, c);
}

void drawPlayer(CCDrawNode* n, PlayerObject* p, float w) {
    auto& c = cfg();
    if (!p) return;
    if (c.hbRotated) obox(n, p, col(1.f, 0.6f, 0.f), w);
    if (c.hbPlayer) {
        float rad = p->getObjectRect().size.width / 2.f;
        if (rad > 0.f) n->drawCircle(p->getPosition(), rad, {0, 0, 0, 0}, w, col(1, 0.9f, 0), (unsigned)std::max(rad, 8.f) * 2);
        poly(n, p->getObjectRect(), col(1, 0, 0, 0.15f), w, col(1, 0, 0));
        poly(n, p->getObjectRect(0.3f, 0.3f), col(0, 0.5f, 1, 0.15f), w, col(0, 0.5f, 1));
    }
}

void drawHitboxes(GJBaseGameLayer* pl) {
    auto& c = cfg();
    g_hb->clear();
    float w = 0.6f / std::max(0.1f, (float)pl->m_gameState.m_cameraZoom);
    auto layer = pl->m_objectLayer;
    auto size = CCDirector::get()->getWinSize();
    auto a = layer->convertToNodeSpace({0, 0});
    auto b = layer->convertToNodeSpace({size.width, size.height});
    CCRect view(std::min(a.x, b.x), std::min(a.y, b.y), std::abs(b.x - a.x), std::abs(b.y - a.y));

    if (c.hbSolid || c.hbHazard || c.hbSpecial) {
        for (auto o : CCArrayExt<GameObject*>(pl->m_objects)) {
            if (!o || o->m_isGroupDisabled) continue;
            auto t = o->m_objectType;
            if (t == GameObjectType::Decoration) continue;
            auto r = o->getObjectRect();
            if (!view.intersectsRect(r)) continue;
            ccColor4F cl; bool on;
            if (t == GameObjectType::Hazard || t == GameObjectType::AnimatedHazard) { cl = col(1, 0.1f, 0.1f); on = c.hbHazard; }
            else if (t == GameObjectType::Solid || t == GameObjectType::Slope || t == GameObjectType::Breakable) { cl = col(0.2f, 0.45f, 1); on = c.hbSolid; }
            else { cl = col(0.1f, 1, 0.3f); on = c.hbSpecial; }
            if (!on) continue;
            if (t == GameObjectType::Hazard || t == GameObjectType::AnimatedHazard) {
                auto ply = typeinfo_cast<PlayLayer*>(pl);
                if (!o->m_isActivated || (ply && o == ply->m_anticheatSpike)) continue;
                float rad = o->m_objectRadius * std::max(o->m_scaleX, o->m_scaleY);
                if (rad > 0.f) {
                    auto segs = (unsigned)(std::max(rad, 8.f) * 2.f * std::sqrt(std::max(0.1f, (float)pl->m_gameState.m_cameraZoom)));
                    g_hb->drawCircle(o->getPosition(), rad, {cl.r, cl.g, cl.b, 0.12f}, w, cl, segs);
                    continue;
                }
            }
            if (c.hbRotated && o->getRotation() != 0.f) obox(g_hb, o, cl, w);
            else poly(g_hb, r, {cl.r, cl.g, cl.b, 0.12f}, w, cl);
        }
    }
    if (c.hbPlayer || c.hbRotated) {
        drawPlayer(g_hb, pl->m_player1, w);
        if (pl->m_gameState.m_isDualMode) drawPlayer(g_hb, pl->m_player2, w);
    }
    if (c.trail) {
        for (auto& tq : g_trail)
            for (size_t i = 0; i < tq.size(); ++i) {
                float al = 0.1f + 0.5f * (float)i / (float)tq.size();
                poly(g_hb, tq[i][0], col(0, 0, 0, 0), w, col(1, 0, 0, al));
            }
    }
}

void copyState(PlayerObject* f, PlayerObject* p) {
    f->toggleFlyMode(p->m_isShip, true);
    f->toggleBirdMode(p->m_isBird, true);
    f->toggleRollMode(p->m_isBall, true);
    f->toggleDartMode(p->m_isDart, true);
    f->toggleRobotMode(p->m_isRobot, true);
    f->toggleSpiderMode(p->m_isSpider, true);
    f->toggleSwingMode(p->m_isSwing, true);
    f->m_isUpsideDown = p->m_isUpsideDown;
    f->m_isSideways = p->m_isSideways;
    f->m_vehicleSize = p->m_vehicleSize;
    f->m_playerSpeed = p->m_playerSpeed;
    f->m_gravityMod = p->m_gravityMod;
    f->m_yVelocity = p->m_yVelocity;
    f->m_isOnGround = p->m_isOnGround;
    f->m_lastGroundObject = p->m_lastGroundObject;
    f->m_groundYVelocity = p->m_groundYVelocity;
    f->m_isGoingLeft = p->m_isGoingLeft;
    f->m_platformerXVelocity = p->m_platformerXVelocity;
    f->m_holdingRight = p->m_holdingRight;
    f->m_holdingLeft = p->m_holdingLeft;
    f->m_isRotating = p->m_isRotating;
    f->m_rotationSpeed = p->m_rotationSpeed;
    f->m_rotateSpeed = p->m_rotateSpeed;
    f->setPosition(p->getPosition());
    f->setRotation(p->getRotation());
    f->setScaleX(p->getScaleX());
    f->setScaleY(p->getScaleY());
    f->updatePlayerScale();
}

bool run(GJBaseGameLayer* pl, PlayerObject* fake, PlayerObject* real, bool hold, int len, bool draw, bool curHeld = false) {
    auto& r = rt();
    float delta = std::min(0.25f, 60.f / curTps());
    copyState(fake, real);
    r.simDead = false;
    auto color = hold ? col(0.2f, 1, 0.3f) : col(1, 0.3f, 0.3f);
    for (int i = 0; i < len; ++i) {
        auto prev = fake->getPosition();
        fake->m_collisionLogTop->removeAllObjects();
        fake->m_collisionLogBottom->removeAllObjects();
        fake->m_collisionLogLeft->removeAllObjects();
        fake->m_collisionLogRight->removeAllObjects();
        pl->checkCollisions(fake, delta, false);
        if (r.simDead) {
            if (draw) poly(g_tr, fake->getObjectRect(), col(1, 0, 0, 0.2f), 0.5f, col(1, 0, 0));
            return true;
        }
        if (i == 0 && (draw || hold != curHeld)) {
            if (hold) fake->pushButton(PlayerButton::Jump);
            else fake->releaseButton(PlayerButton::Jump);
        }
        fake->update(delta);
        fake->updateRotation(delta);
        fake->updatePlayerScale();
        if (draw) {
            auto cl = color;
            if (i >= len - 40) cl.a = (len - i) / 40.f;
            g_tr->drawSegment(prev, fake->getPosition(), 0.6f, cl);
        }
    }
    return false;
}

void drawTraj(GJBaseGameLayer* pl) {
    auto& r = rt();
    g_tr->clear();
    if (!g_fake[0] || !pl->m_player1 || pl->m_player1->m_isDead) return;
    int len = std::clamp(cfg().trajLen, 10, 2000);
    r.simulating = true;
    for (int hold = 1; hold >= 0; --hold) {
        run(pl, g_fake[0], pl->m_player1, hold, len, true);
        if (pl->m_gameState.m_isDualMode && pl->m_player2) run(pl, g_fake[1], pl->m_player2, hold, len, true);
    }
    r.simulating = false;
}
}

void attach(GJBaseGameLayer* pl) {
    g_hb = CCDrawNode::create(); g_hb->retain();
    g_tr = CCDrawNode::create(); g_tr->retain();
    pl->m_objectLayer->addChild(g_hb, 1000);
    pl->m_objectLayer->addChild(g_tr, 1001);
    for (auto& f : g_fake) {
        f = PlayerObject::create(1, 1, pl, pl, true);
        f->retain();
        f->setVisible(false);
        pl->m_objectLayer->addChild(f);
    }
    g_trail[0].clear(); g_trail[1].clear();
}

void detach() {
    for (auto& f : g_fake) { CC_SAFE_RELEASE(f); f = nullptr; }
    CC_SAFE_RELEASE(g_hb); g_hb = nullptr;
    CC_SAFE_RELEASE(g_tr); g_tr = nullptr;
}

void sampleTrail(GJBaseGameLayer* pl) {
    auto& c = cfg();
    PlayerObject* ps[2] = {pl->m_player1, pl->m_gameState.m_isDualMode ? pl->m_player2 : nullptr};
    for (int i = 0; i < 2; ++i) {
        if (!ps[i]) continue;
        g_trail[i].push_back({ps[i]->getObjectRect(), ps[i]->getObjectRect(0.3f, 0.3f)});
        while ((int)g_trail[i].size() > c.trailLen) g_trail[i].pop_front();
    }
}

bool predictDeath(GJBaseGameLayer* pl, int look) {
    auto& r = rt();
    if (!g_fake[0] || !pl->m_player1) return false;
    r.simulating = true;
    bool dead = run(pl, g_fake[0], pl->m_player1, r.want[0], look, false, r.gameHeld[0]);
    if (!dead && pl->m_gameState.m_isDualMode && pl->m_player2)
        dead = run(pl, g_fake[1], pl->m_player2, r.want[1], look, false, r.gameHeld[1]);
    r.simulating = false;
    return dead;
}

void refresh(GJBaseGameLayer* pl) {
    auto& c = cfg();
    updateLabels(pl);
    if (!g_hb || !g_tr) return;
    g_hb->setVisible(c.hitboxes);
    g_tr->setVisible(c.traj);
    if (c.hitboxes) drawHitboxes(pl); else g_hb->clear();
    if (c.traj) drawTraj(pl); else g_tr->clear();
}
}

class $modify(XyzSimGL, GJBaseGameLayer) {
    bool canBeActivatedByPlayer(PlayerObject* p, EffectGameObject* o) {
        if (xyz::rt().simulating) return false;
        return GJBaseGameLayer::canBeActivatedByPlayer(p, o);
    }
    void playerTouchedRing(PlayerObject* p, RingObject* o) {
        if (!xyz::rt().simulating) GJBaseGameLayer::playerTouchedRing(p, o);
    }
    void playerTouchedTrigger(PlayerObject* p, EffectGameObject* o) {
        if (!xyz::rt().simulating) GJBaseGameLayer::playerTouchedTrigger(p, o);
    }
    void gameEventTriggered(GJGameEvent e, int a, int b) {
        if (!xyz::rt().simulating) GJBaseGameLayer::gameEventTriggered(e, a, b);
    }
};

class $modify(XyzSimPlayer, PlayerObject) {
    void incrementJumps() { if (!xyz::rt().simulating) PlayerObject::incrementJumps(); }
    void ringJump(RingObject* o, bool b) { if (!xyz::rt().simulating) PlayerObject::ringJump(o, b); }
};

class $modify(XyzSimStreak, HardStreak) {
    void addPoint(CCPoint p) { if (!xyz::rt().simulating) HardStreak::addPoint(p); }
};

class $modify(XyzSimTrigger, EffectGameObject) {
    void triggerObject(GJBaseGameLayer* l, int i, gd::vector<int> const* v) {
        if (!xyz::rt().simulating) EffectGameObject::triggerObject(l, i, v);
    }
};
