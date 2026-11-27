#pragma once
#include <Geode/Geode.hpp>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace xyz {
enum class Mode : uint8_t { Off, Record, Play };

// sub = offset from the tick in 1/16 ticks (-8..7), only set by Delta Accuracy recordings
struct Input { uint32_t tick; uint8_t button; bool down; bool p2; int8_t sub = 0; };
struct SplitIn { float f; uint8_t button; bool down, p1; int8_t sub; uint32_t tick; }; // input inside the current tick, f = how far into the step (0..1)
struct PendIn { uint32_t due; int8_t sub; uint8_t button; bool down, p1; }; // Delta Accuracy: input waiting for its tick
struct Fix { uint32_t tick; bool p2; float x, y, rot, yv; };
struct TpsChange { uint32_t tick; uint32_t tps; }; // from this tick on, the macro runs at this TPS
// Old anchor macros (.xyz v4/v5) store x, y, rotation and y-velocity on a fixed grid (units per step, in that order).
inline constexpr double ANCHOR_SCALE[4] = {1024.0, 1024.0, 256.0, 4096.0};
struct Macro {
    std::vector<Input> inputs;
    std::vector<Fix> fixes;
    uint32_t tps = 240;
    uint32_t ticks = 0;
    uint64_t seed = 0;
    int64_t levelId = 0;
    float startX = 0.f;
    std::vector<TpsChange> tpsChanges; // sorted by tick, never at tick 0
    bool delta = false; // recorded with Delta Accuracy (inputs only, sub-tick timing)
    uint32_t anchor = 0; // spacing in ticks of old anchor macros, 0 = not an anchor macro
};

struct Cfg {
    bool tps = false;
    float tpsVal = 240.f;
    float budgetMs = 14.f;
    bool speed = false;
    float speedVal = 1.f;
    bool stepper = false;
    bool noclip = false;
    bool noclipTint = false;
    bool velUnround = false; // GD no longer rounds the y velocity to 3 decimals
    bool xExt = false;       // keeps the x position in a double so a very high TPS does not lose movement
    int accuracy = 2;
    bool cbf = false; // click between steps: inputs keep their time inside a physics step and the step is split there
    bool lockDelta = false; // always use the fixed-step loop, even at 240 TPS
    bool fastSub = true, skipVis = false;
    bool ignoreLevel = false;
    bool hitboxes = false;
    bool hbPlayer = true, hbSolid = true, hbHazard = true, hbSpecial = true, hbRotated = true;
    bool trail = false;
    bool logging = false, logPositions = false;
    int logPosEvery = 1;
    bool trailClear = true;
    int trailLen = 200;
    bool traj = false;
    int trajLen = 240;
    bool blockInputs = true;
    bool maintainGravity = false;
    bool mirror = false, mirrorInv = false, noMirror = false;
    bool preventDeath = false;
    int pdLook = 2;
    bool trajAuto = false;
    bool profiler = false;
    int trajLook = 1;
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
    // Macro tools and the rotation fix (adapted from Echo, GPL-3.0)
    bool fixRotation = false;
    bool waveDragFix = false;
    int renderW = 1280, renderH = 720, renderFps = 60, renderBitrate = 12, renderCodec = 0, renderTail = 2;
    bool renderAutoH = true, renderAudio = true;
    std::string renderMacro;
    bool autoName = true, noOverwrite = false;
    bool autoSave = true; // off = ask for a name when a recording ends
    int renameFmt = 0;
    int recPlayers = 0, playPlayers = 0;
    bool swapInput = false, inverse[2] = {false, false};
    bool seqOn = false, seqRandom = false;
    std::vector<std::string> seq;
    // Showcase and click-assist options (modelled on Scarlet Utils)
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
    int uiStyle = 1, fontGlass = 0, fontPaper = 1;
    bool animations = true;
    bool speedGameplay = false;
    int uiRev = 0;
    float winX = -100000.f, winY = -100000.f, winW = 0.f, winH = 0.f; // Paper window rect, winX <= -50000 = centered
    int themeIdx = 0;
    int accR = 0, accG = 217, accB = 153;
    int tintR = 13, tintG = 20, tintB = 28;
    bool acOn[2] = {false, false};
    int acHold[2] = {4, 4}, acRel[2] = {4, 4}, acClicks[2] = {1, 1};
};

struct Ac { uint64_t last = UINT64_MAX; bool clicked = false; };

enum ProfSlot { P_UPD, P_PLAYER, P_COLL, P_CAM, P_VIS, P_POST, P_PROC, P_FX, P_ENTER, P_LOOP, P_N };
struct ProfOut {
    bool valid = false;
    double ticksPerSec = 0, framesPerSec = 0, ticksPerFrame = 0, frameMs = 0, loopMs = 0, drawMs = 0;
    double upd = 0, player = 0, coll = 0, cam = 0, vis = 0, post = 0, proc = 0, fx = 0, enter = 0, other = 0;
    double gainLo = 1, gainMid = 1, gainHi = 1, capNow = 0;
};
struct Prof {
    double acc[P_N] = {};
    long long ticks = 0, frames = 0;
    double gapUs = 0, winStart = 0, lastFrame = 0;
    ProfOut out;
};

