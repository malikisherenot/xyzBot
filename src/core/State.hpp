#pragma once
#include <Geode/Geode.hpp>
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace xyz {
enum class Mode : uint8_t { Off, Record, Play };

struct Input { uint32_t tick; uint8_t button; bool down; bool p2; };
struct Fix { uint32_t tick; bool p2; float x, y, rot, yv; };
struct Macro {
    std::vector<Input> inputs;
    std::vector<Fix> fixes;
    uint32_t tps = 240;
    uint32_t ticks = 0;
    uint64_t seed = 0;
    int64_t levelId = 0;
    float startX = 0.f;
};

struct Cfg {
    bool tps = false;
    float tpsVal = 240.f;
    float budgetMs = 14.f;
    bool speed = false;
    float speedVal = 1.f;
    bool stepper = false;
    bool noclip = false;
    bool fixes = true;
    bool ignoreLevel = false;
    bool hitboxes = false;
    bool hbPlayer = true, hbSolid = true, hbHazard = true, hbSpecial = true, hbRotated = true;
    bool trail = false;
    int trailLen = 200;
    bool traj = false;
    int trajLen = 240;
    bool blockInputs = true;
    bool maintainGravity = false;
    bool mirror = false, mirrorInv = false, noMirror = false;
    bool preventDeath = false;
    int pdLook = 2;
    bool layout = false;
    bool speedAudio = true;
    bool autoExit = false;
    bool autosave = true;
    int autosaveSec = 180;
    bool labels = true;
    bool ssbFix = false, ssbInvert = false;
    bool legacyShip = false;
    int legacyShipPct = 6;
    bool btnInLevel = true;
    // ported from Scarlet Utils (own implementation)
    bool noDeathEffect = false, noEffect = false, hideEndscreen = false, hideNewBest = false;
    bool optStacked = false, restartFirst = false;
    bool fadeLevel = false, fadeAudio = false;
    float fadeLevelIn = 0.5f, fadeLevelOut = 1.0f, fadeAudioIn = 0.5f, fadeAudioOut = 1.0f;
    bool clickDash = false, clickBlack = false, blackUfo = false, clickPads = false, clickGravPads = false;
    bool autoSwift = false, extraClick = false;
    int extraAmt = 1;
    bool sFly = false, sFlyOn[2] = {true, true};
    float sFlyThr[2] = {0.f, 0.f};
    bool sUfo = false, sUfoOn[2] = {true, true};
    float sUfoTarget[2] = {135.f, 135.f}, sUfoThr[2] = {0.f, 0.f};
    bool flipDeath = false, flipP1 = true, flipP2 = true, flipBoth = false, flipSwift = false;
    bool noclipP[2] = {true, true};
    bool acSwift[2] = {false, false};
    int layoutBg[3] = {160, 160, 160}, layoutGr[3] = {160, 160, 160};
    bool stepBar = true;
    int stepN = 10;
    int holdRate = 1;
    int menuFont = 1;
    int themeIdx = 0;
    int accR = 0, accG = 217, accB = 153;
    int tintR = 13, tintG = 20, tintB = 28;
    bool acOn[2] = {false, false};
    int acHold[2] = {4, 4}, acRel[2] = {4, 4}, acClicks[2] = {1, 1};
};

struct Ac { uint64_t last = UINT64_MAX; bool clicked = false; };

struct Rt {
    Mode mode = Mode::Off;
    uint32_t tick = 0;
    size_t nextIn = 0, nextFix = 0;
    Macro rec, play;
    std::array<bool, 6> held{};
    bool feeding = false, resetting = false, simulating = false, simDead = false;
    bool subTick = false;
    bool loadedCp = false;
    int stepPending = 0;
    double leftover = 0.0;
    bool savedTps = false;
    float prevTpsVal = 240.f;
    int64_t levelId = 0;
    bool internal = false;
    std::array<bool, 2> userHeld{}, gameHeld{}, want{}, acHeld{}, frozenWant{};
    bool frozen = false;
    Ac ac[2];
    std::vector<double> cpsT[2];
    int maxCps[2] = {0, 0};
    float startX = 0.f;
    std::string levelName;
    bool edWasDead = false;
    float edLastX = 0.f;
    bool seekPending = false;
    std::array<bool, 6> seekHeld{};
    size_t cpCount = 0;
    double autosaveAcc = 0.0;
    bool padClick[2] = {false, false};
    bool clickedPad = false, optStart = false, optStop = false, optRing = false;
    int flipPlayer = 0;
    bool flipLogic[2] = {true, true}, flipPress[2] = {false, false}, flipSwiftNow[2] = {false, false};
    bool ssbActive = false;
    float ssbLast = 1.f;
    int ssbEvents = 0;
};

std::string macroFileName();
void pressButton(GJBaseGameLayer* gl, bool down, int p);
void assistTick(GJBaseGameLayer* gl);
void flipOnReset();
bool layoutOn();
GJBaseGameLayer* activeLayer();
bool isActive(GJBaseGameLayer* gl);
void onEditorPlaytest(bool started);
float ssbFactor();
void computeWant();
void cpsPush(bool p2);
int cpsNow(int p);

Cfg& cfg();
Rt& rt();
void loadCfg();
void saveCfg();
float curTps();

std::filesystem::path macroDir();
std::vector<std::string> listMacros();
bool saveMacro(Macro const& m, std::string const& name);
bool loadMacro(Macro& m, std::string const& name);
bool importMacro(Macro& m, std::filesystem::path const& file);

bool startRecord();
bool startPlay(std::string const& name);
void stopAll(bool save);
void stepOnce();
}
