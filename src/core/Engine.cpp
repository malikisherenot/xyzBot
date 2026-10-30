#include <ctime>
#include "State.hpp"
#include "../vis/Vis.hpp"
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/binding/CheckpointObject.hpp>
#include <Geode/binding/GameToolbox.hpp>
#include <unordered_map>

using namespace geode::prelude;

namespace xyz {
static size_t hi(int button, bool p2) { return (p2 ? 3 : 0) + std::clamp(button, 1, 3) - 1; }
static bool takeover() { auto& c = cfg(); return c.maintainGravity || c.mirror || c.acOn[0] || c.acOn[1]; }

void cpsPush(bool p2) {
    auto pl = PlayLayer::get();
    if (!pl) return;
    auto& r = rt();
    r.cpsT[p2].push_back(pl->m_gameState.m_levelTime);
    r.maxCps[p2] = std::max(r.maxCps[p2], cpsNow(p2));
}

int cpsNow(int p) {
    auto pl = PlayLayer::get();
    auto& v = rt().cpsT[p];
    if (!pl) return 0;
    double t = pl->m_gameState.m_levelTime;
    v.erase(std::remove_if(v.begin(), v.end(), [&](double x) { return t - x > 1.0 || x > t; }), v.end());
    return (int)v.size();
}

void computeWant() {
    auto& r = rt(); auto& c = cfg();
    if (!takeover() || r.mode == Mode::Play) { r.want = {r.gameHeld[0], r.gameHeld[1]}; return; }
    bool d[2] = {r.userHeld[0] || r.acHeld[0], r.userHeld[1] || r.acHeld[1]};
    if (c.mirror) { bool a = d[0] || d[1]; d[0] = a; d[1] = a != c.mirrorInv; }
    if (c.maintainGravity) {
        if (auto pl = PlayLayer::get()) {
            if (pl->m_player1) d[0] = d[0] != pl->m_player1->m_isUpsideDown;
            if (pl->m_gameState.m_isDualMode && pl->m_player2) d[1] = d[1] != pl->m_player2->m_isUpsideDown;
        }
    }
    r.want = {d[0], d[1]};
}

static void press(GJBaseGameLayer* gl, bool down, int p) {
    auto& r = rt();
    r.internal = true;
    gl->handleButton(down, 1, p == 0);
    r.internal = false;
}

static void acStep(GJBaseGameLayer* gl, int p) {
    auto& r = rt(); auto& c = cfg(); auto& a = r.ac[p];
    if (!c.acOn[p]) { r.acHeld[p] = false; a = Ac{}; return; }
    uint64_t tick = r.tick;
    if (a.last == tick) return;
    uint64_t nxt = a.last + (a.clicked ? c.acHold[p] : c.acRel[p]);
    if (tick >= nxt || a.last == UINT64_MAX) {
        a.last = tick;
        if (!a.clicked) {
            r.acHeld[p] = true; a.clicked = true;
            computeWant();
            if (r.want[p] != r.gameHeld[p]) press(gl, r.want[p], p);
            for (int k = 1; k < c.acClicks[p]; ++k) { press(gl, !r.want[p], p); press(gl, r.want[p], p); }
        } else { r.acHeld[p] = false; a.clicked = false; }
    }
}

static void drive(GJBaseGameLayer* gl) {
    auto& r = rt();
    if (r.mode == Mode::Play || !takeover()) return;
    acStep(gl, 0); acStep(gl, 1);
    computeWant();
    for (int p = 0; p < 2; ++p)
        if (r.want[p] != r.gameHeld[p]) press(gl, r.want[p], p);
}

static void rewind() {
    auto& r = rt();
    r.tick = 0; r.nextIn = 0; r.nextFix = 0; r.held.fill(false); r.leftover = 0;
    r.gameHeld = {false, false}; r.acHeld = {false, false}; r.ac[0] = Ac{}; r.ac[1] = Ac{};
    r.cpsT[0].clear(); r.cpsT[1].clear(); r.maxCps[0] = r.maxCps[1] = 0;
    r.frozen = false; r.autosaveAcc = 0.0;
    if (r.mode == Mode::Record) {
        r.rec = Macro{};
        r.rec.tps = (uint32_t)curTps();
        r.rec.levelId = r.levelId;
        r.rec.seed = GameToolbox::getfast_srand();
    } else if (r.mode == Mode::Play) {
        GameToolbox::fast_srand(r.play.seed);
    }
}

static void applyFix(PlayLayer* pl, Fix const& f) {
    auto p = f.p2 ? pl->m_player2 : pl->m_player1;
    if (!p || (f.p2 && !pl->m_gameState.m_isDualMode)) return;
    p->setPosition({f.x, f.y});
    p->setRotation(f.rot);
    p->m_yVelocity = f.yv;
}

static void feed(GJBaseGameLayer* gl) {
    auto& r = rt();
    if (r.mode != Mode::Play) return;
    auto& m = r.play;
    while (r.nextIn < m.inputs.size() && m.inputs[r.nextIn].tick <= r.tick) {
        auto& i = m.inputs[r.nextIn++];
        r.feeding = true;
        gl->handleButton(i.down, i.button, !i.p2);
        r.feeding = false;
        r.held[hi(i.button, i.p2)] = i.down;
        if (i.down && i.button == 1) cpsPush(i.p2);
    }
}

static void closeTick(PlayLayer* pl) {
    auto& r = rt();
    if (r.mode == Mode::Play) {
        while (r.nextFix < r.play.fixes.size() && r.play.fixes[r.nextFix].tick <= r.tick)
            applyFix(pl, r.play.fixes[r.nextFix++]);
        r.tick++;
        if (r.tick > r.play.ticks + 1) {
            stopAll(false);
            if (cfg().autoExit) queueInMainThread([] {
                if (auto p = PlayLayer::get()) {
                    p->onQuit();
                    CCDirector::sharedDirector()->popSceneWithTransition(0.5f, PopTransition::kPopTransitionFade);
                }
            });
        }
    } else if (r.mode == Mode::Record) {
        if (cfg().fixes) {
            auto add = [&](PlayerObject* p, bool p2) {
                if (p) r.rec.fixes.push_back({r.tick, p2, p->getPositionX(), p->getPositionY(), p->getRotation(), (float)p->m_yVelocity});
            };
            add(pl->m_player1, false);
            if (pl->m_gameState.m_isDualMode) add(pl->m_player2, true);
        }
        r.tick++;
        r.rec.ticks = r.tick;
    } else {
        r.tick++;
    }
    if (cfg().trail) vis::sampleTrail(pl);
}

class $modify(XyzGL, GJBaseGameLayer) {
    void handleButton(bool down, int button, bool p1) {
        auto& r = rt(); auto& c = cfg();
        auto pl = PlayLayer::get();
        if (!pl || (GJBaseGameLayer*)pl != this || r.simulating) { GJBaseGameLayer::handleButton(down, button, p1); return; }
        bool p2 = !p1;
        if (r.mode == Mode::Play && !r.feeding && !r.resetting && c.blockInputs) return;
        bool ext = !r.internal && !r.feeding && !r.resetting;
        if (ext && button == 1) {
            r.userHeld[p2] = down;
            if (down) cpsPush(p2);
            if (takeover() && r.mode != Mode::Play) return;
        }
        if (r.mode == Mode::Record && !r.resetting && button >= 1 && button <= 3) {
            r.held[hi(button, p2)] = down;
            r.rec.inputs.push_back({r.tick, (uint8_t)button, down, p2});
        }
        if (button == 1) r.gameHeld[p2] = down;
        GJBaseGameLayer::handleButton(down, button, p1);
    }

