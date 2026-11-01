#include "State.hpp"
#include "../vis/Vis.hpp"
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/CCScheduler.hpp>
#include <Geode/binding/FMODAudioEngine.hpp>
#include <chrono>

using namespace geode::prelude;

namespace xyz {
void stepOnce() { rt().stepPending = std::min(rt().stepPending + 1, 2000); }
static bool pdActive(GJBaseGameLayer* pl) {
    auto& c = cfg();
    return c.preventDeath && !c.noclip && rt().mode != Mode::Play && pl->m_player1 && !pl->m_player1->m_isDead;
}
}

class $modify(XyzTpsGL, GJBaseGameLayer) {
    bool tickOnce(GJBaseGameLayer* pl, double step) {
        using namespace xyz;
        auto& r = rt(); auto& c = cfg();
        if (pdActive(pl)) {
            computeWant();
            if (r.frozen) {
                if (r.want != r.frozenWant) r.frozen = false;
                else if (r.stepPending > 0) { r.stepPending--; r.frozen = false; GJBaseGameLayer::update((float)step); return true; }
                else return false;
            }
            if (vis::predictDeath(pl, std::clamp(c.pdLook, 1, 10))) {
                r.frozen = true; r.frozenWant = r.want;
                return false;
            }
        } else r.frozen = false;
        GJBaseGameLayer::update((float)step);
        return true;
    }

    void update(float dt) {
        using namespace xyz;
        auto& c = cfg(); auto& r = rt();
        auto pl = xyz::activeLayer();
        bool mine = pl && (GJBaseGameLayer*)pl == this && !r.simulating;
        if (!mine) { GJBaseGameLayer::update(dt); return; }

        double tps = curTps();
        double step = 1.0 / tps;

        if (c.stepper) {
            if (r.stepPending <= 0) return;
            int n = std::min(r.stepPending, 600);
            r.stepPending -= n;
            for (int i = 0; i < n; ++i) {
                r.subTick = i + 1 < n;
                GJBaseGameLayer::update((float)step);
            }
            r.subTick = false;
            vis::refresh(pl);
            return;
        }

        if (r.mode == Mode::Record && c.autosave) {
            r.autosaveAcc += dt;
            if (r.autosaveAcc >= std::max(10, c.autosaveSec)) {
                r.autosaveAcc = 0;
                if (!r.rec.inputs.empty()) saveMacro(r.rec, macroFileName() + " (autosave)");
            }
        }

        if ((tps == 240.0 && !pdActive(pl)) || m_resumeTimer > 0) {
            GJBaseGameLayer::update(dt);
            vis::refresh(pl);
            return;
        }

        double acc = r.leftover + dt;
        int64_t n = (int64_t)(acc / step);
        int64_t done = 0;
        bool blocked = false;
        auto t0 = std::chrono::steady_clock::now();
        double budget = std::max(1.0, (double)c.budgetMs);
        while (done < n) {
            r.subTick = done + 1 < n;
            if (!this->tickOnce(pl, step)) { blocked = true; break; }
            ++done;
            if ((done & 15) == 0 && std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count() > budget) break;
        }
        r.subTick = false;
        r.leftover = (blocked || done < n) ? 0.0 : acc - step * (double)done;
        vis::refresh(pl);
    }

    double getModifiedDelta(float dt) {
        using namespace xyz;
        auto pl = xyz::activeLayer();
        double tps = curTps();
        if ((tps == 240.0 && !(pl && pdActive(pl))) || !pl || (GJBaseGameLayer*)pl != this || rt().simulating)
            return GJBaseGameLayer::getModifiedDelta(dt);
        double step = 1.0 / tps;
        double warp = std::min(1.0, (double)m_gameState.m_timeWarp);
        if (m_resumeTimer > 0) { m_resumeTimer--; dt = 0.f; }
        double total = dt + m_extraDelta;
        double q = std::round(total / (warp * step)) * warp * step;
        m_extraDelta = total - q;
        return q;
    }
};

class $modify(XyzSched, CCScheduler) {
    void update(float dt) {
        auto& c = xyz::cfg();
        CCScheduler::update(c.speed ? dt * c.speedVal : dt);
        static float lastPitch = 1.f;
        float want = (c.speed && c.speedAudio) ? c.speedVal : 1.f;
        if (c.ssbFix && xyz::rt().ssbActive && xyz::activeLayer()) {
            float f = std::clamp(xyz::rt().ssbLast, 0.25f, 4.f);
            want *= c.ssbInvert ? 1.f / f : f;
        }
        if (std::abs(want - lastPitch) > 1e-7f) {
            lastPitch = want;
            if (auto e = FMODAudioEngine::get(); e && e->m_backgroundMusicChannel) e->m_backgroundMusicChannel->setPitch(want);
        }
    }
};
