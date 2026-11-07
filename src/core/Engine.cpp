#include <ctime>
#include <cstring>
#include <random>
#include "State.hpp"
#include <cmath>
#include "../vis/Vis.hpp"
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/LevelEditorLayer.hpp>
#include <Geode/modify/EditorUI.hpp>
#include <Geode/binding/CheckpointObject.hpp>
#include <Geode/binding/GameToolbox.hpp>
#include <unordered_map>

using namespace geode::prelude;

namespace xyz {
std::string macroFileName() {
    std::string out;
    if (!cfg().autoName) return "macro";
    for (char c : rt().levelName) {
        if ((unsigned char)c < 32 || std::strchr("\\/:*?\"<>|", c)) continue;
        out += c;
    }
    while (!out.empty() && (out.back() == ' ' || out.back() == '.')) out.pop_back();
    while (!out.empty() && out.front() == ' ') out.erase(out.begin());
    return out.empty() ? std::string("macro") : out;
}

static size_t hi(int button, bool p2) { return (p2 ? 3 : 0) + std::clamp(button, 1, 3) - 1; }
static bool takeover() { auto& c = cfg(); return c.maintainGravity || c.mirror || c.acOn[0] || c.acOn[1] || c.inverse[0] || c.inverse[1] || c.trajAuto; }

static bool covers(int mode, bool p2) { return mode == 0 || (mode == 1 && !p2) || (mode == 2 && p2); }

static bool noclipFor(PlayerObject* p, GJBaseGameLayer* gl) {
    auto& c = cfg();
    if (!c.noclip) return false;
    if (p == gl->m_player1) return c.noclipP[0];
    if (p == gl->m_player2) return c.noclipP[1];
    return true;
}

static bool wantsPreRelease(PlayerObject* pl) {
    auto& c = cfg();
    if (!pl || !pl->m_touchingRings) return false;
    for (auto ring : CCArrayExt<RingObject*>(pl->m_touchingRings)) {
        if (!ring) continue;
        auto t = ring->m_objectType;
        if (t == GameObjectType::DashRing && c.clickDash) return true;
        if (t == GameObjectType::DropRing) {
            bool falling = (pl->m_yVelocity <= 0 && !pl->m_isUpsideDown) || (pl->m_yVelocity >= 0 && pl->m_isUpsideDown);
            if (c.clickBlack || (c.blackUfo && falling)) return true;
        }
    }
    return false;
}

GJBaseGameLayer* activeLayer() {
    if (auto pl = PlayLayer::get()) return pl;
    if (auto ed = LevelEditorLayer::get(); ed && ed->m_playbackMode == PlaybackMode::Playing) return ed;
    return nullptr;
}
bool isActive(GJBaseGameLayer* gl) { return gl && gl == activeLayer(); }

void cpsPush(bool p2) {
    auto pl = activeLayer();
    if (!pl) return;
    auto& r = rt();
    r.cpsT[p2].push_back(pl->m_gameState.m_levelTime);
    r.maxCps[p2] = std::max(r.maxCps[p2], cpsNow(p2));
}

int cpsNow(int p) {
    auto pl = activeLayer();
    auto& v = rt().cpsT[p];
    if (!pl) return 0;
    double t = pl->m_gameState.m_levelTime;
    v.erase(std::remove_if(v.begin(), v.end(), [&](double x) { return t - x > 1.0 || x > t; }), v.end());
    return (int)v.size();
}

float ssbFactor() {
    auto pl = activeLayer();
    if (!pl || !pl->m_player1) return 1.f;
    static float const speeds[5] = {251.1601f, 311.5801f, 387.4201f, 468.0002f, 576.0002f};
    float s = pl->m_player1->m_playerSpeed;
    int idx = 1;
    if (s == 0.7f) idx = 0; else if (s == 0.9f) idx = 1; else if (s == 1.1f) idx = 2;
    else if (s == 1.3f) idx = 3; else if (s == 1.6f) idx = 4;
    float f = pl->m_player1->getPositionX();
    float a = speeds[idx] / curTps();
    volatile float t = f + a;
    float c = t - f;
    float factor = c / a;
    if (factor <= 0.f || pl->m_levelEndAnimationStarted) factor = 1.f;
    return factor;
}

void computeWant() {
    auto& r = rt(); auto& c = cfg();
    if (!takeover() || r.mode == Mode::Play) { r.want = {r.gameHeld[0], r.gameHeld[1]}; return; }
    bool d[2] = {r.userHeld[0] || r.acHeld[0], r.userHeld[1] || r.acHeld[1]};
    for (int i = 0; i < 2; ++i) if (c.inverse[i]) d[i] = !d[i];
    if (c.mirror) { bool a = d[0] || d[1]; d[0] = a; d[1] = a != c.mirrorInv; }
    if (c.maintainGravity) {
        if (auto pl = activeLayer()) {
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

void pressButton(GJBaseGameLayer* gl, bool down, int p) { press(gl, down, p); }

static void acStep(GJBaseGameLayer* gl, int p) {
    auto& r = rt(); auto& c = cfg(); auto& a = r.ac[p];
    if (!c.acOn[p]) { r.acHeld[p] = false; a = Ac{}; return; }
    uint64_t tick = r.tick;
    if (a.last == tick) return;
    uint64_t nxt = a.last + (a.clicked ? c.acHold[p] : c.acRel[p]);
    if (tick >= nxt || a.last == UINT64_MAX) {
        a.last = tick;
        if (c.acSwift[p]) {
            computeWant();
            press(gl, true, p); press(gl, false, p);
            return;
        }
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
    vis::trajAdjust(gl);
    for (int p = 0; p < 2; ++p) {
        if (r.swiftNow[p]) {
            if (r.gameHeld[p]) press(gl, false, p);
            press(gl, true, p); press(gl, false, p);
        } else if (r.want[p] != r.gameHeld[p]) press(gl, r.want[p], p);
    }
}

static bool alignPlayback() {
    auto& r = rt(); auto& m = r.play;
    float cur = r.startX, rec = m.startX;
    if (std::abs(cur - rec) <= 1.f) return true;
    if (cur < rec) {
        Notification::create("Macro was recorded from a later StartPos", NotificationIcon::Error)->show();
        return false;
    }
    size_t idx = m.fixes.size();
    for (size_t i = 0; i < m.fixes.size(); ++i)
        if (!m.fixes[i].p2 && m.fixes[i].x >= cur - 0.5f) { idx = i; break; }
    if (idx == m.fixes.size()) {
        Notification::create("Cannot align macro to this StartPos (needs frame fixes)", NotificationIcon::Error)->show();
        return false;
    }
    uint32_t t = m.fixes[idx].tick + 1;
    r.tick = t;
    r.nextIn = 0; r.nextFix = 0;
    r.seekHeld.fill(false);
    while (r.nextIn < m.inputs.size() && m.inputs[r.nextIn].tick < t) {
        auto& i = m.inputs[r.nextIn++];
        r.seekHeld[hi(i.button, i.p2)] = i.down;
    }
    while (r.nextFix < m.fixes.size() && m.fixes[r.nextFix].tick < t) r.nextFix++;
    r.seekPending = true;
    return true;
}

static void rewind() {
    auto& r = rt();
    r.tick = 0; r.nextIn = 0; r.nextFix = 0; r.held.fill(false); r.leftover = 0;
    r.gameHeld = {false, false}; r.acHeld = {false, false}; r.ac[0] = Ac{}; r.ac[1] = Ac{};
    r.cpsT[0].clear(); r.cpsT[1].clear(); r.maxCps[0] = r.maxCps[1] = 0;
    r.frozen = false; r.autosaveAcc = 0.0; r.seekPending = false;
    r.ssbActive = false; r.ssbLast = 1.f; r.ssbEvents = 0;
    vis::rotClear();
    auto gl = activeLayer();
    r.startX = (gl && gl->m_player1) ? gl->m_player1->getPositionX() : 0.f;
    r.edWasDead = false; r.edLastX = r.startX;
    if (r.mode == Mode::Record) {
        r.rec = Macro{};
        r.rec.tps = (uint32_t)curTps();
        r.rec.levelId = r.levelId;
        r.rec.startX = r.startX;
        r.rec.seed = GameToolbox::getfast_srand();
    } else if (r.mode == Mode::Play) {
        GameToolbox::fast_srand(r.play.seed);
        if (!alignPlayback()) stopAll(false);
    }
}

static void applyFix(GJBaseGameLayer* pl, Fix const& f) {
    if (!covers(cfg().playPlayers, f.p2)) return;
    auto p = f.p2 ? pl->m_player2 : pl->m_player1;
    if (!p || (f.p2 && !pl->m_gameState.m_isDualMode)) return;
    p->setPosition({f.x, f.y});
    if (!std::isnan(f.rot)) p->setRotation(f.rot);
    if (!std::isnan(f.yv)) p->m_yVelocity = f.yv;
}

static void feed(GJBaseGameLayer* gl) {
    auto& r = rt();
    if (r.mode != Mode::Play) return;
    auto& m = r.play;
    if (r.seekPending) {
        r.seekPending = false;
        r.feeding = true;
        for (size_t i = 0; i < 6; ++i)
            if (r.seekHeld[i] && covers(cfg().playPlayers, i >= 3)) { gl->handleButton(true, (int)(i % 3 + 1), i < 3); r.held[i] = true; }
        r.feeding = false;
    }
    while (r.nextIn < m.inputs.size() && m.inputs[r.nextIn].tick <= r.tick) {
        auto& i = m.inputs[r.nextIn++];
        if (!covers(cfg().playPlayers, i.p2)) continue;
        r.feeding = true;
        gl->handleButton(i.down, i.button, !i.p2);
        r.feeding = false;
        r.held[hi(i.button, i.p2)] = i.down;
        if (i.down && i.button == 1) cpsPush(i.p2);
    }
}

static void takeOver() {
    auto& r = rt();
    if (r.mode != Mode::Play) return;
    Macro m = r.play;
    m.inputs.resize(std::min(r.nextIn, m.inputs.size()));
    m.fixes.resize(std::min(r.nextFix, m.fixes.size()));
    m.ticks = r.tick;
    r.rec = std::move(m);
    r.continueName = r.playName;
    r.mode = Mode::Record;
    r.seekPending = false;
    Notification::create(fmt::format("Recording from tick {}", r.tick), NotificationIcon::Info)->show();
}

bool takeOverNow() {
    if (rt().mode != Mode::Play) return false;
    takeOver();
    return true;
}

static void pushFixes(GJBaseGameLayer* pl, uint32_t tick) {
    auto& r = rt();
    auto add = [&](PlayerObject* p, bool p2) {
        if (p) r.rec.fixes.push_back({tick, p2, p->getPositionX(), p->getPositionY(), p->getRotation(), (float)p->m_yVelocity});
    };
    if (covers(cfg().recPlayers, false)) add(pl->m_player1, false);
    if (pl->m_gameState.m_isDualMode && covers(cfg().recPlayers, true)) add(pl->m_player2, true);
}

static void closeTick(GJBaseGameLayer* pl) {
    auto& r = rt();
    if (cfg().profiler) r.prof.ticks++;
    if (r.mode == Mode::Play) {
        bool useFix = cfg().accuracy > 0;
        while (r.nextFix < r.play.fixes.size() && r.play.fixes[r.nextFix].tick <= r.tick) {
            if (useFix) applyFix(pl, r.play.fixes[r.nextFix]);
            r.nextFix++;
        }
        r.tick++;
        if (r.tick > r.play.ticks + 1 && cfg().continueEnd) {
            takeOver();
        } else if (r.tick > r.play.ticks + 1) {
            bool seq = cfg().seqOn && !cfg().seq.empty();
            stopAll(false);
            if (seq) queueInMainThread([] { seqNext(); });
            else if (cfg().autoExit) queueInMainThread([] {
                if (auto p = PlayLayer::get()) {
                    p->onQuit();
                    CCDirector::sharedDirector()->popSceneWithTransition(0.5f, PopTransition::kPopTransitionFade);
                }
            });
        }
    } else if (r.mode == Mode::Record) {
        if (cfg().accuracy == 2) pushFixes(pl, r.tick);
        r.tick++;
        r.rec.ticks = r.tick;
    } else {
        r.tick++;
    }
    if (cfg().ssbFix || cfg().labels) {
        float f = ssbFactor();
        bool act = std::abs(f - 1.f) > 1e-6f;
        if (act && !r.ssbActive) r.ssbEvents++;
        r.ssbActive = act; r.ssbLast = f;
    }
    if (cfg().trail) vis::sampleTrail(pl);
}

static void editorRespawnCheck(GJBaseGameLayer* gl) {
    auto& r = rt();
    auto p = gl->m_player1;
    if (!p) return;
    float x = p->getPositionX();
    bool dead = p->m_isDead;
    if ((r.edWasDead && !dead) || (r.tick > 4 && x < r.edLastX - 2.f)) rewind();
    r.edWasDead = dead;
    r.edLastX = x;
}

class $modify(XyzGL, GJBaseGameLayer) {
    void handleButton(bool down, int button, bool p1) {
        auto& r = rt(); auto& c = cfg();
        if (!isActive(this) || r.simulating) { GJBaseGameLayer::handleButton(down, button, p1); return; }
        bool ext = !r.internal && !r.feeding && !r.resetting;
        if (ext && c.swapInput) p1 = !p1;
        bool p2 = !p1;
        if (r.mode == Mode::Play && ext && c.takeOver && button >= 1 && button <= 3 && down) takeOver();
        if (r.mode == Mode::Play && !r.feeding && !r.resetting && c.blockInputs && covers(c.playPlayers, p2)) return;
        if (ext && button == 1 && !down && c.autoSwift && r.mode != Mode::Play && !takeover()) return;
        if (ext && button == 1) {
            r.userHeld[p2] = down;
            if (down) cpsPush(p2);
            if (takeover() && r.mode != Mode::Play) return;
        }
        if (r.mode == Mode::Record && !r.resetting && button >= 1 && button <= 3 && covers(c.recPlayers, p2)) {
            if (c.accuracy == 1 && r.tick > 0 && (r.rec.fixes.empty() || r.rec.fixes.back().tick < r.tick - 1)) pushFixes(this, r.tick - 1);
            r.held[hi(button, p2)] = down;
            r.rec.inputs.push_back({r.tick, (uint8_t)button, down, p2});
        }
        bool helper = ext && button == 1 && down && r.mode != Mode::Play;
        if (helper && wantsPreRelease(p1 ? m_player1 : m_player2)) press(this, false, p2);
        if (button == 1) r.gameHeld[p2] = down;
        GJBaseGameLayer::handleButton(down, button, p1);
        if (helper && c.extraClick)
            for (int k = 0; k < std::clamp(c.extraAmt, 1, 10); ++k) { press(this, false, p2); press(this, true, p2); }
        if (helper && c.autoSwift) press(this, false, p2);
    }

    void processCommands(float dt, bool half, bool last) {
        if (!isActive(this) || rt().simulating) {
            GJBaseGameLayer::processCommands(dt, half, last);
            return;
        }
        if (!half) {
            if (!PlayLayer::get()) editorRespawnCheck(this);
            if (rt().mode != Mode::Play) assistTick(this);
            drive(this); feed(this);
        }
        XYZ_TIMED(P_PROC, GJBaseGameLayer::processCommands(dt, half, last));
        if (!half) closeTick(this);
    }
};

class $modify(XyzPL, PlayLayer) {
    struct Snap { uint32_t tick; size_t nextIn, nextFix; std::array<bool, 6> held; uint64_t seed; };
    struct Fields { std::unordered_map<CheckpointObject*, Snap> cps; };

    bool init(GJGameLevel* level, bool a, bool b) {
        if (!PlayLayer::init(level, a, b)) return false;
        rt().levelId = level->m_levelID.value();
        rt().levelName = std::string(level->m_levelName);
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
        flipOnReset();
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
        if (r.simulating) {
            if (o && o == m_anticheatSpike) return;
            r.simDead = true;
            return;
        }
        if (p && !p->m_isDead && o != m_anticheatSpike && noclipFor(p, this)) return;
        PlayLayer::destroyPlayer(p, o);
        if (p) r.flipPlayer = p->m_isSecondPlayer ? 2 : 1;
        if (r.mode == Mode::Play && p && p->m_isDead && !m_isPracticeMode) stopAll(false);
    }

    void levelComplete() {
        if (rt().mode != Mode::Off) stopAll(rt().mode == Mode::Record);
        PlayLayer::levelComplete();
        renderNotifyEnd();
    }

    void onQuit() {
        if (rt().mode != Mode::Off) stopAll(rt().mode == Mode::Record);
        vis::detach();
        vis::rotClear();
        renderStop(true);
        PlayLayer::onQuit();
    }
};

class $modify(XyzEditorLayer, LevelEditorLayer) {
    bool init(GJGameLevel* level, bool p1) {
        if (!LevelEditorLayer::init(level, p1)) return false;
        rt().levelName = std::string(level->m_levelName);
        vis::attach(this);
        return true;
    }

    void destroyPlayer(PlayerObject* p, GameObject* o) {
        auto& r = rt();
        if (r.simulating) { r.simDead = true; return; }
        if (p && noclipFor(p, this)) return;
        LevelEditorLayer::destroyPlayer(p, o);
    }
};

class $modify(XyzEditorUI, EditorUI) {
    void onPlaytest(CCObject* sender) {
        bool starting = m_editorLayer && m_editorLayer->m_playbackMode == PlaybackMode::Not;
        EditorUI::onPlaytest(sender);
        onEditorPlaytest(starting);
    }
};

void onEditorPlaytest(bool started) {
    auto& r = rt();
    if (started) {
        rewind();
        return;
    }
    if (r.mode != Mode::Off) stopAll(r.mode == Mode::Record);
    vis::hideLabels();
}

void seqNext() {
    auto& c = cfg(); auto& r = rt();
    if (c.seq.empty() || !PlayLayer::get()) return;
    if (c.seqRandom && c.seq.size() > 1) {
        int n = r.seqIdx;
        static std::mt19937 rng{std::random_device{}()};
        while (n == r.seqIdx) n = (int)(rng() % c.seq.size());
        r.seqIdx = n;
    } else r.seqIdx = (r.seqIdx + 1) % (int)c.seq.size();
    if (!startPlay(c.seq[r.seqIdx])) Notification::create("Sequence: cannot play next macro", NotificationIcon::Error)->show();
}

bool startRecord() {
    auto& r = rt();
    r.continueName.clear();
    if (auto pl = PlayLayer::get()) {
        r.mode = Mode::Record;
        pl->resetLevelFromStart();
        return true;
    }
    if (LevelEditorLayer::get()) {
        r.mode = Mode::Record;
        r.rec = Macro{};
        Notification::create(activeLayer() ? "Restart the playtest to begin recording" : "Recording armed: start the playtest", NotificationIcon::Info)->show();
        return true;
    }
    return false;
}

bool startPlay(std::string const& name) {
    auto& r = rt();
    auto pl = PlayLayer::get();
    auto ed = LevelEditorLayer::get();
    if ((!pl && !ed) || !loadMacro(r.play, name)) return false;
    if (pl && !cfg().ignoreLevel && r.play.levelId != 0 && r.play.levelId != pl->m_level->m_levelID.value()) return false;
    if (!r.tpsOverridden) { r.prevTpsVal = cfg().tpsVal; r.savedTps = cfg().tps; }
    cfg().tps = true; cfg().tpsVal = (float)r.play.tps;
    r.tpsOverridden = true;
    r.playName = name;
    r.continueName.clear();
    r.mode = Mode::Play;
    if (pl) pl->resetLevelFromStart();
    else Notification::create(activeLayer() ? "Restart the playtest to begin playback" : "Playback armed: start the playtest", NotificationIcon::Info)->show();
    return true;
}

void stopAll(bool save) {
    auto& r = rt();
    if (r.mode == Mode::Off) return;
    if (r.mode == Mode::Play && isRendering()) renderNotifyEnd();
    if (r.mode == Mode::Record && save && !r.rec.inputs.empty()) {
        auto name = r.continueName.empty() ? macroFileName() : r.continueName;
        if (cfg().noOverwrite) name = uniqueMacroName(name);
        saveMacro(r.rec, name);
        Notification::create("Macro saved", NotificationIcon::Success)->show();
    }
    if (r.tpsOverridden) { cfg().tps = r.savedTps; cfg().tpsVal = r.prevTpsVal; r.tpsOverridden = false; }
    r.continueName.clear();
    r.mode = Mode::Off;
    r.held.fill(false);
}
}
