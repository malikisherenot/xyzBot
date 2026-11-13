#include "State.hpp"
#include <fstream>

using namespace geode::prelude;

namespace xyz {
Cfg& cfg() { static Cfg c; return c; }
Rt& rt() { static Rt r; return r; }
float curTps() { return cfg().tps ? std::max(1.f, cfg().tpsVal) : 240.f; }

void profFrame(double loopUs, double gapUs) {
    auto& p = rt().prof;
    double now = nowUs();
    if (p.lastFrame > 0 && now - p.lastFrame > 2e6) { p = Prof{}; }
    p.lastFrame = now;
    if (p.winStart == 0) p.winStart = now;
    p.acc[P_LOOP] += loopUs; p.gapUs += gapUs; p.frames++;
    if (now - p.winStart < 1e6) return;
    double secs = (now - p.winStart) / 1e6, t = (double)p.ticks;
    auto& o = p.out;
    o.valid = p.ticks > 0 && p.frames > 0;
    if (o.valid) {
        o.ticksPerSec = t / secs; o.framesPerSec = (double)p.frames / secs; o.ticksPerFrame = t / (double)p.frames;
        o.frameMs = p.gapUs / (double)p.frames / 1000.0;
        o.loopMs = p.acc[P_LOOP] / (double)p.frames / 1000.0;
        o.drawMs = std::max(0.0, o.frameMs - o.loopMs);
        o.upd = p.acc[P_UPD] / t; o.player = p.acc[P_PLAYER] / t; o.coll = p.acc[P_COLL] / t;
        o.cam = p.acc[P_CAM] / t; o.vis = p.acc[P_VIS] / t; o.post = p.acc[P_POST] / t; o.proc = p.acc[P_PROC] / t;
        o.fx = p.acc[P_FX] / t; o.enter = p.acc[P_ENTER] / t;
        o.other = std::max(0.0, o.upd - o.player - o.coll - o.cam - o.vis - o.proc - o.fx - o.enter);
        double n = std::max(1.0, o.ticksPerFrame), save = 1.0 - 1.0 / n, loopUs = o.loopMs * 1000.0;
        auto gain = [&](double perCall) {
            double saved = perCall * save * n;
            return loopUs - saved > 1.0 ? loopUs / (loopUs - saved) : 1.0;
        };
        o.gainLo = gain(o.cam + o.vis);
        o.gainMid = gain(o.cam + o.vis + o.fx + o.enter);
        o.gainHi = gain(o.cam + o.vis + o.fx + o.enter + o.other);
        o.capNow = loopUs > 1.0 ? n / loopUs * 1e6 : 0.0;
    }
    for (auto& a : p.acc) a = 0;
    p.ticks = 0; p.frames = 0; p.gapUs = 0; p.winStart = now;
}

#define LD(f, T) c.f = m->getSavedValue<T>(#f, c.f)
#define SV(f, T) m->setSavedValue<T>(#f, c.f)

void loadCfg() {
    auto& c = cfg(); auto m = Mod::get();
    { int a = m->getSavedValue<int>("accuracy", -1); c.accuracy = a >= 0 ? std::min(a, 2) : (m->getSavedValue<bool>("fixes", true) ? 2 : 0); }
    LD(tps, bool); LD(tpsVal, double); LD(budgetMs, double); LD(speed, bool); LD(speedVal, double);
    LD(noclip, bool); LD(trajAuto, bool); LD(profiler, bool); LD(trajLook, int); LD(fastSub, bool); LD(skipVis, bool); LD(ignoreLevel, bool); LD(hitboxes, bool);
    LD(hbPlayer, bool); LD(hbSolid, bool); LD(hbHazard, bool); LD(hbSpecial, bool); LD(hbRotated, bool);
    LD(trail, bool); LD(trailLen, int); LD(traj, bool); LD(trajLen, int);
    LD(blockInputs, bool); LD(maintainGravity, bool); LD(mirror, bool); LD(mirrorInv, bool); LD(noMirror, bool);
    LD(preventDeath, bool); LD(pdLook, int); LD(layout, bool); LD(speedAudio, bool); LD(autoExit, bool);
    LD(autosave, bool); LD(autosaveSec, int); LD(labels, bool);
    LD(ssbFix, bool); LD(ssbInvert, bool); LD(legacyShip, bool); LD(legacyShipPct, int);
    LD(btnInLevel, bool);
    LD(fixRotation, bool); LD(waveDragFix, bool); LD(renderW, int); LD(renderFps, int); LD(renderBitrate, int); LD(renderCodec, int); LD(renderTail, int); LD(renderAudio, bool);
    c.renderMacro = m->getSavedValue<std::string>("renderMacro", ""); LD(takeOver, bool); LD(continueEnd, bool); LD(autoName, bool); LD(noOverwrite, bool); LD(renameFmt, int); LD(recPlayers, int); LD(playPlayers, int);
    LD(swapInput, bool); LD(seqOn, bool); LD(seqRandom, bool);
    c.inverse[0] = m->getSavedValue<bool>("inverse0", false); c.inverse[1] = m->getSavedValue<bool>("inverse1", false);
    {
        c.seq.clear();
        std::string all = m->getSavedValue<std::string>("seq", "");
        size_t pos = 0;
        while (pos <= all.size() && !all.empty()) {
            auto nl = all.find('\n', pos);
            auto item = all.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos);
            if (!item.empty()) c.seq.push_back(item);
            if (nl == std::string::npos) break;
            pos = nl + 1;
        }
    }
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
    } LD(stepBar, bool); LD(stepN, int); LD(holdRate, int); LD(uiStyle, int); LD(fontGlass, int); LD(fontPaper, int); LD(themeIdx, int); LD(accR, int); LD(accG, int); LD(accB, int); LD(tintR, int); LD(tintG, int); LD(tintB, int);
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
    SV(noclip, bool); SV(accuracy, int); SV(trajAuto, bool); SV(profiler, bool); SV(trajLook, int); SV(fastSub, bool); SV(skipVis, bool); SV(ignoreLevel, bool); SV(hitboxes, bool);
    SV(hbPlayer, bool); SV(hbSolid, bool); SV(hbHazard, bool); SV(hbSpecial, bool); SV(hbRotated, bool);
    SV(trail, bool); SV(trailLen, int); SV(traj, bool); SV(trajLen, int);
    SV(blockInputs, bool); SV(maintainGravity, bool); SV(mirror, bool); SV(mirrorInv, bool); SV(noMirror, bool);
    SV(preventDeath, bool); SV(pdLook, int); SV(layout, bool); SV(speedAudio, bool); SV(autoExit, bool);
    SV(autosave, bool); SV(autosaveSec, int); SV(labels, bool);
    SV(ssbFix, bool); SV(ssbInvert, bool); SV(legacyShip, bool); SV(legacyShipPct, int);
    SV(btnInLevel, bool);
    SV(fixRotation, bool); SV(waveDragFix, bool); SV(renderW, int); SV(renderFps, int); SV(renderBitrate, int); SV(renderCodec, int); SV(renderTail, int); SV(renderAudio, bool);
    m->setSavedValue<std::string>("renderMacro", c.renderMacro); SV(takeOver, bool); SV(continueEnd, bool); SV(autoName, bool); SV(noOverwrite, bool); SV(renameFmt, int); SV(recPlayers, int); SV(playPlayers, int);
    SV(swapInput, bool); SV(seqOn, bool); SV(seqRandom, bool);
    m->setSavedValue<bool>("inverse0", c.inverse[0]); m->setSavedValue<bool>("inverse1", c.inverse[1]);
    {
        std::string all;
        for (auto& n : c.seq) { if (!all.empty()) all += '\n'; all += n; }
        m->setSavedValue<std::string>("seq", all);
    }
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
    } SV(stepBar, bool); SV(stepN, int); SV(holdRate, int); SV(uiStyle, int); SV(fontGlass, int); SV(fontPaper, int); SV(themeIdx, int); SV(accR, int); SV(accG, int); SV(accB, int); SV(tintR, int); SV(tintG, int); SV(tintB, int);
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

