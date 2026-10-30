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

template <class T> static void w(std::ofstream& f, T v) { f.write(reinterpret_cast<char const*>(&v), sizeof(T)); }
template <class T> static bool r(std::ifstream& f, T& v) { return (bool)f.read(reinterpret_cast<char*>(&v), sizeof(T)); }

bool saveMacro(Macro const& m, std::string const& name) {
    std::ofstream f(macroDir() / (name + ".xyz"), std::ios::binary);
    if (!f) return false;
    f.write("XYZM", 4); w<uint32_t>(f, 1);
    w(f, m.tps); w(f, m.ticks); w(f, m.seed); w(f, m.levelId);
    w<uint32_t>(f, m.inputs.size());
    for (auto& i : m.inputs) { w(f, i.tick); w(f, i.button); w<uint8_t>(f, i.down | (i.p2 << 1)); }
    w<uint32_t>(f, m.fixes.size());
    for (auto& x : m.fixes) { w(f, x.tick); w<uint8_t>(f, x.p2); w(f, x.x); w(f, x.y); w(f, x.rot); w(f, x.yv); }
    return (bool)f;
}

bool loadMacro(Macro& m, std::string const& name) {
    std::ifstream f(macroDir() / (name + ".xyz"), std::ios::binary);
    char magic[4]; uint32_t ver = 0, n = 0;
    if (!f || !f.read(magic, 4) || std::string(magic, 4) != "XYZM" || !r(f, ver) || ver != 1) return false;
    m = Macro{};
    if (!r(f, m.tps) || !r(f, m.ticks) || !r(f, m.seed) || !r(f, m.levelId) || !r(f, n)) return false;
    m.inputs.resize(n);
    for (auto& i : m.inputs) { uint8_t fl; if (!r(f, i.tick) || !r(f, i.button) || !r(f, fl)) return false; i.down = fl & 1; i.p2 = fl & 2; }
    if (!r(f, n)) return false;
    m.fixes.resize(n);
    for (auto& x : m.fixes) { uint8_t p; if (!r(f, x.tick) || !r(f, p) || !r(f, x.x) || !r(f, x.y) || !r(f, x.rot) || !r(f, x.yv)) return false; x.p2 = p; }
    return true;
}
}