struct Rt {
    Mode mode = Mode::Off;
    uint32_t tick = 0;
    size_t nextIn = 0, nextFix = 0, nextTps = 0;
    Macro rec, play;
    std::array<bool, 6> held{};
    bool feeding = false, resetting = false, simulating = false, simDead = false;
    bool subTick = false;
    Prof prof;
    bool loadedCp = false;
    int stepPending = 0, backReq = 0;
    bool bsReplay = false, bsLoad = false, bsSaving = false;
    double leftover = 0.0;
    float tint = 0.f; // Noclip Tint strength, fades out
    std::chrono::steady_clock::time_point frameT{};
    std::vector<PendIn> pend;
    std::vector<SplitIn> split;
    uint32_t flushTick = 0;
    bool flushing = false;
    int8_t flushSub = 0;
    bool savedTps = false;
    float prevTpsVal = 240.f;
    int64_t levelId = 0;
    bool internal = false;
    std::array<bool, 2> userHeld{}, gameHeld{}, want{}, acHeld{}, frozenWant{}, swiftNow{};
    bool frozen = false;
    Ac ac[2];
    std::vector<double> cpsT[2];
    int maxCps[2] = {0, 0};
    float startX = 0.f;
    std::string levelName;
    std::string macroName, pendingTmp; // name typed in the menu, temp file of a finished recording that is not saved yet
    bool menuOpen = false;
    bool edWasDead = false;
    float edLastX = 0.f;
    bool seekPending = false;
    std::array<bool, 6> seekHeld{};
    size_t cpCount = 0;
    double autosaveAcc = 0.0;
    int seqIdx = -1;
    bool tpsOverridden = false;
    bool padClick[2] = {false, false};
    bool clickedPad = false, optStart = false, optStop = false, optRing = false;
    int flipPlayer = 0;
    bool flipLogic[2] = {true, true}, flipPress[2] = {false, false}, flipSwiftNow[2] = {false, false};
    bool ssbActive = false;
    float ssbLast = 1.f;
    int ssbEvents = 0;
};

std::string macroFileName();
std::string uniqueMacroName(std::string const& base);
bool mergeMacros(std::string const& a, std::string const& b, std::string& outName);
void seqNext();
bool renderAvailable();
bool isRendering();
float renderFps();
std::string startRender();
void renderStop(bool mixAudio);
void renderNotifyEnd();
void renderCapture();
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
void applySplitInput(GJBaseGameLayer* gl, SplitIn const& s);
int cpsNow(int p);

Cfg& cfg();
// the accuracy that is really used: with CBF the fix modes are off and recording always uses the timed inputs
inline int accEff() { auto& c = cfg(); return c.cbf ? 3 : c.accuracy; }
inline int menuFontNow() { auto& c = cfg(); return c.uiStyle ? c.fontPaper : c.fontGlass; }
Rt& rt();
void loadCfg();
void saveCfg();
float curTps();
bool tpsDynamic();
void resyncTps();

struct RenderStats { bool on = false; double realSec = 0, videoSec = 0; long long frames = 0; int fps = 0; };
RenderStats renderStats();
void profFrame(double loopUs, double gapUs);
inline double nowUs() { return std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
struct ProfScope {
    int slot; double t0 = 0; bool on;
    explicit ProfScope(int s) : slot(s), on(cfg().profiler) { if (on) t0 = nowUs(); }
    ~ProfScope() { if (on) rt().prof.acc[slot] += nowUs() - t0; }
};
struct ProfFrame {
    double t0 = 0, gap = 0; bool on;
    ProfFrame() : on(cfg().profiler) {
        if (!on) return;
        static double last = 0;
        t0 = nowUs(); gap = last > 0 ? t0 - last : 0; last = t0;
    }
    ~ProfFrame() { if (on) profFrame(nowUs() - t0, gap); }
};
#define XYZ_TIMED(slot, ...) do { xyz::ProfScope xyzPs(slot); __VA_ARGS__; } while (0)

std::filesystem::path persistDir(std::string const& sub); // geode-persistent/<this mod>/<sub>
std::filesystem::path macroDir();
std::vector<std::string> listMacros();
std::vector<std::string> listMacrosRecent(); // newest first
std::string const& saveError();
bool saveMacro(Macro const& m, std::string const& name);
bool loadMacro(Macro& m, std::string const& name);
bool importMacro(Macro& m, std::filesystem::path const& file);
bool canImport(std::filesystem::path const& file);
std::string const& importError();

bool startRecord();
bool startPlay(std::string const& name);
void stopAll(bool save);
void stepOnce();
void stepBack(int n);
void backstepClear();
void backstepLog(uint32_t tick, int button, bool down, bool p1);
void backstepSave(GJBaseGameLayer* gl, int batch);
bool backstepLoad(PlayLayer* pl, int n, uint32_t& k);
void backstepFeed(GJBaseGameLayer* gl, uint32_t tick);
void backstepFinish(PlayLayer* pl);
}
