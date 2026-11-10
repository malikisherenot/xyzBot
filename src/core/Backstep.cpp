#include "State.hpp"
#include <Geode/binding/CheckpointObject.hpp>
#include <Geode/binding/GameToolbox.hpp>
#include <Geode/binding/GJEffectManager.hpp>
#include <Geode/binding/EnhancedGameObject.hpp>
#include <Geode/binding/HardStreak.hpp>
#include <algorithm>
#include <deque>
#include <vector>

using namespace geode::prelude;

namespace xyz {
namespace {
struct Used { EnhancedGameObject* o; bool p1, p2; };
struct Save {
    CheckpointObject* cp = nullptr;
    uint32_t tick = 0;
    size_t nextIn = 0, nextFix = 0;
    std::array<bool, 6> held, uheld;
    uint64_t seed = 0;
    gd::unordered_map<int, int> items;
    gd::vector<GameObject*> calc;
    int calcSize = 0;
    std::vector<Used> used;
};
struct In { uint32_t tick; uint8_t button; bool down, p1; };

constexpr size_t RING = 400;
std::deque<Save> s_ring;
std::vector<In> s_log;
std::array<bool, 6> s_uheld{};
uint32_t s_expect = 0, s_from = 0, s_target = 0;

size_t hi(int button, bool p2) { return (p2 ? 3 : 0) + std::clamp(button, 1, 3) - 1; }

void drop(Save& s) { if (s.cp) s.cp->release(); s.cp = nullptr; }

// Echo (backstep.cpp): objects whose "used" flags are not part of a GD checkpoint
std::vector<Used> usedObjects(PlayLayer* play) {
    std::vector<Used> out;
    auto* p1 = play->m_player1;
    if (!p1) return out;
    auto& grid = play->m_nonEffectObjects;
    auto& sizes = play->m_nonEffectObjectsSizes;
    float factor = play->m_sectionXFactor > 0.f ? play->m_sectionXFactor : 0.01f;
    int center = (int)std::floor(p1->getPositionX() * factor);
    int last = (int)std::min(grid.size(), sizes.size()) - 1;
    for (int x = std::max(center - 10, 0); x <= std::min(center + 10, last); ++x) {
        auto* column = grid[x];
        auto* counts = sizes[x];
        if (!column || !counts) continue;
        for (size_t y = 0; y < std::min(column->size(), counts->size()); ++y) {
            auto* bucket = (*column)[y];
            if (!bucket) continue;
            int amt = std::min<int>((*counts)[y], (int)bucket->size());
            for (int i = 0; i < amt; ++i) {
                auto* obj = (*bucket)[i];
                if (!obj) continue;
                switch (obj->m_objectType) {
                case GameObjectType::Solid:
                case GameObjectType::Hazard:
                case GameObjectType::AnimatedHazard:
                case GameObjectType::Slope:
                case GameObjectType::Decoration:
                    continue;
                default: break;
                }
                if (auto* e = typeinfo_cast<EnhancedGameObject*>(obj)) out.push_back({e, e->m_activatedByPlayer1, e->m_activatedByPlayer2});
            }
        }
    }
    return out;
}
}

void backstepClear() {
    for (auto& s : s_ring) drop(s);
    s_ring.clear();
    s_log.clear();
    s_uheld.fill(false);
    s_expect = 0;
}

void backstepLog(uint32_t tick, int button, bool down, bool p1) {
    if (button < 1 || button > 3) return;
    s_log.push_back({tick, (uint8_t)button, down, p1});
    bool q1 = cfg().swapInput ? !p1 : p1;
    s_uheld[hi(button, !q1)] = down;
    if (s_log.size() > 50000) s_log.erase(s_log.begin(), s_log.begin() + 10000);
}

void backstepSave(GJBaseGameLayer* gl, int batch) {
    auto& r = rt();
    auto pl = typeinfo_cast<PlayLayer*>(gl);
    if (!pl || r.simulating || r.bsReplay || !pl->m_checkpointArray || !pl->m_player1 || pl->m_player1->m_isDead) return;
    if (r.tick != s_expect) {
        auto keepLog = s_log;
        backstepClear();
        s_log = std::move(keepLog);
    }
    s_expect = r.tick + 1;
    uint32_t iv = batch >= 4 ? 8 : 1;
    if (!s_ring.empty()) {
        if (r.tick < s_ring.back().tick) { backstepClear(); s_expect = r.tick + 1; }
        else if (r.tick - s_ring.back().tick < iv) return;
    }
    r.bsSaving = true;
    auto cp = pl->createCheckpoint();
    r.bsSaving = false;
    if (!cp) return;
    cp->retain();
    Save s;
    s.cp = cp; s.tick = r.tick; s.nextIn = r.nextIn; s.nextFix = r.nextFix;
    s.held = r.held; s.uheld = s_uheld; s.seed = GameToolbox::getfast_srand();
    s.calcSize = 0;
    if (pl->m_effectManager) s.items = pl->m_effectManager->m_persistentItemCountMap;
    s.calc = pl->m_calcNonEffectObjects;
    s.calcSize = pl->m_calcNonEffectObjectsSize;
    s.used = usedObjects(pl);
    s_ring.push_back(std::move(s));
    while (s_ring.size() > RING) {
        drop(s_ring.front());
        s_ring.pop_front();
    }
    uint32_t first = s_ring.front().tick;
    s_log.erase(std::remove_if(s_log.begin(), s_log.end(), [&](In const& i) { return i.tick < first; }), s_log.end());
}

bool backstepLoad(PlayLayer* pl, int n, uint32_t& k) {
    auto& r = rt();
    k = 0;
    auto fail = [](char const* m) { Notification::create(m, NotificationIcon::Error)->show(); return false; };
    if (!pl->m_checkpointArray || pl->m_hasCompletedLevel || pl->m_levelEndAnimationStarted) return fail("Can't step back here");
    uint32_t target = r.tick > (uint32_t)n ? r.tick - (uint32_t)n : 0;
    while (!s_ring.empty() && s_ring.back().tick > target) { drop(s_ring.back()); s_ring.pop_back(); }
    if (s_ring.empty()) return fail("Nothing saved to step back to");
    Save s = s_ring.back();
    r.bsLoad = true;
    pl->m_checkpointArray->addObject(s.cp);
    pl->resetLevel();
    pl->m_checkpointArray->removeObject(s.cp, true);
    r.bsLoad = false;
    if (s_ring.empty()) return fail("Step back failed");

    if (pl->m_effectManager) pl->m_effectManager->m_persistentItemCountMap = s.items;
    pl->m_calcNonEffectObjects = s.calc;
    pl->m_calcNonEffectObjectsSize = s.calcSize;
    for (auto& u : s.used) { u.o->m_activatedByPlayer1 = u.p1; u.o->m_activatedByPlayer2 = u.p2; }

    GameToolbox::fast_srand(s.seed);
    r.tick = s.tick; r.nextIn = s.nextIn; r.nextFix = s.nextFix; r.held = s.held;
    if (r.mode == Mode::Record) {
        auto& in = r.rec.inputs; auto& fx = r.rec.fixes;
        in.erase(std::remove_if(in.begin(), in.end(), [&](auto& i) { return i.tick > s.tick; }), in.end());
        fx.erase(std::remove_if(fx.begin(), fx.end(), [&](auto& f) { return f.tick >= s.tick; }), fx.end());
        r.rec.ticks = s.tick;
    }
    pl->m_queuedButtons.clear();
    r.feeding = true;
    for (size_t i = 0; i < 6; ++i) {
        bool want = s.held[i] || s.uheld[i];
        if (i >= 3 && !want && !pl->m_gameState.m_isDualMode) continue;
        pl->handleButton(false, (int)(i % 3 + 1), i < 3);
        if (want) pl->handleButton(true, (int)(i % 3 + 1), i < 3);
    }
    r.feeding = false;
    s_uheld = s.uheld;
    s_from = s.tick;
    s_target = target;
    k = target - s.tick;
    return true;
}

void backstepFeed(GJBaseGameLayer* gl, uint32_t tick) {
    if (tick <= s_from) return;
    auto log = s_log;
    for (auto& e : log)
        if (e.tick == tick) gl->handleButton(e.down, e.button, e.p1);
}

void backstepFinish(PlayLayer* pl) {
    auto& r = rt();
    s_log.erase(std::remove_if(s_log.begin(), s_log.end(), [&](In const& i) { return i.tick > s_target; }), s_log.end());
    for (auto* b : {pl->m_player1, pl->m_player2})
        if (b && b->m_waveTrail) b->m_waveTrail->reset();
    s_expect = r.tick;
    backstepSave(pl, 1);
}
}
