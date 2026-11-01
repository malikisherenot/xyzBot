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
    LD(btnInLevel, bool);
    LD(noDeathEffect, bool); LD(noEffect, bool); LD(hideEndscreen, bool); LD(hideNewBest, bool); LD(optStacked, bool); LD(restartFirst, bool);
    LD(fadeLevel, bool); LD(fadeAudio, bool); LD(clickDash, bool); LD(clickBlack, bool); LD(blackUfo, bool); LD(clickPads, bool); LD(clickGravPads, bool);
    LD(autoSwift, bool); LD(extraClick, bool); LD(extraAmt, int); LD(sFly, bool); LD(sUfo, bool);
    LD(flipDeath, bool); LD(flipP1, bool); LD(flipP2, bool); LD(flipBoth, bool); LD(flipSwift, bool);
    c.fadeLevelIn = (float)m->getSavedValue<double>("fadeLevelIn", c.fadeLevelIn); c.fadeLevelOut = (float)m->getSavedValue<double>("fadeLevelOut", c.fadeLevelOut);
    c.fadeAudioIn = (float)m->getSavedValue<double>("fadeAudioIn", c.fadeAudioIn); c.fadeAudioOut = (float)m->getSavedValue<double>("fadeAudioOut", c.fadeAudioOut);
    for (int i = 0; i < 2; ++i) {
        auto k = std::to_string(i);
        c.sFlyOn[i] = m->getSavedValue<bool>("sFlyOn" + k, true); c.sFlyThr[i] = (float)m->getSavedValue<double>("sFlyThr" + k, 0.0);
        c.sUfoOn[i] = m->getSavedValue<bool>("sUfoOn" + k, true); c.sUfoTarget[i] = (float)m->getSavedValue<double>("sUfoTarget" + k, 135.0);
        c.sUfoThr[i] = (float)m->getSavedValue<double>("sUfoThr" + k, 0.0);
        c.noclipP[i] = m->getSavedValue<bool>("noclipP" + k, true); c.acSwift[i] = m->getSavedValue<bool>("acSwift" + k, false);
    }
    for (int i = 0; i < 3; ++i) {
        auto k = std::to_string(i);
        c.layoutBg[i] = m->getSavedValue<int>("layoutBg" + k, 160); c.layoutGr[i] = m->getSavedValue<int>("layoutGr" + k, 160);
    } LD(stepBar, bool); LD(stepN, int); LD(holdRate, int); LD(menuFont, int); LD(themeIdx, int); LD(accR, int); LD(accG, int); LD(accB, int); LD(tintR, int); LD(tintG, int); LD(tintB, int);
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
    SV(btnInLevel, bool);
    SV(noDeathEffect, bool); SV(noEffect, bool); SV(hideEndscreen, bool); SV(hideNewBest, bool); SV(optStacked, bool); SV(restartFirst, bool);
    SV(fadeLevel, bool); SV(fadeAudio, bool); SV(clickDash, bool); SV(clickBlack, bool); SV(blackUfo, bool); SV(clickPads, bool); SV(clickGravPads, bool);
    SV(autoSwift, bool); SV(extraClick, bool); SV(extraAmt, int); SV(sFly, bool); SV(sUfo, bool);
    SV(flipDeath, bool); SV(flipP1, bool); SV(flipP2, bool); SV(flipBoth, bool); SV(flipSwift, bool);
    m->setSavedValue<double>("fadeLevelIn", c.fadeLevelIn); m->setSavedValue<double>("fadeLevelOut", c.fadeLevelOut);
    m->setSavedValue<double>("fadeAudioIn", c.fadeAudioIn); m->setSavedValue<double>("fadeAudioOut", c.fadeAudioOut);
    for (int i = 0; i < 2; ++i) {
        auto k = std::to_string(i);
        m->setSavedValue<bool>("sFlyOn" + k, c.sFlyOn[i]); m->setSavedValue<double>("sFlyThr" + k, c.sFlyThr[i]);
        m->setSavedValue<bool>("sUfoOn" + k, c.sUfoOn[i]); m->setSavedValue<double>("sUfoTarget" + k, c.sUfoTarget[i]);
        m->setSavedValue<double>("sUfoThr" + k, c.sUfoThr[i]);
        m->setSavedValue<bool>("noclipP" + k, c.noclipP[i]); m->setSavedValue<bool>("acSwift" + k, c.acSwift[i]);
    }
    for (int i = 0; i < 3; ++i) {
        auto k = std::to_string(i);
        m->setSavedValue<int>("layoutBg" + k, c.layoutBg[i]); m->setSavedValue<int>("layoutGr" + k, c.layoutGr[i]);
    } SV(stepBar, bool); SV(stepN, int); SV(holdRate, int); SV(menuFont, int); SV(themeIdx, int); SV(accR, int); SV(accG, int); SV(accB, int); SV(tintR, int); SV(tintG, int); SV(tintB, int);
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
