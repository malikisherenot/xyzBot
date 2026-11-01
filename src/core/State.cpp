#include "State.hpp"
#include <fstream>

using namespace geode::prelude;

namespace xyz {
Cfg& cfg() { static Cfg c; return c; }
Rt& rt() { static Rt r; return r; }
float curTps() { return cfg().tps ? std::max(1.f, cfg().tpsVal) : 240.f; }

#define LD(f, T) c.f = m->getSavedValue<T>(#f, c.f)
#define SV(f, T) m->setSavedValue<T>(#f, c.f)

void loadCfg() {
    auto& c = cfg(); auto m = Mod::get();
    LD(tps, bool); LD(tpsVal, double); LD(budgetMs, double); LD(speed, bool); LD(speedVal, double);
    LD(noclip, bool); LD(fixes, bool); LD(ignoreLevel, bool); LD(hitboxes, bool);
    LD(hbPlayer, bool); LD(hbSolid, bool); LD(hbHazard, bool); LD(hbSpecial, bool); LD(hbRotated, bool);
    LD(trail, bool); LD(trailLen, int); LD(traj, bool); LD(trajLen, int);
    LD(blockInputs, bool); LD(maintainGravity, bool); LD(mirror, bool); LD(mirrorInv, bool); LD(noMirror, bool);
    LD(preventDeath, bool); LD(pdLook, int); LD(layout, bool); LD(speedAudio, bool); LD(autoExit, bool);
    LD(autosave, bool); LD(autosaveSec, int); LD(labels, bool);
    LD(ssbFix, bool); LD(ssbInvert, bool); LD(legacyShip, bool); LD(legacyShipPct, int);
    LD(btnInLevel, bool); LD(stepBar, bool); LD(stepN, int); LD(holdRate, int); LD(menuFont, int); LD(themeIdx, int); LD(accR, int); LD(accG, int); LD(accB, int); LD(tintR, int); LD(tintG, int); LD(tintB, int);
    for (int i = 0; i < 2; ++i) {
        auto k = std::to_string(i);
        c.acOn[i] = m->getSavedValue<bool>("acOn" + k, false); c.acHold[i] = m->getSavedValue<int>("acHold" + k, 4);
        c.acRel[i] = m->getSavedValue<int>("acRel" + k, 4); c.acClicks[i] = m->getSavedValue<int>("acClicks" + k, 1);
    }
}
void saveCfg() {
    auto& c = cfg(); auto m = Mod::get();
    SV(tps, bool); m->setSavedValue<double>("tpsVal", c.tpsVal); m->setSavedValue<double>("budgetMs", c.budgetMs);
    SV(speed, bool); m->setSavedValue<double>("speedVal", c.speedVal);
    SV(noclip, bool); SV(fixes, bool); SV(ignoreLevel, bool); SV(hitboxes, bool);
    SV(hbPlayer, bool); SV(hbSolid, bool); SV(hbHazard, bool); SV(hbSpecial, bool); SV(hbRotated, bool);
    SV(trail, bool); SV(trailLen, int); SV(traj, bool); SV(trajLen, int);
    SV(blockInputs, bool); SV(maintainGravity, bool); SV(mirror, bool); SV(mirrorInv, bool); SV(noMirror, bool);
    SV(preventDeath, bool); SV(pdLook, int); SV(layout, bool); SV(speedAudio, bool); SV(autoExit, bool);
    SV(autosave, bool); SV(autosaveSec, int); SV(labels, bool);
    SV(ssbFix, bool); SV(ssbInvert, bool); SV(legacyShip, bool); SV(legacyShipPct, int);
    SV(btnInLevel, bool); SV(stepBar, bool); SV(stepN, int); SV(holdRate, int); SV(menuFont, int); SV(themeIdx, int); SV(accR, int); SV(accG, int); SV(accB, int); SV(tintR, int); SV(tintG, int); SV(tintB, int);
    for (int i = 0; i < 2; ++i) {
        auto k = std::to_string(i);
        m->setSavedValue<bool>("acOn" + k, c.acOn[i]); m->setSavedValue<int>("acHold" + k, c.acHold[i]);
        m->setSavedValue<int>("acRel" + k, c.acRel[i]); m->setSavedValue<int>("acClicks" + k, c.acClicks[i]);
    }
}

std::filesystem::path macroDir() {
    auto p = Mod::get()->getSaveDir() / "macros";
    std::error_code ec; std::filesystem::create_directories(p, ec);
    return p;
}

std::vector<std::string> listMacros() {
    std::vector<std::string> out;
    std::error_code ec;
    for (auto& e : std::filesystem::directory_iterator(macroDir(), ec))
        if (e.path().extension() == ".xyz") out.push_back(e.path().stem().string());
    std::sort(out.begin(), out.end());
    return out;
}

}