std::string uniqueMacroName(std::string const& base) {
    static char const* FMT[] = {"_#", " #", " (#)", "-#"};
    std::string f = FMT[std::clamp(cfg().renameFmt, 0, 3)];
    auto exists = [](std::string const& n) { std::error_code ec; return std::filesystem::exists(macroDir() / (n + ".xyz"), ec); };
    if (!exists(base)) return base;
    for (int n = 1; n < 10000; ++n) {
        std::string suffix = f;
        suffix.replace(suffix.find('#'), 1, std::to_string(n));
        if (!exists(base + suffix)) return base + suffix;
    }
    return base;
}

bool mergeMacros(std::string const& a, std::string const& b, std::string& outName) {
    Macro ma, mb;
    if (!loadMacro(ma, a) || !loadMacro(mb, b) || ma.tps != mb.tps) return false;
    ma.inputs.insert(ma.inputs.end(), mb.inputs.begin(), mb.inputs.end());
    std::stable_sort(ma.inputs.begin(), ma.inputs.end(), [](auto& x, auto& y) { return x.tick < y.tick; });
    for (auto& f : mb.fixes) {
        bool dup = false;
        for (auto& g : ma.fixes) if (g.tick == f.tick && g.p2 == f.p2) { dup = true; break; }
        if (!dup) ma.fixes.push_back(f);
    }
    std::stable_sort(ma.fixes.begin(), ma.fixes.end(), [](auto& x, auto& y) { return x.tick < y.tick; });
    ma.ticks = std::max(ma.ticks, mb.ticks);
    outName = uniqueMacroName(a + " + " + b);
    return saveMacro(ma, outName);
}
}