    void processCommands(float dt, bool half, bool last) {
        auto pl = PlayLayer::get();
        if (!pl || (GJBaseGameLayer*)pl != this || rt().simulating) {
            GJBaseGameLayer::processCommands(dt, half, last);
            return;
        }
        if (!half) { drive(this); feed(this); }
        GJBaseGameLayer::processCommands(dt, half, last);
        if (!half) closeTick(pl);
    }
};

class $modify(XyzPL, PlayLayer) {
    struct Snap { uint32_t tick; size_t nextIn, nextFix; std::array<bool, 6> held; uint64_t seed; };
    struct Fields { std::unordered_map<CheckpointObject*, Snap> cps; };

    bool init(GJGameLevel* level, bool a, bool b) {
        if (!PlayLayer::init(level, a, b)) return false;
        rt().levelId = level->m_levelID.value();
        return true;
    }

    void setupHasCompleted() {
        PlayLayer::setupHasCompleted();
        vis::attach(this);
    }

    void resetLevel() {
        auto& r = rt();
        r.resetting = true; r.loadedCp = false;
        PlayLayer::resetLevel();
        r.resetting = false;
        if (!r.loadedCp) rewind();
    }

    void resetLevelFromStart() {
        auto& r = rt();
        r.resetting = true;
        m_fields->cps.clear(); r.cpCount = 0;
        PlayLayer::resetLevelFromStart();
        r.resetting = false;
        rewind();
    }

    CheckpointObject* createCheckpoint() {
        auto cp = PlayLayer::createCheckpoint();
        auto& r = rt();
        if (cp) m_fields->cps[cp] = {r.tick, r.nextIn, r.nextFix, r.held, GameToolbox::getfast_srand()};
        r.cpCount = m_fields->cps.size();
        return cp;
    }

    void removeAllCheckpoints() {
        PlayLayer::removeAllCheckpoints();
        m_fields->cps.clear(); rt().cpCount = 0;
    }

    void loadFromCheckpoint(CheckpointObject* cp) {
        auto& r = rt();
        auto it = m_fields->cps.find(cp);
        if (r.mode == Mode::Off || it == m_fields->cps.end()) { PlayLayer::loadFromCheckpoint(cp); return; }
        auto s = it->second;
        auto live = r.held;
        r.resetting = true;
        PlayLayer::loadFromCheckpoint(cp);
        GameToolbox::fast_srand(s.seed);
        r.tick = s.tick; r.held = s.held;
        if (r.mode == Mode::Record) {
            auto& in = r.rec.inputs; auto& fx = r.rec.fixes;
            in.erase(std::remove_if(in.begin(), in.end(), [&](auto& i) { return i.tick >= s.tick; }), in.end());
            fx.erase(std::remove_if(fx.begin(), fx.end(), [&](auto& f) { return f.tick >= s.tick; }), fx.end());
            for (size_t i = 0; i < 6; ++i)
                if (live[i] != s.held[i]) {
                    in.push_back({s.tick, (uint8_t)(i % 3 + 1), live[i], i >= 3});
                    GJBaseGameLayer::handleButton(live[i], i % 3 + 1, i < 3);
                }
            r.held = live;
        } else {
            r.nextIn = s.nextIn; r.nextFix = s.nextFix;
            r.feeding = true;
            for (size_t i = 0; i < 6; ++i)
                if (live[i] != s.held[i]) GJBaseGameLayer::handleButton(s.held[i], i % 3 + 1, i < 3);
            r.feeding = false;
        }
        r.resetting = false;
        r.loadedCp = true;
    }

    void destroyPlayer(PlayerObject* p, GameObject* o) {
        auto& r = rt();
        if (r.simulating) { r.simDead = true; return; }
        if (cfg().noclip) return;
        PlayLayer::destroyPlayer(p, o);
        if (r.mode == Mode::Play && p && p->m_isDead && !m_isPracticeMode) stopAll(false);
    }

    void levelComplete() {
        if (rt().mode != Mode::Off) stopAll(rt().mode == Mode::Record);
        PlayLayer::levelComplete();
    }

    void onQuit() {
        if (rt().mode != Mode::Off) stopAll(rt().mode == Mode::Record);
        vis::detach();
        PlayLayer::onQuit();
    }
};

bool startRecord() {
    auto pl = PlayLayer::get(); auto& r = rt();
    if (!pl) return false;
    r.mode = Mode::Record;
    pl->resetLevelFromStart();
    return true;
}

bool startPlay(std::string const& name) {
    auto pl = PlayLayer::get(); auto& r = rt();
    if (!pl || !loadMacro(r.play, name)) return false;
    if (!cfg().ignoreLevel && r.play.levelId != 0 && r.play.levelId != pl->m_level->m_levelID.value()) return false;
    r.prevTpsVal = cfg().tpsVal; r.savedTps = cfg().tps;
    cfg().tps = true; cfg().tpsVal = (float)r.play.tps;
    r.mode = Mode::Play;
    pl->resetLevelFromStart();
    return true;
}

void stopAll(bool save) {
    auto& r = rt();
    if (r.mode == Mode::Off) return;
    if (r.mode == Mode::Record && save && !r.rec.inputs.empty()) {
        saveMacro(r.rec, fmt::format("macro_{}", (long long)std::time(nullptr)));
        Notification::create("Macro saved", NotificationIcon::Success)->show();
    }
    if (r.mode == Mode::Play) { cfg().tps = r.savedTps; cfg().tpsVal = r.prevTpsVal; }
    r.mode = Mode::Off;
    r.held.fill(false);
}
}
