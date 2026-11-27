#include "Menu.hpp"
#include <set>
#include "Glass.hpp"
#include "MacroEditor.hpp"
#include "Draw.hpp"
#include "../core/State.hpp"
#include "../core/lua/Lua.hpp"
#include "../core/precision/Precision.hpp"
#include "../clickbot/Clickbot.hpp"
#include "../discord/Discord.hpp"
#include "../vis/Vis.hpp"
#if !defined(GEODE_IS_IOS)
#include <Geode/modify/CCKeyboardDispatcher.hpp>
#endif
#include <memory>
#include <cstring>
#include <Geode/ui/TextInput.hpp>
#include <map>
#include <fstream>
#include <iterator>

using namespace geode::prelude;
using namespace xyz;
using namespace xyz::draw;

static XyzMenu* s_open = nullptr;
static constexpr float SW = 98.f;
static constexpr float ROW = 30.f;
static int s_startTab = 0;
static std::string s_newScript;
static bool paper() { return cfg().uiStyle == 1; }
static std::set<std::string>& closedSecs() { static std::set<std::string> s; return s; }
static ccColor4F pGray(float v, float a = 1.f) { return {v, v, v, a}; }
static constexpr float P_TITLE = 32.f, P_TABS = 38.f, P_FOOT = 24.f, P_MINW = 320.f, P_MINH = 190.f;
static ccColor4F pWin() { return {0.062f, 0.066f, 0.078f, 0.96f}; }

static ccColor4F acc(float a = 1.f, float mul = 1.f) {
    auto& c = cfg();
    return {c.accR / 255.f * mul, c.accG / 255.f * mul, c.accB / 255.f * mul, a};
}
static ccColor4F accL(float a = 1.f) { return mix(acc(a), {1, 1, 1, a}, 0.5f); }
static ccColor3B to3(ccColor4F c) { return {(GLubyte)(c.r * 255), (GLubyte)(c.g * 255), (GLubyte)(c.b * 255)}; }
static ccColor3B tint3() { auto& c = cfg(); return {(GLubyte)c.tintR, (GLubyte)c.tintG, (GLubyte)c.tintB}; }

static int const LABEL_COLORS[][3] = {
    {255, 255, 255}, {0, 217, 153}, {255, 90, 90}, {255, 160, 60}, {255, 225, 80},
    {100, 255, 120}, {80, 170, 255}, {180, 120, 255}, {255, 120, 190},
};

static CCMenuItemSpriteExtra* makeSwitch(bool on, std::function<void(bool)> cb) {
    if (paper()) {
        auto node = CCNode::create();
        node->setContentSize({18, 18});
        auto bx = CCDrawNode::create(); node->addChild(bx);
        auto ck = CCDrawNode::create(); node->addChild(ck, 1);
        auto st = std::make_shared<bool>(on);
        auto draw = [bx, ck, st] {
            bx->clear(); ck->clear();
            rrect(bx, {0, 0}, {18, 18}, 3, pGray(0.11f), 1.f, *st ? accL(1.f) : pGray(0.36f));
            if (*st) {
                ck->drawSegment({4.5f, 9.f}, {7.8f, 5.4f}, 1.1f, pm(accL(1.f)));
                ck->drawSegment({7.8f, 5.4f}, {13.5f, 12.6f}, 1.1f, pm(accL(1.f)));
            }
        };
        draw();
        return CCMenuItemExt::createSpriteExtra(node, [st, draw, cb, node](auto) {
            *st = !*st; draw();
            if (cfg().animations) {
                node->stopAllActions();
                node->setScale(.93f);
                node->runAction(CCEaseSineOut::create(CCScaleTo::create(.12f, 1.f)));
            }
            cb(*st);
        });
    }
    auto node = CCNode::create();
    node->setContentSize({34, 20});
    auto tr = CCDrawNode::create(); node->addChild(tr);
    auto kn = CCDrawNode::create(); node->addChild(kn, 1);
    auto st = std::make_shared<bool>(on);
    auto draw = [tr, kn, st] {
        bool v = *st;
        tr->clear(); kn->clear();
        rrect(tr, {0, 3}, {34, 14}, 7, v ? acc(0.95f, 0.8f) : ccc4f(0.04f, 0.03f, 0.06f, 0.92f), 1.f, v ? accL(0.95f) : ccc4f(1, 1, 1, 0.22f));
        disc(kn, {0, 0}, 4.6f, ccc4f(1, 1, 1, 1.f), 0.f, {0, 0, 0, 0}, 20);
    };
    draw();
    kn->setPosition({on ? 25.f : 9.f, 10.f});
    return CCMenuItemExt::createSpriteExtra(node, [st, draw, cb, kn](auto) {
        *st = !*st; draw();
        kn->stopAllActions();
        if (cfg().animations) kn->runAction(CCEaseSineOut::create(CCMoveTo::create(.1f, {*st ? 25.f : 9.f, 10.f})));
        else kn->setPosition({*st ? 25.f : 9.f, 10.f});
        cb(*st);
    });
}

static CCMenuItemSpriteExtra* makePill(std::string const& text, bool accent, std::function<void()> cb, float minW = 0.f) {
    auto lbl = mkLabel(text.c_str());
    sc(lbl, .34f);
    float w = std::max(minW, lbl->getScaledContentSize().width + 18.f);
    auto node = CCNode::create();
    node->setContentSize({w, 20});
    auto d = CCDrawNode::create();
    if (paper()) rrect(d, {0, 0}, {w, 20}, 4, accent ? acc(1.f, 0.6f) : pGray(0.17f), 1.f, accent ? accL(0.9f) : pGray(0.27f));
    else rrect(d, {0, 0}, {w, 20}, 5, accent ? acc(0.9f, 0.75f) : ccc4f(1, 1, 1, 0.10f), 1.f, accent ? accL(0.8f) : ccc4f(1, 1, 1, 0.14f));
    node->addChild(d);
    lbl->setPosition({w / 2, 10});
    node->addChild(lbl);
    return CCMenuItemExt::createSpriteExtra(node, [cb](auto) { cb(); });
}

static CCMenuItemSpriteExtra* makeRound(char const* glyph, std::function<void()> cb) {
    auto node = CCNode::create();
    node->setContentSize({20, 20});
    auto d = CCDrawNode::create();
    if (paper()) rrect(d, {0, 0}, {20, 20}, 4, pGray(0.17f), 1.f, pGray(0.27f));
    else rrect(d, {0, 0}, {20, 20}, 5, ccc4f(1, 1, 1, 0.10f), 1.f, ccc4f(1, 1, 1, 0.14f));
    node->addChild(d);
    auto l = mkLabel(glyph);
    sc(l, .42f); l->setPosition({10, 10});
    node->addChild(l);
    return CCMenuItemExt::createSpriteExtra(node, [cb](auto) { cb(); });
}

static CCMenuItemSpriteExtra* makeGear(std::function<void()> cb) {
    if (paper()) {
        auto node = CCNode::create();
        node->setContentSize({22, 20});
        auto d = CCDrawNode::create();
        rrect(d, {0, 0}, {22, 20}, 4, pGray(0.17f), 1.f, pGray(0.27f));
        for (int i = 0; i < 8; ++i) {
            float a = i * 3.14159265f / 4.f;
            disc(d, {11 + std::cos(a) * 5.2f, 10 + std::sin(a) * 5.2f}, 1.5f, pGray(0.88f), 0.f, {0, 0, 0, 0}, 8);
        }
        disc(d, {11, 10}, 4.3f, pGray(0.88f), 0.f, {0, 0, 0, 0}, 16);
        disc(d, {11, 10}, 1.9f, pGray(0.17f), 0.f, {0, 0, 0, 0}, 12);
        node->addChild(d);
        return CCMenuItemExt::createSpriteExtra(node, [cb](auto) { cb(); });
    }
    auto node = CCNode::create();
    node->setContentSize({22, 22});
    auto d = CCDrawNode::create();
    disc(d, {11, 11}, 11.f, ccc4f(1, 1, 1, 0.10f), 0.f, {0, 0, 0, 0}, 24);
    for (int i = 0; i < 8; ++i) {
        float a = i * 3.14159265f / 4.f;
        disc(d, {11 + std::cos(a) * 7.4f, 11 + std::sin(a) * 7.4f}, 2.3f, ccc4f(0.85f, 0.95f, 1.f, 0.95f), 0.f, {0, 0, 0, 0}, 8);
    }
    disc(d, {11, 11}, 6.2f, ccc4f(0.85f, 0.95f, 1.f, 0.95f), 0.f, {0, 0, 0, 0}, 20);
    disc(d, {11, 11}, 2.6f, ccc4f(0.06f, 0.09f, 0.12f, 1.f), 0.f, {0, 0, 0, 0}, 14);
    node->addChild(d);
    return CCMenuItemExt::createSpriteExtra(node, [cb](auto) { cb(); });
}

static CCMenuItemSpriteExtra* makeChip(ccColor3B col, bool sel, std::function<void()> cb) {
    auto node = CCNode::create();
    node->setContentSize({24, 24});
    auto d = CCDrawNode::create();
    disc(d, {12, 12}, 9.5f, {col.r / 255.f, col.g / 255.f, col.b / 255.f, 1.f}, 0.f, {0, 0, 0, 0}, 24);
    if (sel) disc(d, {12, 12}, 11.5f, {0, 0, 0, 0}, 1.6f, {1, 1, 1, 0.95f}, 28);
    else disc(d, {12, 12}, 10.f, {0, 0, 0, 0}, 1.f, {1, 1, 1, 0.3f}, 28);
    node->addChild(d);
    return CCMenuItemExt::createSpriteExtra(node, [cb](auto) { cb(); });
}

static std::string helpText(std::string const& k) {
    static std::map<std::string, std::string> const H = {
        {"Frame Stepper", "Freezes the game and only advances when you step. On touch devices use the on-screen controls, on desktop press F7 (step) or F9 (back)."},
        {"On-Screen Controls", "Shows Back, -N, Step, +N, Hold and Run buttons at the bottom right of the screen while the stepper is on."},
        {"Steps Per +N Button", "How many ticks the +N and -N buttons move at once."},
        {"Hold Speed (ticks/frame)", "How many ticks the Hold button advances every frame while you keep it pressed."},
        {"Noclip", "Stops the player from dying."},
        {"Frame Fixes", "Records the player's position, rotation and Y velocity every tick so macros stay accurate. Makes macro files larger."},
        {"Ignore Level Check", "Lets you play a macro on a level other than the one it was recorded on."},
        {"Block Inputs In Playback", "Ignores your own taps while a macro is playing."},
        {"Autosave While Recording", "Saves an 'autosave' macro every few seconds while you record."},
        {"Interval (s)", "How many seconds pass between autosaves."},
        {"Auto Exit On Finish", "Leaves the level when a macro finishes playing."},
        {"TPS Bypass", "Runs the physics at a custom ticks-per-second. Higher is more accurate. Macros replay at the TPS they were recorded at."},
        {"TPS", "The ticks-per-second the physics runs at. 240 is normal."},
        {"Frame Budget (ms)", "The most time spent on physics each frame. If it runs out, the game slows down instead of freezing."},
        {"Speedhack", "Changes how fast the whole game runs."},
        {"Gameplay Only", "The speedhack only runs while you play a level. It turns off in the pause menu, in the xyzBot menu and on every other screen."},
        {"Animations", "Turns the menu, button and switch animations on or off."},
        {"Speed", "Game speed multiplier. 1.00 is normal."},
        {"Speedhack Audio", "Changes the music pitch together with the speed."},
        {"SSB Fix (song sync)", "While the scroll speed bug is happening, changes the song speed so it stays in sync with the level."},
        {"Invert Correction", "Flips the direction of the song correction. Turn on if the song drifts further off instead of back."},
        {"Legacy Ship Weight (2.1)", "Makes the upside-down ship heavier, like in 2.1 and earlier."},
        {"Extra Weight (%)", "How much extra weight the upside-down ship gets. Starts at 6."},
        {"Show Hitboxes", "Draws the hitboxes of the player and the level's objects."},
        {"Hitbox Trail", "Draws the player's hitboxes from the last few moments."},
        {"Trail Length", "How many past ticks the hitbox trail keeps."},
        {"Logging", "Writes inputs, attempts and deaths to a log file in the mod's logs folder."},
        {"Position Logging", "Also writes the player position, rotation and y-velocity. Large files when the interval is 1."},
        {"Position Interval", "Ticks between position lines. 1 logs every tick."},
        {"Clear Trail On Death", "Wipes the hitbox trail when you respawn. Turn it off to keep the old trail around."},
        {"Show Trajectory", "Previews where the player will go if you hold or release right now."},
        {"Length", "How many ticks ahead the preview looks."},
        {"Layout Mode (reload level)", "Strips decoration so only gameplay objects show. Takes effect when the level loads again."},
        {"Player", "Shows the player's hitboxes."},
        {"Solid / Hazard", "Shows the hitboxes of blocks and spikes."},
        {"Special / Rotated", "Shows portals, orbs and pads, and rotated hitboxes."},
        {"Prevent Death", "Freezes the game one moment before a predicted death so you can change your input."},
        {"Lookahead Ticks", "How many ticks ahead Prevent Death looks for a death."},
        {"Maintain Gravity", "Flips your hold while upside down so wave and ship controls stay the same."},
        {"Mirror Inputs", "Player 2 copies Player 1's inputs."},
        {"Mirror Inverted", "Player 2 does the opposite of Player 1."},
        {"No Mirror Portals", "Mirror portals stop flipping the screen."},
        {"Autoclicker P1", "Clicks for Player 1 automatically using the hold and release ticks in its settings."},
        {"Autoclicker P2", "Clicks for Player 2 automatically using the hold and release ticks in its settings."},
        {"Hold Ticks", "How many ticks each click is held."},
        {"Release Ticks", "How many ticks pass between clicks."},
        {"Clicks Per Hold", "Extra clicks added each time the button is pressed."},
        {"Labels Enabled", "Shows or hides every on-screen label."},
        {"Enabled", "Shows or hides this label."},
        {"Anchor", "Which corner of the screen the label sits in."},
        {"Size", "How large the label is."},
        {"Opacity", "How see-through the label is."},
        {"Red", "Red amount, from 0 to 255."}, {"Green", "Green amount, from 0 to 255."}, {"Blue", "Blue amount, from 0 to 255."},
        {"Fix Rotation Bug", "Fixes the ship, UFO, wave and swing tilt looking wrong at high TPS. Only changes how it looks, not the physics."},
        {"Wave Drag Fix", "Fixes the wave trail on D blocks (drag), slopes, portals and spider orbs, also at high TPS. Only changes the trail, not the physics."},
        {"Trajectory Autoclicker", "Every tick it simulates what happens if the current input state continues. If that dies within the lookahead, it holds, releases or taps instead, whichever survives longest. Works while recording, so the clicks are saved in the macro. Orbs, pads and portals are not simulated yet."},
        {"Click Lookahead", "How many ticks ahead the Trajectory Autoclicker looks. 1 only reacts to death on the next tick (Echo default). Higher reacts earlier but costs more per tick."},
        {"Profiler", "Times what a tick costs and where it goes, and estimates how much faster running many ticks in one update call could be. Show the results with the Profiler labels (Labels page). Costs a little speed itself, turn it off when you are done."},
        {"UI Style", "Glass is the blurred panel with the tabs on the side. Paper is a plain flat panel with the tabs on top. The menu reopens when you switch."},
        {"Velocity Unrounding", "Stops GD from rounding the y velocity to the third decimal. This changes the physics: macros must be recorded and played with the same setting, and it is not meant for normal play or submissions. Full effect on Windows GD 2.2091 only."},
        {"X Position Extension", "Experimental. At a very high TPS one step moves the player less than the gap between two floats, so the game loses the movement. This keeps the exact x in a double. It measures the player's speed on its own and only helps in classic (non platformer) levels."},
        {"Discord Rich Presence", "Shows the level you play and whether you record or play a macro on your Discord profile. PC only. Needs a Discord Application ID, see src/discord/Discord.hpp."},
        {"CBF (click between steps)", "Clicks keep their exact time inside a physics step and the step is split there, like the CBF mod. Macros store the timing and replay with the same split. Turns the fix modes off (Accuracy shows Delta Accuracy). Experimental. Do not run the CBF mod at the same time."},
        {"Accuracy", "Vanilla: no position corrections. Input Fixes: the player position is stored and restored at every click. Frame Fixes: stored and restored every tick. A macro plays with whatever fixes it contains, so macros from xdBot load in any mode."},
        {"Fast Sub-Ticks", "At high TPS, skips the progress bar update on the ticks of a frame that are never drawn. Does not change the physics."},
        {"Skip Visibility On Sub-Ticks", "Experimental. Also skips the object visibility refresh on ticks that are never drawn. Faster on big levels, turn off if anything looks wrong."},
        {"Record Players", "Which players get recorded. Pick one player to record the other later and merge the two macros."},
        {"Play Players", "Which players the macro controls during playback. You control the other player yourself."},
        {"Auto Save", "When on, a finished recording saves by itself with the automatic name. When off, you are asked for a name every time a recording ends."},
        {"Auto Name Replay", "Names a new macro after the level. When off, it is called 'macro'."},
        {"Rename Format", "How a number is added to the name when No Overwrite is on."},
        {"No Overwrite", "A new recording gets a numbered name instead of replacing the macro with the same name."},
        {"Sequence", "When a macro finishes, the next macro in your sequence starts. Add macros with +Seq."},
        {"Random Order", "Picks the next macro in the sequence at random."},
        {"Swap Player Input", "Your Player 1 taps control Player 2 and the other way around."},
        {"Inverse Input P1", "Player 1 holds while you are not touching the screen, and lets go while you are."},
        {"Inverse Input P2", "Player 2 holds while you are not touching the screen, and lets go while you are."},
        {"No Death Effect", "Hides the player's death explosion."},
        {"No Effects", "Stops most visual effects (circles, particles, shines, trigger flashes) from showing."},
        {"Hide Endscreen", "Skips the end-of-level screen. Useful for recording."},
        {"Hide New Best", "Hides the 'New Best' popup."},
        {"Stacked Orb Optimization", "When several orbs are hit in a row, only the first one plays its effect."},
        {"Restart First Frame", "Restarts the attempt once right after the level starts. Fixes levels that break on their first frame when recording."},
        {"Level Fade In/Out", "Fades the level in from black at the start and out to black at the end. Made for recording."},
        {"Audio Fade In/Out", "Fades the music in at the start and out at the end. Made for recording."},
        {"Fade In (s)", "How many seconds the level takes to fade in."},
        {"Fade Out (s)", "How many seconds the level takes to fade out."},
        {"Audio Fade In (s)", "How many seconds the music takes to fade in."},
        {"Audio Fade Out (s)", "How many seconds the music takes to fade out."},
        {"Click Green Dash Orbs", "When you press while touching a green dash orb, it releases first so the orb always triggers."},
        {"Click Black Orbs", "When you press while touching a black orb, it releases first so the orb always triggers."},
        {"Easy Black Orb UFO", "Only clicks black orbs while the UFO is falling, so they do not push you the wrong way."},
        {"Click Jump Pads", "Automatically taps when you hit a pink, yellow or red pad."},
        {"Click Gravity Pads", "Automatically taps when you hit a blue pad."},
        {"Auto Straight Fly", "Presses and releases for you to keep the ship level. It can drift in some spots."},
        {"Auto Straight UFO", "Taps to hold the UFO near a set height."},
        {"Auto Swift", "Every click becomes a quick tap: press and release at once."},
        {"Extra Clicks", "Adds extra quick taps after each click you make."},
        {"Extra Clicks Amount", "How many extra taps each click adds."},
        {"Flip Input On Death", "After you die, flips the input of the player that died when the attempt restarts."},
        {"Flip Player 1", "Flip the input when Player 1 dies."},
        {"Flip Player 2", "Flip the input when Player 2 dies."},
        {"Flip Both", "Flip both players' inputs whenever anyone dies."},
        {"Flip Swift", "Flip with a quick tap instead of holding."},
        {"Noclip Player 1", "Noclip applies to Player 1."},
        {"Noclip Player 2", "Noclip applies to Player 2."},
        {"Noclip Tint", "Flashes the screen red while noclip is saving you from a death."},
        {"Swift Clicks", "The autoclicker taps (press and release at once) instead of holding."},
        {"Straight Fly P1", "Use Auto Straight Fly on Player 1."}, {"Straight Fly P2", "Use Auto Straight Fly on Player 2."},
        {"Fly Threshold P1", "How much vertical speed is allowed before the ship is corrected."}, {"Fly Threshold P2", "How much vertical speed is allowed before the ship is corrected."},
        {"Straight UFO P1", "Use Auto Straight UFO on Player 1."}, {"Straight UFO P2", "Use Auto Straight UFO on Player 2."},
        {"UFO Target Y P1", "The height Player 1's UFO is held at."}, {"UFO Target Y P2", "The height Player 2's UFO is held at."},
        {"UFO Threshold P1", "How far Player 1's UFO may drift from the target."}, {"UFO Threshold P2", "How far Player 2's UFO may drift from the target."},
        {"Macro To Render", "The macro that plays while rendering. 'None' lets you play the level yourself."},
        {"Width", "Video width in pixels. The height follows the shape of your screen."},
        {"Auto Height (match screen)", "When on, video height is calculated from the game's current window aspect ratio. Turn it off to choose a fixed height."},
        {"Video Height", "The renderer's output height in pixels. Adjust the slider or use the arrows; changing it switches Auto Height off."},
        {"FPS", "Frames per second of the video."},
        {"Bitrate (Mbps)", "Video quality. Higher looks better and makes a bigger file."},
        {"Codec", "Video encoder. Auto picks the best one your device offers."},
        {"End Tail (s)", "How many seconds keep recording after the level ends."},
        {"Include Song Audio", "Adds the level's song to the video afterwards. It starts at the beginning of the song, so it may be off for StartPos or practice runs."},
        {"Button In Levels", "Shows the floating button while you play. Turn off to hide it until you pause."},
    };
    auto it = H.find(k);
    return it == H.end() ? std::string() : it->second;
}

static CCMenuItemSpriteExtra* makeHelp(std::string title, std::string text) {
    auto node = CCNode::create();
    node->setContentSize({16, 16});
    if (paper()) {
        auto l = mkLabel("(?)");
        sc(l, .34f); l->setOpacity(130); l->setPosition({8, 8});
        node->addChild(l);
        return CCMenuItemExt::createSpriteExtra(node, [title, text](auto) { FLAlertLayer::create(title.c_str(), text, "OK")->show(); });
    }
    auto d = CCDrawNode::create();
    disc(d, {8, 8}, 7.5f, ccc4f(1, 1, 1, 0.07f), 1.f, ccc4f(1, 1, 1, 0.3f), 20);
    node->addChild(d);
    auto l = mkLabel("?");
    sc(l, .34f); l->setOpacity(110); l->setPosition({8, 8});
    node->addChild(l);
    return CCMenuItemExt::createSpriteExtra(node, [title, text](auto) { FLAlertLayer::create(title.c_str(), text, "OK")->show(); });
}

static int autoRenderHeight() {
    auto ws = CCDirector::get()->getWinSize();
    if (ws.width <= 0.f) return 720;
    int width = std::clamp(cfg().renderW, 320, 3840) & ~1;
    return (int)std::lround(width * ws.height / ws.width) & ~1;
}

static std::function<void(double)> numSetter(std::string const& k) {
    auto& c = cfg();
    auto F = [](float* p, float lo, float hi) { return std::function<void(double)>([p, lo, hi](double v) { *p = std::clamp((float)v, lo, hi); saveCfg(); }); };
    auto I = [](int* p, int lo, int hi) { return std::function<void(double)>([p, lo, hi](double v) { *p = std::clamp((int)std::lround(v), lo, hi); saveCfg(); }); };
    if (k == "TPS") return F(&c.tpsVal, 1.f, 1000000.f);
    if (k == "Frame Budget (ms)") return F(&c.budgetMs, 1.f, 100.f);
    if (k == "Speed") return F(&c.speedVal, 0.05f, 100.f);
    if (k == "Fade In (s)") return F(&c.fadeLevelIn, 0.f, 10.f);
    if (k == "Fade Out (s)") return F(&c.fadeLevelOut, 0.f, 10.f);
    if (k == "Audio Fade In (s)") return F(&c.fadeAudioIn, 0.f, 10.f);
    if (k == "Audio Fade Out (s)") return F(&c.fadeAudioOut, 0.f, 10.f);
    if (k == "Fly Threshold P1") return F(&c.sFlyThr[0], 0.f, 50.f);
    if (k == "Fly Threshold P2") return F(&c.sFlyThr[1], 0.f, 50.f);
    if (k == "UFO Target Y P1") return F(&c.sUfoTarget[0], 0.f, 3000.f);
    if (k == "UFO Target Y P2") return F(&c.sUfoTarget[1], 0.f, 3000.f);
    if (k == "UFO Threshold P1") return F(&c.sUfoThr[0], 0.f, 300.f);
    if (k == "UFO Threshold P2") return F(&c.sUfoThr[1], 0.f, 300.f);
    if (k == "Extra Clicks Amount") return I(&c.extraAmt, 1, 10);
    if (k == "Position Interval") return I(&c.logPosEvery, 1, 240);
    if (k == "Steps Per +N Button") return I(&c.stepN, 1, 240);
    if (k == "Hold Speed (ticks/frame)") return I(&c.holdRate, 1, 16);
    if (k == "Extra Weight (%)") return I(&c.legacyShipPct, 0, 100);
    if (k == "Trail Length") return I(&c.trailLen, 10, 2000);
    if (k == "Length") return I(&c.trajLen, 10, 2000);
    if (k == "Lookahead Ticks") return I(&c.pdLook, 1, 10);
    if (k == "Click Lookahead") return I(&c.trajLook, 1, 240);
    if (k == "Interval (s)") return I(&c.autosaveSec, 10, 3600);
    if (k == "Width") return I(&c.renderW, 320, 3840);
    if (k == "Video Height") {
        auto* height = &c.renderH;
        auto* automatic = &c.renderAutoH;
        return [height, automatic](double v) {
            *height = std::clamp((int)std::lround(v), 320, 3840) & ~1;
            *automatic = false;
            saveCfg();
        };
    }
    if (k == "FPS") return I(&c.renderFps, 10, 240);
    if (k == "Bitrate (Mbps)") return I(&c.renderBitrate, 1, 200);
    if (k == "End Tail (s)") return I(&c.renderTail, 0, 30);
    return nullptr;
}

static bool sliderInfo(std::string const& k, double& lo, double& hi, std::function<double()>& val) {
    auto& c = cfg();
    auto F = [&](float* p, double a, double b) { lo = a; hi = b; val = [p] { return (double)*p; }; return true; };
    auto I = [&](int* p, double a, double b) { lo = a; hi = b; val = [p] { return (double)*p; }; return true; };
    if (k == "Fade In (s)") return F(&c.fadeLevelIn, 0, 10);
    if (k == "Fade Out (s)") return F(&c.fadeLevelOut, 0, 10);
    if (k == "Audio Fade In (s)") return F(&c.fadeAudioIn, 0, 10);
    if (k == "Audio Fade Out (s)") return F(&c.fadeAudioOut, 0, 10);
    if (k == "Frame Budget (ms)") return F(&c.budgetMs, 1, 100);
    if (k == "Fly Threshold P1") return F(&c.sFlyThr[0], 0, 50);
    if (k == "Fly Threshold P2") return F(&c.sFlyThr[1], 0, 50);
    if (k == "UFO Threshold P1") return F(&c.sUfoThr[0], 0, 300);
    if (k == "UFO Threshold P2") return F(&c.sUfoThr[1], 0, 300);
    if (k == "Extra Clicks Amount") return I(&c.extraAmt, 1, 10);
    if (k == "Position Interval") return I(&c.logPosEvery, 1, 240);
    if (k == "Steps Per +N Button") return I(&c.stepN, 1, 240);
    if (k == "Hold Speed (ticks/frame)") return I(&c.holdRate, 1, 16);
    if (k == "Extra Weight (%)") return I(&c.legacyShipPct, 0, 100);
    if (k == "Trail Length") return I(&c.trailLen, 10, 2000);
    if (k == "Length") return I(&c.trajLen, 10, 2000);
    if (k == "Lookahead Ticks") return I(&c.pdLook, 1, 10);
    if (k == "Click Lookahead") return I(&c.trajLook, 1, 240);
    if (k == "FPS") return I(&c.renderFps, 10, 240);
    if (k == "Width") return I(&c.renderW, 320, 3840);
    if (k == "Video Height") {
        lo = 320;
        hi = 3840;
        val = [] {
            auto& c = cfg();
            return (double)(c.renderAutoH ? autoRenderHeight() : c.renderH);
        };
        return true;
    }
    if (k == "Bitrate (Mbps)") return I(&c.renderBitrate, 1, 200);
    if (k == "End Tail (s)") return I(&c.renderTail, 0, 30);
    return false;
}

class XyzValuePopup : public geode::Popup {
    TextInput* m_input = nullptr;
    std::function<void(double)> m_cb;
public:
    static void open(std::string title, std::string cur, std::function<void(double)> cb) {
        auto p = new XyzValuePopup();
        if (p->setup(title, cur, std::move(cb))) { p->autorelease(); p->show(); }
        else delete p;
    }
    bool setup(std::string const& title, std::string const& cur, std::function<void(double)> cb) {
        m_cb = std::move(cb);
        if (!Popup::init(250.f, 135.f)) return false;
        this->setTitle(title);
        m_input = TextInput::create(180.f, "Value", xyz::draw::fontOf());
        m_input->setFilter("0123456789.-");
        m_input->setString(cur);
        m_input->setPosition({125.f, 72.f});
        m_mainLayer->addChild(m_input);
        auto menu = CCMenu::create();
        menu->setPosition({0, 0});
        m_mainLayer->addChild(menu);
        auto ok = CCMenuItemExt::createSpriteExtra(xyz::draw::restyleButton(ButtonSprite::create("Apply")), [this](auto) {
            auto r = geode::utils::numFromString<double>(m_input->getString());
            if (r.isErr()) { Notification::create("That is not a number", NotificationIcon::Error)->show(); return; }
            m_cb(r.unwrap());
            this->onClose(nullptr);
        });
        ok->setPosition({125.f, 28.f});
        menu->addChild(ok);
        return true;
    }
};

// Slider for the Glass UI. A CCMenu subclass so it gets touches like every other menu (also inside the scroll layer).
class XyzSlider : public CCMenu {
    float m_w = 100.f, m_frac = 0.f;
    bool m_drag = false;
    CCDrawNode* m_draw = nullptr;
    CCNode* m_view = nullptr;
    std::function<void(float)> m_cb;

    void redraw() {
        m_draw->clear();
        float x = m_w * m_frac;
        m_draw->drawSegment({0, 0}, {m_w, 0}, 1.3f, pm(ccc4f(1, 1, 1, 0.22f)));
        if (x > 0.5f) m_draw->drawSegment({0, 0}, {x, 0}, 1.3f, pm(accL(1.f)));
        disc(m_draw, {x, 0}, 5.2f, ccc4f(1, 1, 1, 1.f), 0.f, {0, 0, 0, 0}, 20);
    }
    bool inside(CCTouch* t, float pad) {
        for (CCNode* n = this; n; n = n->getParent()) if (!n->isVisible()) return false;
        auto loc = t->getLocation();
        if (m_view) {
            auto lp = m_view->convertToNodeSpace(loc);
            auto sz = m_view->getContentSize();
            if (lp.x < 0 || lp.y < 0 || lp.x > sz.width || lp.y > sz.height) return false;
        }
        auto p = this->convertToNodeSpace(loc);
        return p.x >= -pad && p.x <= m_w + pad && p.y >= 0.f && p.y <= 18.f;
    }
    void applyTouch(CCTouch* t) {
        auto p = this->convertToNodeSpace(t->getLocation());
        float f = std::clamp(p.x / m_w, 0.f, 1.f);
        if (f == m_frac) return;
        m_frac = f;
        this->redraw();
        if (m_cb) m_cb(f);
    }
public:
    static XyzSlider* create(float w, float frac, CCNode* view, std::function<void(float)> cb) {
        auto s = new XyzSlider();
        if (s && s->init()) {
            s->autorelease();
            s->m_w = w; s->m_frac = std::clamp(frac, 0.f, 1.f); s->m_view = view; s->m_cb = std::move(cb);
            s->setContentSize({w, 18.f});
            s->m_draw = CCDrawNode::create();
            s->m_draw->setPosition({0, 9.f});
            s->addChild(s->m_draw);
            s->redraw();
            return s;
        }
        delete s;
        return nullptr;
    }
    bool ccTouchBegan(CCTouch* t, CCEvent*) override {
        if (!this->inside(t, 8.f)) return false;
        m_drag = true;
        this->applyTouch(t);
        return true;
    }
    void ccTouchMoved(CCTouch* t, CCEvent*) override { if (m_drag) this->applyTouch(t); }
    void ccTouchEnded(CCTouch*, CCEvent*) override { m_drag = false; }
    void ccTouchCancelled(CCTouch*, CCEvent*) override { m_drag = false; }
};

class XyzChoicePopup : public geode::Popup {
    std::function<void(int)> m_cb;
public:
    static void open(std::string title, std::vector<std::string> opts, int cur, std::function<void(int)> cb) {
        auto p = new XyzChoicePopup();
        if (p->setup(title, opts, cur, std::move(cb))) { p->autorelease(); p->show(); }
        else delete p;
    }
    bool setup(std::string const& title, std::vector<std::string> const& opts, int cur, std::function<void(int)> cb) {
        m_cb = std::move(cb);
        float rowH = 28.f;
        float h = std::min(64.f + opts.size() * rowH, 300.f);
        if (!Popup::init(240.f, h)) return false;
        this->setTitle(title);
        auto menu = CCMenu::create();
        menu->setPosition({0, 0});
        m_mainLayer->addChild(menu);
        for (size_t i = 0; i < opts.size(); ++i) {
            int idx = (int)i;
            auto b = makePill(opts[i], idx == cur, [this, idx] {
                auto cb2 = m_cb;
                this->onClose(nullptr);
                if (cb2) cb2(idx);
            }, 170.f);
            b->setPosition({120.f, h - 52.f - i * rowH});
            menu->addChild(b);
        }
        return true;
    }
};

class XyzTextPopup : public geode::Popup {
    TextInput* m_input = nullptr;
    std::function<void(std::string)> m_cb;
public:
    static void open(std::string title, std::string cur, std::function<void(std::string)> cb, std::string ok = "Search", std::string hint = "Type to search") {
        auto p = new XyzTextPopup();
        if (p->setup(title, cur, std::move(cb), ok, hint)) { p->autorelease(); p->show(); }
        else delete p;
    }
    bool setup(std::string const& title, std::string const& cur, std::function<void(std::string)> cb, std::string const& ok = "Search", std::string const& hint = "Type to search") {
        m_cb = std::move(cb);
        if (!Popup::init(250.f, 135.f)) return false;
        this->setTitle(title);
        m_input = TextInput::create(180.f, hint, xyz::draw::fontOf());
        m_input->setString(cur);
        m_input->setPosition({125.f, 72.f});
        m_mainLayer->addChild(m_input);
        auto menu = CCMenu::create();
        menu->setPosition({0, 0});
        m_mainLayer->addChild(menu);
        auto okb = CCMenuItemExt::createSpriteExtra(xyz::draw::restyleButton(ButtonSprite::create(ok.c_str())), [this](auto) {
            auto v = m_input->getString();
            auto cb2 = m_cb;
            this->onClose(nullptr);
            cb2(v);
        });
        okb->setPosition({125.f, 28.f});
        menu->addChild(okb);
        return true;
    }
};

static std::string cleanMacroName(std::string s) {
    for (auto& ch : s) if (std::string("\\/:*?\"<>|").find(ch) != std::string::npos) ch = '_';
    while (!s.empty() && s.back() == ' ') s.pop_back();
    while (!s.empty() && s.front() == ' ') s.erase(s.begin());
    return s;
}

static ccColor3B hsv2rgb(float h, float s, float v) {
    h = std::fmod(std::max(h, 0.f), 360.f) / 60.f;
    int i = (int)h;
    float f = h - (float)i, p = v * (1.f - s), q = v * (1.f - s * f), t = v * (1.f - s * (1.f - f));
    float r = v, g = v, b = v;
    switch (i % 6) {
        case 0: r = v; g = t; b = p; break;
        case 1: r = q; g = v; b = p; break;
        case 2: r = p; g = v; b = t; break;
        case 3: r = p; g = q; b = v; break;
        case 4: r = t; g = p; b = v; break;
        default: r = v; g = p; b = q; break;
    }
    return {(GLubyte)std::lround(r * 255.f), (GLubyte)std::lround(g * 255.f), (GLubyte)std::lround(b * 255.f)};
}

// h is left alone for greys so the hue does not jump while dragging
static void rgb2hsv(int r, int g, int b, float& h, float& s, float& v) {
    float R = r / 255.f, G = g / 255.f, B = b / 255.f;
    float mx = std::max(R, std::max(G, B)), mn = std::min(R, std::min(G, B)), d = mx - mn;
    v = mx;
    s = mx > 0.f ? d / mx : 0.f;
    if (d <= 0.f) return;
    if (mx == R) h = 60.f * std::fmod((G - B) / d, 6.f);
    else if (mx == G) h = 60.f * ((B - R) / d + 2.f);
    else h = 60.f * ((R - G) / d + 4.f);
    if (h < 0.f) h += 360.f;
}

// touch area that reports where inside it (0..1) the finger is
class XyzPad : public CCMenu {
    float m_w = 100.f, m_h = 100.f;
    bool m_drag = false;
    std::function<void(float, float)> m_cb;
    void apply(CCTouch* t) {
        auto p = this->convertToNodeSpace(t->getLocation());
        if (m_cb) m_cb(std::clamp(p.x / m_w, 0.f, 1.f), std::clamp(p.y / m_h, 0.f, 1.f));
    }
public:
    static XyzPad* create(float w, float h, std::function<void(float, float)> cb) {
        auto r = new XyzPad();
        if (r && r->init()) {
            r->autorelease();
            r->m_w = w; r->m_h = h; r->m_cb = std::move(cb);
            r->setContentSize({w, h});
            return r;
        }
        delete r;
        return nullptr;
    }
    bool ccTouchBegan(CCTouch* t, CCEvent*) override {
        for (CCNode* n = this; n; n = n->getParent()) if (!n->isVisible()) return false;
        auto p = this->convertToNodeSpace(t->getLocation());
        if (p.x < -6.f || p.y < -6.f || p.x > m_w + 6.f || p.y > m_h + 6.f) return false;
        m_drag = true;
        this->apply(t);
        return true;
    }
    void ccTouchMoved(CCTouch* t, CCEvent*) override { if (m_drag) this->apply(t); }
    void ccTouchEnded(CCTouch*, CCEvent*) override { m_drag = false; }
    void ccTouchCancelled(CCTouch*, CCEvent*) override { m_drag = false; }
};

// Color picker: saturation/value square, hue strip, Current and Original swatches, RGB, HSV and hex fields
class XyzColorPopup : public geode::Popup {
    static constexpr float SVW = 190.f, SVH = 118.f, HX = 212.f, HW = 16.f, X0 = 14.f, Y0 = 86.f;
    int m_o[3] = {0, 0, 0}, m_c[3] = {0, 0, 0};
    float m_hh = 0.f, m_ss = 0.f, m_vv = 0.f;
    int m_skip = -1; // the field being typed in is not rewritten while typing
    std::function<void(int, int, int)> m_set;
    std::function<void()> m_done;
    CCLayerColor* m_base = nullptr;
    CCDrawNode* m_marks = nullptr;
    CCDrawNode* m_swatch = nullptr;
    TextInput* m_in[7] = {};

    void put(int i, std::string const& s) { if (i != m_skip && m_in[i]) m_in[i]->setString(s, false); }
    void refresh() {
        m_base->setColor(hsv2rgb(m_hh, 1.f, 1.f));
        m_marks->clear();
        float cx = X0 + m_ss * SVW, cy = Y0 + m_vv * SVH;
        disc(m_marks, {cx, cy}, 5.5f, {0, 0, 0, 0}, 2.2f, {0, 0, 0, 0.9f}, 20);
        disc(m_marks, {cx, cy}, 5.5f, {0, 0, 0, 0}, 1.2f, {1, 1, 1, 1.f}, 20);
        float hy = Y0 + (1.f - m_hh / 360.f) * SVH;
        m_marks->drawSegment({HX - 2.f, hy}, {HX + HW + 2.f, hy}, 2.2f, pm({0, 0, 0, 0.9f}));
        m_marks->drawSegment({HX - 2.f, hy}, {HX + HW + 2.f, hy}, 1.1f, pm({1, 1, 1, 1.f}));
        m_swatch->clear();
        rrect(m_swatch, {240.f, 158.f}, {76.f, 30.f}, 3, {m_c[0] / 255.f, m_c[1] / 255.f, m_c[2] / 255.f, 1.f}, 1.f, pGray(0.35f));
        this->put(0, fmt::format("{}", m_c[0]));
        this->put(1, fmt::format("{}", m_c[1]));
        this->put(2, fmt::format("{}", m_c[2]));
        this->put(3, fmt::format("{}", (int)std::lround(m_hh)));
        this->put(4, fmt::format("{}", (int)std::lround(m_ss * 255.f)));
        this->put(5, fmt::format("{}", (int)std::lround(m_vv * 255.f)));
        this->put(6, fmt::format("{:02X}{:02X}{:02X}", m_c[0], m_c[1], m_c[2]));
    }
    void push() {
        if (m_set) m_set(m_c[0], m_c[1], m_c[2]);
        this->refresh();
    }
    void fromRgb() { rgb2hsv(m_c[0], m_c[1], m_c[2], m_hh, m_ss, m_vv); this->push(); }
    void fromHsv() {
        auto c = hsv2rgb(m_hh, m_ss, m_vv);
        m_c[0] = c.r; m_c[1] = c.g; m_c[2] = c.b;
        this->push();
    }
    // a box you type in directly; the value is applied while you type
    void field(char const* prefix, int idx, float x, float y, float w, char const* filter, int maxLen, std::function<void(std::string const&)> onText) {
        auto l = mkLabel(prefix);
        sc(l, .36f); l->setAnchorPoint({0, .5f}); l->setPosition({x + 2.f, y});
        m_mainLayer->addChild(l, 3);
        float iw = w - 14.f;
        auto in = TextInput::create(iw / .55f, "", xyz::draw::fontOf());
        in->setScale(.55f);
        in->setFilter(filter);
        in->setMaxCharCount((size_t)maxLen);
        in->setPosition({x + 14.f + iw / 2.f, y});
        in->setCallback(std::move(onText));
        m_mainLayer->addChild(in, 5);
        m_in[idx] = in;
    }
    std::function<void(std::string const&)> numCb(int idx, double lo, double hi, std::function<void(double)> apply) {
        return [this, idx, lo, hi, apply](std::string const& t) {
            auto r = geode::utils::numFromString<double>(t);
            if (r.isErr()) return;
            m_skip = idx;
            apply(std::clamp(r.unwrap(), lo, hi));
            m_skip = -1;
        };
    }
public:
    static void open(std::string title, int r, int g, int b, std::function<void(int, int, int)> set, std::function<void()> done) {
        auto p = new XyzColorPopup();
        if (p->setup(title, r, g, b, std::move(set), std::move(done))) { p->autorelease(); p->show(); }
        else delete p;
    }
    bool setup(std::string const& title, int r, int g, int b, std::function<void(int, int, int)> set, std::function<void()> done) {
        m_set = std::move(set); m_done = std::move(done);
        m_o[0] = m_c[0] = r; m_o[1] = m_c[1] = g; m_o[2] = m_c[2] = b;
        rgb2hsv(r, g, b, m_hh, m_ss, m_vv);
        m_noElasticity = true;
        if (!Popup::init(330.f, 236.f)) return false;
        if (m_bgSprite) m_bgSprite->setVisible(false);
        if (m_closeBtn) m_closeBtn->setVisible(false);
        auto bg = CCDrawNode::create();
        rrect(bg, {0, 0}, {330.f, 236.f}, 7, pGray(0.115f, 0.99f), 1.f, pGray(0.3f));
        m_mainLayer->addChild(bg, -1);

        auto tl = mkLabel(title.c_str());
        sc(tl, .38f); tl->setAnchorPoint({0, .5f}); tl->setPosition({14.f, 218.f}); tl->setOpacity(220);
        m_mainLayer->addChild(tl);

        auto sv = CCNode::create();
        sv->setPosition({X0, Y0});
        m_mainLayer->addChild(sv);
        m_base = CCLayerColor::create(ccc4(255, 0, 0, 255), SVW, SVH);
        sv->addChild(m_base);
        auto wg = CCLayerGradient::create(ccc4(255, 255, 255, 255), ccc4(255, 255, 255, 0), ccp(1, 0));
        wg->setContentSize({SVW, SVH});
        sv->addChild(wg);
        auto bk = CCLayerGradient::create(ccc4(0, 0, 0, 0), ccc4(0, 0, 0, 255), ccp(0, -1));
        bk->setContentSize({SVW, SVH});
        sv->addChild(bk);

        static ccColor3B const STOPS[7] = {{255, 0, 0}, {255, 255, 0}, {0, 255, 0}, {0, 255, 255}, {0, 0, 255}, {255, 0, 255}, {255, 0, 0}};
        auto hue = CCNode::create();
        hue->setPosition({HX, Y0});
        m_mainLayer->addChild(hue);
        float seg = SVH / 6.f;
        for (int i = 0; i < 6; ++i) {
            auto g2 = CCLayerGradient::create(ccc4(STOPS[i].r, STOPS[i].g, STOPS[i].b, 255), ccc4(STOPS[i + 1].r, STOPS[i + 1].g, STOPS[i + 1].b, 255), ccp(0, -1));
            g2->setContentSize({HW, seg + 0.5f});
            g2->setPosition({0, SVH - (i + 1) * seg});
            hue->addChild(g2);
        }
        auto fr = CCDrawNode::create();
        rrect(fr, {X0, Y0}, {SVW, SVH}, 0.5f, {0, 0, 0, 0}, 1.f, pGray(0.35f));
        rrect(fr, {HX, Y0}, {HW, SVH}, 0.5f, {0, 0, 0, 0}, 1.f, pGray(0.35f));
        m_mainLayer->addChild(fr, 2);

        auto padSv = XyzPad::create(SVW, SVH, [this](float fx, float fy) { m_ss = fx; m_vv = fy; this->fromHsv(); });
        padSv->setPosition({X0, Y0});
        m_mainLayer->addChild(padSv, 4);
        auto padHue = XyzPad::create(HW + 8.f, SVH, [this](float, float fy) { m_hh = std::min((1.f - fy) * 360.f, 359.99f); this->fromHsv(); });
        padHue->setPosition({HX - 4.f, Y0});
        m_mainLayer->addChild(padHue, 4);

        m_marks = CCDrawNode::create();
        m_mainLayer->addChild(m_marks, 3);
        m_swatch = CCDrawNode::create();
        m_mainLayer->addChild(m_swatch, 1);

        auto cl = mkLabel("Current");
        sc(cl, .34f); cl->setAnchorPoint({0, .5f}); cl->setPosition({240.f, 196.f}); cl->setOpacity(200);
        m_mainLayer->addChild(cl);
        auto ol = mkLabel("Original");
        sc(ol, .34f); ol->setAnchorPoint({0, .5f}); ol->setPosition({240.f, 138.f}); ol->setOpacity(200);
        m_mainLayer->addChild(ol);

        auto menu = CCMenu::create();
        menu->setPosition({0, 0});
        m_mainLayer->addChild(menu, 6);
        {
            auto node = CCNode::create();
            node->setContentSize({76.f, 30.f});
            auto d = CCDrawNode::create();
            rrect(d, {0, 0}, {76.f, 30.f}, 3, {r / 255.f, g / 255.f, b / 255.f, 1.f}, 1.f, pGray(0.35f));
            node->addChild(d);
            auto it = CCMenuItemExt::createSpriteExtra(node, [this](auto) {
                m_c[0] = m_o[0]; m_c[1] = m_o[1]; m_c[2] = m_o[2];
                this->fromRgb();
            });
            it->setPosition({240.f + 38.f, 102.f + 15.f});
            menu->addChild(it);
        }
        auto x = makeRound("x", [this] { this->onClose(nullptr); });
        x->setPosition({330.f - 20.f, 218.f});
        menu->addChild(x);

        float fw = 96.f, gap = 7.f;
        this->field("R", 0, X0, 66.f, fw, "0123456789", 3, this->numCb(0, 0, 255, [this](double v) { m_c[0] = (int)std::lround(v); this->fromRgb(); }));
        this->field("G", 1, X0 + fw + gap, 66.f, fw, "0123456789", 3, this->numCb(1, 0, 255, [this](double v) { m_c[1] = (int)std::lround(v); this->fromRgb(); }));
        this->field("B", 2, X0 + 2 * (fw + gap), 66.f, fw, "0123456789", 3, this->numCb(2, 0, 255, [this](double v) { m_c[2] = (int)std::lround(v); this->fromRgb(); }));
        this->field("H", 3, X0, 40.f, fw, "0123456789.", 6, this->numCb(3, 0, 360, [this](double v) { m_hh = std::min((float)v, 359.99f); this->fromHsv(); }));
        this->field("S", 4, X0 + fw + gap, 40.f, fw, "0123456789.", 6, this->numCb(4, 0, 255, [this](double v) { m_ss = (float)v / 255.f; this->fromHsv(); }));
        this->field("V", 5, X0 + 2 * (fw + gap), 40.f, fw, "0123456789.", 6, this->numCb(5, 0, 255, [this](double v) { m_vv = (float)v / 255.f; this->fromHsv(); }));
        this->field("#", 6, X0, 14.f, 3 * fw + 2 * gap, "0123456789abcdefABCDEF#", 7, [this](std::string const& t) {
            std::string v = t;
            while (!v.empty() && v[0] == '#') v.erase(v.begin());
            if (v.size() != 6 || v.find_first_not_of("0123456789abcdefABCDEF") != std::string::npos) return;
            unsigned val = (unsigned)std::stoul(v, nullptr, 16);
            m_c[0] = (int)((val >> 16) & 255); m_c[1] = (int)((val >> 8) & 255); m_c[2] = (int)(val & 255);
            m_skip = 6;
            this->fromRgb();
            m_skip = -1;
        });
        this->refresh();
        return true;
    }
    void onClose(CCObject* o) override {
        auto done = m_done;
        m_done = nullptr;
        Popup::onClose(o);
        if (done) done();
    }
};

static ccColor3B luaColor(std::string const& line) {
    size_t i = line.find_first_not_of(" \t");
    if (i == std::string::npos) return {225, 225, 225};
    if (line.compare(i, 2, "--") == 0) return {110, 165, 110};
    size_t j = i;
    while (j < line.size() && (std::isalnum((unsigned char)line[j]) || line[j] == '_')) ++j;
    auto word = line.substr(i, j - i);
    static char const* KW[] = {"function", "end", "if", "then", "else", "elseif", "for", "while", "do", "local", "return", "repeat", "until", "break"};
    for (auto k : KW) if (word == k) return {125, 175, 255};
    return {225, 225, 225};
}

// Built-in Lua editor: the script is shown line by line, tap a line to type in it directly
class XyzLuaEditor : public geode::Popup {
    static constexpr float LH = 16.f;
    std::string m_name;
    std::vector<std::string> m_lines;
    bool m_dirty = false, m_force = false;
    int m_sel = -1;
    ScrollLayer* m_scroll = nullptr;
    CCLabelBMFont* m_status = nullptr;
    CCLabelBMFont* m_title = nullptr;
    float m_w = 0.f, m_h = 0.f;

    void load() {
        std::ifstream f(lua::dir() / m_name, std::ios::binary);
        std::string all((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        m_lines.clear();
        std::string cur;
        for (char ch : all) {
            if (ch == '\n') { m_lines.push_back(cur); cur.clear(); }
            else if (ch != '\r') cur += ch;
        }
        if (!cur.empty() || m_lines.empty()) m_lines.push_back(cur);
    }
    bool writeOut() {
        std::string all;
        for (auto& l : m_lines) { all += l; all += '\n'; }
        std::ofstream o(lua::dir() / m_name, std::ios::binary | std::ios::trunc);
        if (!o) return false;
        o.write(all.data(), (std::streamsize)all.size());
        return (bool)o;
    }
    void titleNow() { m_title->setString((m_name + (m_dirty ? " *" : "")).c_str()); }
    void say(std::string const& t) { m_status->setString(t.c_str()); fit(m_status, m_w - 28.f, .3f, .15f); }
    void save() {
        if (!this->writeOut()) { this->say("Could not save the script"); return; }
        m_dirty = false;
        this->titleNow();
        this->say("Saved");
    }
    void run() {
        if (!this->writeOut()) { this->say("Could not save the script"); return; }
        m_dirty = false;
        this->titleNow();
        lua::setEnabled(m_name, true);
        lua::reload();
        auto out = lua::output();
        std::string msg = lua::status();
        if (!out.empty()) msg += "  |  " + out.back();
        this->say(msg);
    }
    void rebuild(bool keep) {
        auto content = m_scroll->m_contentLayer;
        float viewH = m_scroll->getContentSize().height, viewW = m_scroll->getContentSize().width;
        float oldY = content->getPositionY();
        content->removeAllChildren();
        float total = std::max(viewH, (float)m_lines.size() * LH + 8.f);
        content->setContentSize({viewW, total});
        auto stripes = CCDrawNode::create();
        content->addChild(stripes);
        auto menu = CCMenu::create();
        menu->setPosition({0, 0});
        content->addChild(menu);
        float avail = viewW - 44.f;
        for (size_t i = 0; i < m_lines.size(); ++i) {
            float cy = total - 4.f - ((float)i + .5f) * LH;
            bool sel = (int)i == m_sel;
            if (sel) poly(stripes, {{0, cy - LH / 2.f}, {viewW, cy - LH / 2.f}, {viewW, cy + LH / 2.f}, {0, cy + LH / 2.f}}, accL(0.18f));
            else if (i % 2 == 0) poly(stripes, {{0, cy - LH / 2.f}, {viewW, cy - LH / 2.f}, {viewW, cy + LH / 2.f}, {0, cy + LH / 2.f}}, {1, 1, 1, 0.035f});
            auto num = mkLabel(std::to_string(i + 1).c_str());
            sc(num, .3f); num->setAnchorPoint({1.f, .5f}); num->setPosition({30.f, cy}); num->setOpacity(sel ? 220 : 110);
            content->addChild(num, 1);
            int idx = (int)i;
            if (sel) {
                auto in = TextInput::create(avail / .55f, "code", xyz::draw::fontOf());
                in->setScale(.55f);
                in->setMaxCharCount(500);
                in->setString(m_lines[i], false);
                in->setPosition({38.f + avail / 2.f, cy});
                in->setCallback([this, idx](std::string const& t) {
                    if (idx < 0 || idx >= (int)m_lines.size()) return;
                    m_lines[idx] = t;
                    if (!m_dirty) { m_dirty = true; this->titleNow(); }
                });
                menu->addChild(in);
                continue;
            }
            std::string shown = m_lines[i];
            for (auto& ch : shown) if (ch == '\t') ch = ' ';
            if (shown.empty()) shown = " ";
            auto l = mkLabel(shown.c_str());
            sc(l, .3f);
            while (l->getScaledContentSize().width > avail && shown.size() > 6) {
                shown.resize(shown.size() - std::max<size_t>(2, shown.size() / 10));
                l->setString((shown + "...").c_str());
            }
            l->setAnchorPoint({0, .5f}); l->setPosition({0, LH / 2.f});
            l->setColor(luaColor(m_lines[i]));
            auto node = CCNode::create();
            node->setContentSize({viewW, LH});
            node->addChild(l);
            l->setPositionX(38.f);
            auto it = CCMenuItemExt::createSpriteExtra(node, [this, idx](auto) {
                m_sel = idx;
                this->rebuild(true);
            });
            it->m_scaleMultiplier = 1.f;
            it->setPosition({viewW / 2.f, cy});
            menu->addChild(it);
        }
        if (keep) content->setPositionY(std::clamp(oldY, viewH - total, 0.f));
        else content->setPositionY(viewH - total);
    }
public:
    static void open(std::string name) {
        auto p = new XyzLuaEditor();
        if (p->setup(std::move(name))) { p->autorelease(); p->show(); }
        else delete p;
    }
    bool setup(std::string name) {
        m_name = std::move(name);
        auto ws = CCDirector::get()->getWinSize();
        m_w = std::min(ws.width - 16.f, 540.f);
        m_h = std::min(ws.height - 12.f, 300.f);
        m_noElasticity = true;
        if (!Popup::init(m_w, m_h)) return false;
        if (m_bgSprite) m_bgSprite->setVisible(false);
        if (m_closeBtn) m_closeBtn->setVisible(false);
        this->load();
        auto bg = CCDrawNode::create();
        rrect(bg, {0, 0}, {m_w, m_h}, 7, pGray(0.095f, 0.99f), 1.f, pGray(0.3f));
        bg->drawSegment({1.f, m_h - 32.f}, {m_w - 1.f, m_h - 32.f}, 0.5f, pm(pGray(0.28f)));
        bg->drawSegment({1.f, 22.f}, {m_w - 1.f, 22.f}, 0.5f, pm(pGray(0.22f)));
        m_mainLayer->addChild(bg, -1);

        m_title = mkLabelBold(m_name.c_str());
        sc(m_title, .4f); m_title->setAnchorPoint({0, .5f}); m_title->setPosition({14.f, m_h - 16.f});
        m_mainLayer->addChild(m_title);
        m_status = mkLabel("Tap a line, then type in it");
        sc(m_status, .3f); m_status->setAnchorPoint({0, .5f}); m_status->setPosition({14.f, 11.f}); m_status->setOpacity(190);
        m_mainLayer->addChild(m_status);

        m_scroll = ScrollLayer::create({m_w - 20.f, m_h - 32.f - 22.f - 6.f});
        m_scroll->setPosition({10.f, 25.f});
        m_mainLayer->addChild(m_scroll);

        auto menu = CCMenu::create();
        menu->setPosition({0, 0});
        m_mainLayer->addChild(menu, 5);
        auto x = makeRound("x", [this] { this->onClose(nullptr); });
        x->setPosition({m_w - 20.f, m_h - 16.f});
        menu->addChild(x);
        float px = m_w - 40.f;
        auto add = [&](char const* t, bool accent, std::function<void()> f) {
            auto b = makePill(t, accent, std::move(f));
            float w = b->getContentSize().width;
            b->setPosition({px - w / 2.f, m_h - 16.f});
            menu->addChild(b);
            px -= w + 6.f;
        };
        add("Run", true, [this] { this->run(); });
        add("Save", false, [this] { this->save(); });
        add("Add Line", false, [this] {
            int at = m_sel >= 0 && m_sel < (int)m_lines.size() ? m_sel + 1 : (int)m_lines.size();
            m_lines.insert(m_lines.begin() + at, std::string());
            m_sel = at;
            m_dirty = true;
            this->titleNow();
            this->rebuild(true);
        });
        add("Del Line", false, [this] {
            if (m_sel < 0 || m_sel >= (int)m_lines.size()) return;
            if (m_lines.size() > 1) m_lines.erase(m_lines.begin() + m_sel);
            else m_lines[0].clear();
            m_sel = std::min(m_sel, (int)m_lines.size() - 1);
            m_dirty = true;
            this->titleNow();
            this->rebuild(true);
        });
        this->rebuild(false);
        return true;
    }
    void onClose(CCObject* o) override {
        if (m_dirty && !m_force) {
            Ref<XyzLuaEditor> self = this;
            geode::createQuickPopup("Unsaved changes", "Save this script before closing?", "Discard", "Save", [self](auto*, bool saveIt) {
                if (saveIt) self->save();
                self->m_force = true;
                self->onClose(nullptr);
            });
            return;
        }
        Popup::onClose(o);
    }
};

static void openBugReport() {
    geode::createQuickPopup("Report a bug", "Open the xyzBot issue tracker on GitHub?", "No", "Open", [](auto*, bool open) {
        if (open) geode::utils::web::openLinkInBrowser("https://github.com/malikisherenot/xyzBot/issues");
    });
}

static char const* TABS[] = {"Bot", "Physics", "Visuals", "Macros", "Labels", "Assist", "Render", "Scripts", "Clickbot", "GUI"};
static constexpr int NTABS = 10;
static char const* TAB_ICONS[] = {"tab-bot.png", "tab-physics.png", "tab-visuals.png", "tab-macros.png", "tab-labels.png", "tab-assist.png", "tab-render.png", "tab-scripts.png", "tab-clickbot.png", "tab-gui.png"};
static CCSprite* tabIcon(int i, float size) {
    auto s = CCSprite::create(Mod::get()->expandSpriteName(TAB_ICONS[i]).c_str());
    if (!s) return nullptr;
    s->setScale(size / std::max(1.f, s->getContentSize().width));
    return s;
}
static char const* PLAYERS[] = {"Both", "Player 1 only", "Player 2 only"};
static char const* ACCURACY[] = {"Vanilla", "Input Fixes", "Frame Fixes", "Delta Accuracy"};
static float tabPitch(float menuH) { return std::min(29.f, (menuH - 52.f - 8.f) / NTABS); }

// opens a folder in the file manager; where that does not work (Wine and Proton on Linux often can't) the path is shown instead
static void openFolderOrShow(std::filesystem::path const& p) {
    if (!utils::file::openFolder(p))
        Notification::create("Could not open the folder. It is here: " + utils::string::pathToString(p), NotificationIcon::Warning)->show();
}

XyzMenu* XyzMenu::create() {
    auto r = new XyzMenu();
    if (r->init()) { r->autorelease(); return r; }
    delete r;
    return nullptr;
}

XyzMenu::~XyzMenu() {
    if (s_open == this) s_open = nullptr;
    rt().menuOpen = false;
}

void XyzMenu::toggle() {
    if (s_open && s_open->getParent()) { s_open->onClose(nullptr); return; }
    s_open = nullptr;
    s_open = create();
    if (s_open) s_open->show();
}

void XyzMenu::restyle() {
    s_startTab = m_tab;
    this->onClose(nullptr);
    queueInMainThread([] { XyzMenu::toggle(); });
}

void XyzMenu::show() {
    Popup::show();
    if (m_paper) {
        // the Paper window floats over the game without dimming it
        this->stopAllActions();
        this->setOpacity(0);
    }
}

void XyzMenu::noAnim(CCNode* n) {
    if (cfg().animations || !n) return;
    if (auto b = typeinfo_cast<CCMenuItemSpriteExtra*>(n)) b->m_scaleMultiplier = 1.f;
    if (n->getChildren()) {
        for (auto c : CCArrayExt<CCNode*>(n->getChildren())) this->noAnim(c);
    }
}

void XyzMenu::drawPanel() {
    if (!m_panel) return;
    m_panel->clear();
    if (m_glow) m_glow->clear();
    float h = m_collapsed ? P_TITLE : m_h, y0 = m_h - h;
    rrect(m_panel, {0, y0}, {m_w, h}, 9, pWin(), 1.2f, pGray(0.3f));
    if (m_glow && m_drag == 2) {
        // only the side being dragged lights up, in the accent color
        auto a = acc();
        for (int side = 0; side < 4; ++side) {
            if (!(m_edge & (1 << side))) continue;
            bool horiz = side >= 2; // bottom, top
            float base = side == 0 ? 0.f : side == 1 ? m_w : side == 2 ? y0 : m_h;
            float dir = (side == 0 || side == 2) ? -1.f : 1.f;
            float lo = horiz ? 9.f : y0 + 9.f, hi = horiz ? m_w - 9.f : m_h - 9.f;
            for (int i = 0; i <= 12; ++i) {
                float t = 1.f - (float)i / 13.f, off = base + dir * (float)i;
                ccColor4F col = {a.r, a.g, a.b, (i == 0 ? 1.f : 0.34f * t * t)};
                if (horiz) m_glow->drawSegment({lo, off}, {hi, off}, i == 0 ? 1.4f : 1.f, col);
                else m_glow->drawSegment({off, lo}, {off, hi}, i == 0 ? 1.4f : 1.f, col);
            }
        }
    }
    if (!m_collapsed) {
        m_panel->drawSegment({1.f, m_h - P_TITLE}, {m_w - 1.f, m_h - P_TITLE}, 0.5f, pm(pGray(0.26f)));
        m_panel->drawSegment({1.f, P_FOOT}, {m_w - 1.f, P_FOOT}, 0.5f, pm(pGray(0.2f)));
    }
}

void XyzMenu::collapse() {
    m_collapsed = !m_collapsed;
    if (m_arrow) {
        if (cfg().animations) m_arrow->runAction(CCEaseExponentialOut::create(CCRotateTo::create(.12f, m_collapsed ? -90.f : 0.f)));
        else m_arrow->setRotation(m_collapsed ? -90.f : 0.f);
    }
    this->drawPanel();
    m_scroll->setVisible(!m_collapsed);
    m_scroll->setTouchEnabled(!m_collapsed);
    if (m_tabScroll) { m_tabScroll->setVisible(!m_collapsed); m_tabScroll->setTouchEnabled(!m_collapsed); }
    m_side->setVisible(!m_collapsed);
}

void XyzMenu::defer(std::function<void()> f) {
    Ref<XyzMenu> self = this;
    queueInMainThread([self, f] { if (s_open == self.data()) f(); });
}
void XyzMenu::later(bool animate) { this->defer([this, animate] { this->rebuild(animate); }); }

void XyzMenu::themeLive() {
    if (m_glass) glass::setTheme(m_glass, to3(acc()), tint3());
    this->drawPanel();
    this->buildSide();
}
void XyzMenu::themeFull() { this->themeLive(); this->rebuild(false); }

void XyzMenu::saveRect() {
    if (!m_paper) return;
    auto o = m_mainLayer->convertToWorldSpace({0.f, 0.f});
    auto& c = cfg();
    c.winX = o.x; c.winY = o.y; c.winW = m_w; c.winH = m_h;
    saveCfg();
}

// keeps the title bar reachable
static void keepOnScreen(CCNode* layer, float w, float h) {
    auto ws = CCDirector::get()->getWinSize();
    auto o = layer->convertToWorldSpace({0.f, 0.f});
    CCPoint n = {std::clamp(o.x, 90.f - w, ws.width - 90.f), std::clamp(o.y, 10.f + P_TITLE - h, ws.height - P_TITLE)};
    if (n.x != o.x || n.y != o.y) layer->setPosition(layer->getPosition() + (n - o));
}

bool XyzMenu::init() {
    auto ws = CCDirector::get()->getWinSize();
    auto& cf = cfg();
    m_paper = paper();
    m_tab = std::clamp(s_startTab, 0, NTABS - 1);
    s_startTab = 0;
    if (m_paper) {
        m_w = cf.winW > 0.f ? cf.winW : std::min(ws.width - 30.f, 500.f);
        m_h = cf.winH > 0.f ? cf.winH : std::min(ws.height - 24.f, 290.f);
        m_w = std::clamp(m_w, P_MINW, std::max(P_MINW, ws.width - 4.f));
        m_h = std::clamp(m_h, P_MINH, std::max(P_MINH, ws.height - 4.f));
        m_cw = m_w - 24.f;
        m_ch = m_h - P_TITLE - P_TABS - P_FOOT - 8.f;
    } else {
        m_w = std::min(ws.width - 30.f, 470.f);
        m_h = std::min(ws.height - 24.f, 280.f);
        m_cw = m_w - SW - 30.f;
        m_ch = m_h - 52.f;
    }
    m_glass = m_paper ? nullptr : glass::create({m_w, m_h}, 16.f);
    m_noElasticity = true;
    if (!Popup::init(m_w, m_h)) return false;
    if (m_bgSprite) m_bgSprite->setVisible(false);
    if (m_closeBtn) m_closeBtn->setVisible(false);

    if (m_glass) {
        this->addChild(m_glass, -5);
        m_glass->setPosition(ws / 2);
        glass::setAlpha(m_glass, 0.f);
        glass::setTheme(m_glass, to3(acc()), tint3());
    } else if (m_paper) {
        m_glow = CCDrawNode::create();
        m_mainLayer->addChild(m_glow, -3);
        m_panel = CCDrawNode::create();
        m_mainLayer->addChild(m_panel, -1);
        this->drawPanel();
        if (cf.winX > -50000.f) {
            auto o = m_mainLayer->convertToWorldSpace({0.f, 0.f});
            m_mainLayer->setPosition(m_mainLayer->getPosition() + (CCPoint{cf.winX, cf.winY} - o));
        }
        keepOnScreen(m_mainLayer, m_w, m_h);
    } else {
        auto d = CCDrawNode::create();
        auto t = tint3();
        rrect(d, {0, 0}, {m_w, m_h}, 16, {t.r / 255.f, t.g / 255.f, t.b / 255.f, 0.9f}, 1.2f, accL(0.5f));
        m_mainLayer->addChild(d, -1);
    }

    m_head = CCNode::create();
    m_mainLayer->addChild(m_head, 5);
    this->buildHead();

    m_scroll = ScrollLayer::create({m_cw, m_ch});
    m_scroll->setPosition(m_paper ? CCPoint{12.f, P_FOOT + 4.f} : CCPoint{SW + 20.f, 12.f});
    m_mainLayer->addChild(m_scroll);

    if (m_paper) {
        m_tabScroll = ScrollLayer::create(CCSize{m_w - 24.f, 26.f}, true, false);
        m_mainLayer->addChild(m_tabScroll, 2);
    }

    m_side = CCNode::create();
    m_mainLayer->addChild(m_side);

    s_open = this;
    rt().menuOpen = true;
    this->buildSide();
    this->rebuild(true);

    if (!m_paper) {
        auto dm = CCMenu::create();
        dm->setPosition({0, 0});
        m_mainLayer->addChild(dm, 20);
        auto db = makePill("Report a bug", false, [] { openBugReport(); });
        db->setPosition({ws.width - 10.f - db->getContentWidth() / 2.f - (ws.width - m_w) / 2.f, 18.f - (ws.height - m_h) / 2.f});
        dm->addChild(db);
    }

    if (cf.animations) {
        m_mainLayer->setScale(.97f);
        m_mainLayer->setPositionY(m_mainLayer->getPositionY() - 4.f);
        m_mainLayer->runAction(CCEaseExponentialOut::create(CCScaleTo::create(.14f, 1.f)));
        m_mainLayer->runAction(CCEaseExponentialOut::create(CCMoveBy::create(.14f, {0, 4.f})));
    }
    if (m_paper) this->schedule(schedule_selector(XyzMenu::paperStep));
    else if (cf.animations) this->schedule(schedule_selector(XyzMenu::fadeStep));
    else if (m_glass) glass::setAlpha(m_glass, 1.f);
    this->noAnim(m_mainLayer);
    return true;
}

void XyzMenu::buildHead() {
    m_head->removeAllChildren();
    if (m_paper) {
        m_arrow = nullptr;
        auto title = mkLabelBold(fmt::format("xyzBot v{}", Mod::get()->getVersion().toVString().substr(1)).c_str());
        sc(title, .42f); title->setPosition({m_w / 2, m_h - P_TITLE / 2.f}); title->setOpacity(215);
        m_head->addChild(title);
        auto top = CCMenu::create();
        top->setPosition({0, 0});
        m_head->addChild(top);
        auto an = CCNode::create();
        an->setContentSize({22, 22});
        auto tri = CCDrawNode::create();
        poly(tri, {{-5.f, 3.5f}, {5.f, 3.5f}, {0.f, -4.f}}, pGray(0.85f));
        tri->setPosition({11, 11});
        tri->setRotation(m_collapsed ? -90.f : 0.f);
        an->addChild(tri);
        m_arrow = tri;
        auto ab = CCMenuItemExt::createSpriteExtra(an, [this](auto) { this->collapse(); });
        ab->setPosition({20, m_h - P_TITLE / 2.f});
        top->addChild(ab);
        auto x = makeRound("x", [this] { this->onClose(nullptr); });
        x->setPosition({m_w - 20, m_h - P_TITLE / 2.f});
        top->addChild(x);
        this->noAnim(m_head);
        return;
    }
    {
        auto bd = CCDrawNode::create();
        rrect(bd, {1, m_h - 38.f}, {m_w - 2.f, 37.f}, 15, ccc4f(1, 1, 1, 0.05f), 0.f, {0, 0, 0, 0});
        bd->drawSegment({2, m_h - 38.f}, {m_w - 2.f, m_h - 38.f}, 0.5f, pm(ccc4f(1, 1, 1, 0.09f)));
        bd->drawSegment({SW + 14.f, 10.f}, {SW + 14.f, m_h - 42.f}, 0.5f, pm(ccc4f(1, 1, 1, 0.09f)));
        m_head->addChild(bd, -1);
    }
    auto title = mkLabelBold(fmt::format("xyzBot v{}", Mod::get()->getVersion().toVString().substr(1)).c_str());
    sc(title, .45f); title->setOpacity(225); title->setPosition({m_w / 2, m_h - 20});
    m_head->addChild(title);
    auto top = CCMenu::create();
    top->setPosition({0, 0});
    m_head->addChild(top);
    auto x = makeRound("x", [this] { this->onClose(nullptr); });
    x->setPosition({m_w - 20, m_h - 20});
    top->addChild(x);
    this->noAnim(m_head);
}

void XyzMenu::applyFont() {
    this->buildHead();
    this->buildSide();
    this->rebuild(false);
}

void XyzMenu::fadeStep(float dt) {
    m_fade = std::clamp(m_fade + dt * (m_closing ? -8.f : 6.f), 0.f, 1.f);
    if (m_glass) glass::setAlpha(m_glass, m_fade);
    if (!m_closing && m_fade >= 1.f) this->unschedule(schedule_selector(XyzMenu::fadeStep));
}

void XyzMenu::relayout(bool rows) {
    m_cw = m_w - 24.f;
    m_ch = m_h - P_TITLE - P_TABS - P_FOOT - 8.f;
    this->drawPanel();
    this->buildHead();
    this->buildSide();
    m_scroll->setContentSize({m_cw, m_ch});
    m_scroll->setPosition({12.f, P_FOOT + 4.f});
    if (rows) this->rebuild(false);
}

void XyzMenu::paperStep(float dt) {
    if (m_closing) return;
    m_geoClock += dt;
    if (m_geoDirty && m_geoClock > .07f) {
        m_geoClock = 0.f;
        m_geoDirty = false;
        this->relayout(true);
    }
    static float t = 0.f;
    t += dt;
    if (t < .1f) return;
    t = 0.f;
    if (m_footer) {
        m_footer->setString(fmt::format("Frame: {}  TPS: {:.6f}", rt().tick, (double)curTps()).c_str());
        fit(m_footer, m_w * 0.6f, .32f, .15f);
    }
    if (m_tabScroll && m_tabBar) {
        m_tabBar->clear();
        float vw = m_w - 24.f, tw = m_tabScroll->m_contentLayer->getContentSize().width;
        if (tw > vw + 1.f) {
            float px = -m_tabScroll->m_contentLayer->getPositionX(), room = tw - vw;
            float thumb = std::max(24.f, vw * vw / tw), tx = std::clamp(px / room, 0.f, 1.f) * (vw - thumb);
            m_tabBar->drawSegment({0.f, 0.f}, {vw, 0.f}, 1.2f, pm(pGray(0.2f)));
            m_tabBar->drawSegment({tx, 0.f}, {tx + thumb, 0.f}, 1.2f, pm(accL(0.9f)));
        }
    }
}

void XyzMenu::onClose(CCObject*) {
    if (m_closing) return;
    m_closing = true;
    s_open = nullptr;
    rt().menuOpen = false;
    this->saveRect();
    m_mainLayer->stopAllActions();
    if (!cfg().animations) { Popup::onClose(nullptr); return; }
    m_mainLayer->runAction(CCEaseExponentialIn::create(CCScaleTo::create(.08f, .97f)));
    if (!m_paper) this->schedule(schedule_selector(XyzMenu::fadeStep));
    this->scheduleOnce(schedule_selector(XyzMenu::doClose), .09f);
}
void XyzMenu::doClose(float) { Popup::onClose(nullptr); }

bool XyzMenu::ccTouchBegan(CCTouch* t, CCEvent* e) {
    m_drag = 0;
    if (m_paper && !m_closing) {
        auto lp = m_mainLayer->convertToNodeSpace(t->getLocation());
        float y0 = m_collapsed ? m_h - P_TITLE : 0.f;
        constexpr float E = 8.f, O = 7.f;
        bool in = lp.x >= -O && lp.x <= m_w + O && lp.y >= y0 - O && lp.y <= m_h + O;
        if (in) {
            int edge = 0;
            if (lp.x < E) edge |= 1;
            if (lp.x > m_w - E) edge |= 2;
            if (!m_collapsed) {
                if (lp.y < y0 + E) edge |= 4;
                if (lp.y > m_h - E) edge |= 8;
            }
            if (edge) m_drag = 2;
            else if (lp.y > m_h - P_TITLE && lp.x > 44.f && lp.x < m_w - 44.f) m_drag = 1;
            if (m_drag) {
                m_edge = edge;
                m_dragFrom = t->getLocation();
                if (m_drag == 2) this->drawPanel();
                m_startPos = m_mainLayer->getPosition();
                m_startSize = CCSize(m_w, m_h);
                return true;
            }
        }
    }
    return Popup::ccTouchBegan(t, e);
}

void XyzMenu::ccTouchMoved(CCTouch* t, CCEvent* e) {
    if (!m_drag) { Popup::ccTouchMoved(t, e); return; }
    auto d = t->getLocation() - m_dragFrom;
    if (m_drag == 1) {
        m_mainLayer->setPosition(m_startPos + d);
        keepOnScreen(m_mainLayer, m_w, m_collapsed ? P_TITLE : m_h);
        return;
    }
    auto ws = CCDirector::get()->getWinSize();
    float nw = m_startSize.width, nh = m_startSize.height;
    CCPoint np = m_startPos;
    if (m_edge & 1) { nw -= d.x; np.x += d.x; }
    if (m_edge & 2) nw += d.x;
    if (m_edge & 4) { nh -= d.y; np.y += d.y; }
    if (m_edge & 8) nh += d.y;
    float maxW = std::max(P_MINW, ws.width - 4.f), maxH = std::max(P_MINH, ws.height - 4.f);
    if (nw < P_MINW) { if (m_edge & 1) np.x -= P_MINW - nw; nw = P_MINW; }
    if (nw > maxW) { if (m_edge & 1) np.x += nw - maxW; nw = maxW; }
    if (nh < P_MINH) { if (m_edge & 4) np.y -= P_MINH - nh; nh = P_MINH; }
    if (nh > maxH) { if (m_edge & 4) np.y += nh - maxH; nh = maxH; }
    if (nw == m_w && nh == m_h && np.x == m_mainLayer->getPositionX() && np.y == m_mainLayer->getPositionY()) return;
    m_w = nw; m_h = nh;
    m_mainLayer->setPosition(np);
    this->drawPanel();
    m_geoDirty = true;
}

void XyzMenu::ccTouchEnded(CCTouch* t, CCEvent* e) {
    if (!m_drag) { Popup::ccTouchEnded(t, e); return; }
    bool resized = m_drag == 2;
    m_drag = 0;
    if (resized) { m_geoDirty = false; this->relayout(true); }
    this->saveRect();
}

void XyzMenu::ccTouchCancelled(CCTouch* t, CCEvent* e) {
    if (!m_drag) { Popup::ccTouchCancelled(t, e); return; }
    this->ccTouchEnded(t, e);
}

void XyzMenu::scrollTabs() {
    if (!m_tabScroll || m_tab >= (int)m_tabX.size()) return;
    auto content = m_tabScroll->m_contentLayer;
    float vw = m_w - 24.f, tw = content->getContentSize().width;
    if (tw <= vw) { content->setPositionX(0.f); return; }
    float want = -(m_tabX[m_tab] + m_tabW[m_tab] / 2.f - vw / 2.f);
    content->setPositionX(std::clamp(want, vw - tw, 0.f));
}

void XyzMenu::moveHl(bool animate) {
    auto paintIcons = [&](GLubyte dim) {
        for (int i = 0; i < (int)m_tabIcons.size(); ++i) {
            auto ic = m_tabIcons[i];
            if (!ic) continue;
            bool sel = i == m_tab;
            ic->stopAllActions();
            ic->setColor(sel ? to3(accL()) : ccColor3B{255, 255, 255});
            GLubyte op = sel ? 255 : dim;
            if (animate && cfg().animations) ic->runAction(CCFadeTo::create(.08f, op));
            else ic->setOpacity(op);
        }
    };
    if (m_paper) {
        for (int i = 0; i < (int)m_tabBg.size() && i < (int)m_tabW.size(); ++i) {
            bool sel = i == m_tab;
            m_tabBg[i]->clear();
            rrect(m_tabBg[i], {0, 0}, {m_tabW[i], 24.f}, 4, sel ? pGray(0.32f) : pGray(0.16f), 1.f, sel ? pGray(0.52f) : pGray(0.24f));
        }
        for (int i = 0; i < (int)m_tabLabels.size(); ++i) {
            m_tabLabels[i]->stopAllActions();
            if (animate && cfg().animations) m_tabLabels[i]->runAction(CCFadeTo::create(.08f, i == m_tab ? 255 : 190));
            else m_tabLabels[i]->setOpacity(i == m_tab ? 255 : 190);
        }
        paintIcons(190);
        this->scrollTabs();
        return;
    }
    if (!m_hl) return;
    float h2 = m_h - 52.f;
    CCPoint p = {10.f, 12.f + h2 - 6.f - 26.f - m_tab * tabPitch(m_h)};
    m_hl->stopAllActions();
    if (animate && cfg().animations) m_hl->runAction(CCEaseExponentialOut::create(CCMoveTo::create(.15f, p)));
    else m_hl->setPosition(p);
    for (int i = 0; i < (int)m_tabLabels.size(); ++i) {
        m_tabLabels[i]->stopAllActions();
        if (cfg().animations) m_tabLabels[i]->runAction(CCFadeTo::create(.08f, i == m_tab ? 255 : 150));
        else m_tabLabels[i]->setOpacity(i == m_tab ? 255 : 150);
    }
    paintIcons(150);
}

void XyzMenu::buildSide() {
    m_side->removeAllChildren();
    m_tabLabels.clear();
    m_tabIcons.clear();
    m_tabBg.clear();
    m_footer = nullptr;
    m_tabBar = nullptr;
    if (m_paper) {
        m_tabX.clear(); m_tabW.clear();
        float vw = m_w - 24.f;
        m_tabScroll->setContentSize({vw, 26.f});
        m_tabScroll->setPosition({12.f, m_h - P_TITLE - 31.f});
        auto content = m_tabScroll->m_contentLayer;
        float keepX = content->getPositionX();
        content->removeAllChildren();
        auto menu = CCMenu::create();
        menu->setPosition({0, 0});
        content->addChild(menu);
        float x = 0.f;
        for (int i = 0; i < NTABS; ++i) {
            auto l = mkLabelBold(TABS[i]);
            sc(l, .36f);
            auto ic = tabIcon(i, 12.f);
            float lw = l->getScaledContentSize().width, gap = ic ? 17.f : 0.f;
            float w = std::max(46.f, lw + gap + 20.f);
            auto node = CCNode::create();
            node->setContentSize({w, 24.f});
            auto bg = CCDrawNode::create();
            node->addChild(bg);
            m_tabBg.push_back(bg);
            float x0 = (w - (lw + gap)) / 2.f;
            if (ic) { ic->setPosition({x0 + 6.f, 12.f}); node->addChild(ic, 1); }
            m_tabIcons.push_back(ic);
            l->setAnchorPoint({0, .5f});
            l->setPosition({x0 + gap, 12.f});
            node->addChild(l, 1);
            m_tabLabels.push_back(l);
            m_tabX.push_back(x); m_tabW.push_back(w);
            auto b = CCMenuItemExt::createSpriteExtra(node, [this, i](auto) {
                if (m_tab == i && m_sub.empty()) return;
                m_tab = i; m_sel = -1; m_sub.clear();
                this->moveHl(true);
                this->later(true);
            });
            b->setPosition({x + w / 2, 13.f});
            menu->addChild(b);
            x += w + 4.f;
        }
        float total = std::max(x - 4.f, vw);
        content->setContentSize({total, 26.f});
        content->setPositionX(std::clamp(keepX, vw - total, 0.f));

        m_tabBar = CCDrawNode::create();
        m_tabBar->setPosition({12.f, m_h - P_TITLE - P_TABS + 3.f});
        m_side->addChild(m_tabBar);

        m_footer = mkLabel(fmt::format("Frame: {}  TPS: {:.6f}", rt().tick, (double)curTps()).c_str());
        sc(m_footer, .32f); m_footer->setAnchorPoint({0, .5f}); m_footer->setPosition({12.f, P_FOOT / 2.f}); m_footer->setOpacity(190);
        m_side->addChild(m_footer);
        auto fm = CCMenu::create();
        fm->setPosition({0, 0});
        m_side->addChild(fm, 2);
        auto bug = makePill("Report a bug", false, [] { openBugReport(); });
        bug->setPosition({m_w - 12.f - bug->getContentWidth() / 2.f, P_FOOT / 2.f});
        fm->addChild(bug);
        this->moveHl(false);
        this->noAnim(m_side);
        this->noAnim(m_tabScroll);
        return;
    }
    float h2 = m_h - 52.f;
    auto hl = CCDrawNode::create();
    rrect(hl, {0, 0}, {SW + 4.f, 26}, 6, ccc4f(1, 1, 1, 0.09f), 0.f, {0, 0, 0, 0});
    rrect(hl, {SW + 1.f, 3}, {3, 20}, 1.5f, accL(1.f), 0.f, {0, 0, 0, 0});
    m_side->addChild(hl);
    m_hl = hl;
    auto menu = CCMenu::create();
    menu->setPosition({0, 0});
    m_side->addChild(menu, 2);
    for (int i = 0; i < NTABS; ++i) {
        auto node = CCNode::create();
        node->setContentSize({SW + 4.f, 26});
        auto l = mkLabelBold(TABS[i]);
        auto ic = tabIcon(i, 13.f);
        if (ic) { ic->setPosition({19.f, 13}); node->addChild(ic); }
        m_tabIcons.push_back(ic);
        sc(l, .4f); l->setAnchorPoint({0, .5f}); l->setPosition({ic ? 33.f : 12.f, 13});
        node->addChild(l);
        m_tabLabels.push_back(l);
        auto b = CCMenuItemExt::createSpriteExtra(node, [this, i](auto) {
            if (m_tab == i && m_sub.empty()) return;
            m_tab = i; m_sel = -1; m_sub.clear();
            this->moveHl(true);
            this->later(true);
        });
        b->setPosition({10 + (SW + 4.f) / 2, 12.f + h2 - 6.f - 13.f - i * tabPitch(m_h)});
        menu->addChild(b);
    }
    this->moveHl(false);
    this->noAnim(m_side);
}

namespace {
struct Row {
    enum K { Section, Toggle, Num, Pills, Text, Custom } k = Text;
    std::string label;
    bool on = false;
    std::string gear;
    std::string help;
    std::function<void(bool)> setb;
    std::function<std::string()> get;
    std::function<void(int)> step;
    std::function<void(double)> setv;
    std::vector<std::pair<std::string, std::function<void()>>> pills;
    std::function<void(CCMenu*, float, float)> custom;
    double lo = 0, hi = 0;
    std::function<double()> val;
    std::vector<std::string> opts;
    std::function<int()> cur;
    std::function<void(int)> pick;
};
}

void XyzMenu::rebuild(bool animate) {
    animate = animate && cfg().animations;
    auto content = m_scroll->m_contentLayer;
    content->removeAllChildren();
    auto& c = cfg(); auto& r = rt();
    std::vector<Row> rows;

    auto sec = [&](std::string t) { Row x; x.k = Row::Section; x.label = std::move(t); rows.push_back(std::move(x)); };
    auto tog = [&](std::string t, bool v, std::function<void(bool)> f, std::string gear = "") { Row x; x.k = Row::Toggle; x.label = std::move(t); x.on = v; x.setb = std::move(f); x.gear = std::move(gear); x.help = helpText(x.label); rows.push_back(std::move(x)); };
    auto numS = [&](std::string t, std::function<std::string()> g, std::function<void(int)> s, std::function<void(double)> sv) { Row x; x.k = Row::Num; x.label = std::move(t); x.get = std::move(g); x.step = std::move(s); x.setv = std::move(sv); x.help = helpText(x.label); sliderInfo(x.label, x.lo, x.hi, x.val); rows.push_back(std::move(x)); };
    auto num = [&](std::string t, std::function<std::string()> g, std::function<void(int)> s) { auto sv = numSetter(t); numS(std::move(t), std::move(g), std::move(s), std::move(sv)); };
    auto txt = [&](std::string t) { Row x; x.k = Row::Text; x.label = std::move(t); rows.push_back(std::move(x)); };
    auto pills = [&](std::vector<std::pair<std::string, std::function<void()>>> p) { Row x; x.k = Row::Pills; x.pills = std::move(p); rows.push_back(std::move(x)); };
    auto rng = [&](double lo, double hi, std::function<double()> v) { auto& r2 = rows.back(); r2.lo = lo; r2.hi = hi; r2.val = std::move(v); };
    auto opt = [&](std::vector<std::string> o, std::function<int()> cu, std::function<void(int)> pk) { auto& r2 = rows.back(); r2.opts = std::move(o); r2.cur = std::move(cu); r2.pick = std::move(pk); };
    auto custom = [&](std::function<void(CCMenu*, float, float)> f) { Row x; x.k = Row::Custom; x.custom = std::move(f); rows.push_back(std::move(x)); };

    auto colorRows = [&](int* cr, int* cg, int* cb, std::function<void()> changed, std::string title = "Color") {
        custom([=, this](CCMenu* menu, float y, float w) {
            float bw = w - 24.f;
            auto node = CCNode::create();
            node->setContentSize({bw, 22.f});
            auto d = CCDrawNode::create();
            rrect(d, {0, 0}, {bw, 22.f}, 4, pGray(0.15f), 1.f, pGray(0.3f));
            rrect(d, {4.f, 4.f}, {34.f, 14.f}, 3, {*cr / 255.f, *cg / 255.f, *cb / 255.f, 1.f}, 1.f, pGray(0.45f));
            node->addChild(d);
            auto l = mkLabel(fmt::format("#{:02X}{:02X}{:02X}    R:{}  G:{}  B:{}", *cr, *cg, *cb, *cr, *cg, *cb).c_str());
            sc(l, .38f); l->setAnchorPoint({0, .5f}); l->setPosition({46.f, 11.f});
            node->addChild(l);
            auto hint = mkLabel("Pick");
            sc(hint, .34f); hint->setAnchorPoint({1.f, .5f}); hint->setPosition({bw - 10.f, 11.f}); hint->setOpacity(150);
            node->addChild(hint);
            Ref<XyzMenu> self = this;
            auto it = CCMenuItemExt::createSpriteExtra(node, [self, cr, cg, cb, changed, title](auto) {
                XyzColorPopup::open(title, *cr, *cg, *cb, [cr, cg, cb, changed](int r, int g, int b) {
                    *cr = std::clamp(r, 0, 255); *cg = std::clamp(g, 0, 255); *cb = std::clamp(b, 0, 255);
                    changed();
                }, [self] { if (s_open == self.data()) self->later(false); });
            });
            it->setPosition({w / 2.f, y});
            menu->addChild(it);
        });
    };
    auto labelChips = [&](std::function<bool(int)> selected, std::function<void(int)> pick) {
        custom([=, this](CCMenu* menu, float y, float w) {
            for (int i = 0; i < 9; ++i) {
                ccColor3B col = {(GLubyte)LABEL_COLORS[i][0], (GLubyte)LABEL_COLORS[i][1], (GLubyte)LABEL_COLORS[i][2]};
                auto ch = makeChip(col, selected(i), [this, i, pick] { pick(i); this->later(false); });
                ch->setPosition({22.f + i * ((w - 44.f) / 8.f), y});
                menu->addChild(ch);
            }
        });
    };

    auto sub = m_sub;
    if (!sub.empty()) {
        pills({{"< Back", [this] { m_sub.clear(); this->later(true); }}});
        if (sub == "tps") {
            sec("TPS SETTINGS");
            num("TPS", [] { return fmt::format("{}", (int)cfg().tpsVal); }, [](int d) {
                auto& v = cfg().tpsVal; float s = v >= 10000 ? 5000 : v >= 1000 ? 500 : v >= 240 ? 60 : 10;
                v = std::clamp(v + d * s, 1.f, 1000000.f); saveCfg(); });
            num("Frame Budget (ms)", [] { return fmt::format("{}", (int)cfg().budgetMs); }, [](int d) { cfg().budgetMs = std::clamp(cfg().budgetMs + d, 1.f, 100.f); saveCfg(); });
            tog("Lock Delta", c.lockDelta, [](bool v) {
                cfg().lockDelta = v; saveCfg();
                if (v) FLAlertLayer::create("Lock Delta", "This feature is experimental and shouldn't be used. It can leak accuracy and make macros desync. Only keep it on if you really know why you need it.", "OK")->show();
            });
            sec("OPTIMIZATION");
            tog("Fast Sub-Ticks", c.fastSub, [](bool v) { cfg().fastSub = v; saveCfg(); });
            tog("Skip Visibility On Sub-Ticks", c.skipVis, [](bool v) { cfg().skipVis = v; saveCfg(); });
            sec("PROFILER");
            tog("Profiler", c.profiler, [](bool v) { cfg().profiler = v; rt().prof = Prof{}; saveCfg(); });
        } else if (sub == "speed") {
            sec("SPEEDHACK SETTINGS");
            num("Speed", [] { return fmt::format("{:.2f}", cfg().speedVal); }, [](int d) { cfg().speedVal = std::clamp(cfg().speedVal + d * 0.1f, 0.05f, 100.f); saveCfg(); });
            tog("Gameplay Only", c.speedGameplay, [](bool v) { cfg().speedGameplay = v; saveCfg(); });
            tog("Speedhack Audio", c.speedAudio, [](bool v) { cfg().speedAudio = v; saveCfg(); });
        } else if (sub == "noclip") {
            sec("NOCLIP SETTINGS");
            tog("Noclip Player 1", c.noclipP[0], [](bool v) { cfg().noclipP[0] = v; saveCfg(); });
            tog("Noclip Player 2", c.noclipP[1], [](bool v) { cfg().noclipP[1] = v; saveCfg(); });
            tog("Noclip Tint", c.noclipTint, [](bool v) { cfg().noclipTint = v; saveCfg(); });
        } else if (sub == "fade") {
            sec("LEVEL FADE SETTINGS");
            num("Fade In (s)", [] { return fmt::format("{:.1f}", cfg().fadeLevelIn); }, [](int d) { cfg().fadeLevelIn = std::clamp(cfg().fadeLevelIn + d * 0.1f, 0.f, 10.f); saveCfg(); });
            num("Fade Out (s)", [] { return fmt::format("{:.1f}", cfg().fadeLevelOut); }, [](int d) { cfg().fadeLevelOut = std::clamp(cfg().fadeLevelOut + d * 0.1f, 0.f, 10.f); saveCfg(); });
        } else if (sub == "afade") {
            sec("AUDIO FADE SETTINGS");
            num("Audio Fade In (s)", [] { return fmt::format("{:.1f}", cfg().fadeAudioIn); }, [](int d) { cfg().fadeAudioIn = std::clamp(cfg().fadeAudioIn + d * 0.1f, 0.f, 10.f); saveCfg(); });
            num("Audio Fade Out (s)", [] { return fmt::format("{:.1f}", cfg().fadeAudioOut); }, [](int d) { cfg().fadeAudioOut = std::clamp(cfg().fadeAudioOut + d * 0.1f, 0.f, 10.f); saveCfg(); });
        } else if (sub == "layout") {
            sec("BACKGROUND COLOR");
            colorRows(&c.layoutBg[0], &c.layoutBg[1], &c.layoutBg[2], [] { saveCfg(); }, "Background");
            sec("GROUND COLOR");
            colorRows(&c.layoutGr[0], &c.layoutGr[1], &c.layoutGr[2], [] { saveCfg(); }, "Ground");
        } else if (sub == "sfly") {
            sec("AUTO STRAIGHT FLY SETTINGS");
            for (int p = 0; p < 2; ++p) {
                std::string n = p ? "P2" : "P1";
                tog("Straight Fly " + n, c.sFlyOn[p], [p](bool v) { cfg().sFlyOn[p] = v; saveCfg(); });
                num("Fly Threshold " + n, [p] { return fmt::format("{:.1f}", cfg().sFlyThr[p]); }, [p](int d) { cfg().sFlyThr[p] = std::clamp(cfg().sFlyThr[p] + d * 0.5f, 0.f, 50.f); saveCfg(); });
            }
        } else if (sub == "sufo") {
            sec("AUTO STRAIGHT UFO SETTINGS");
            for (int p = 0; p < 2; ++p) {
                std::string n = p ? "P2" : "P1";
                tog("Straight UFO " + n, c.sUfoOn[p], [p](bool v) { cfg().sUfoOn[p] = v; saveCfg(); });
                num("UFO Target Y " + n, [p] { return fmt::format("{:.0f}", cfg().sUfoTarget[p]); }, [p](int d) { cfg().sUfoTarget[p] = std::clamp(cfg().sUfoTarget[p] + d * 15.f, 0.f, 3000.f); saveCfg(); });
                num("UFO Threshold " + n, [p] { return fmt::format("{:.1f}", cfg().sUfoThr[p]); }, [p](int d) { cfg().sUfoThr[p] = std::clamp(cfg().sUfoThr[p] + d * 1.f, 0.f, 300.f); saveCfg(); });
            }
        } else if (sub == "extra") {
            sec("EXTRA CLICKS SETTINGS");
            num("Extra Clicks Amount", [] { return fmt::format("{}", cfg().extraAmt); }, [](int d) { cfg().extraAmt = std::clamp(cfg().extraAmt + d, 1, 10); saveCfg(); });
        } else if (sub == "flip") {
            sec("FLIP INPUT ON DEATH SETTINGS");
            tog("Flip Player 1", c.flipP1, [](bool v) { cfg().flipP1 = v; saveCfg(); });
            tog("Flip Player 2", c.flipP2, [](bool v) { cfg().flipP2 = v; saveCfg(); });
            tog("Flip Both", c.flipBoth, [](bool v) { cfg().flipBoth = v; saveCfg(); });
            tog("Flip Swift", c.flipSwift, [](bool v) { cfg().flipSwift = v; saveCfg(); });
        } else if (sub == "naming") {
            static char const* FMTS[] = {"Name_1", "Name 1", "Name (1)", "Name-1"};
            sec("NAMING SETTINGS");
            num("Rename Format", [] { return std::string(FMTS[std::clamp(cfg().renameFmt, 0, 3)]); }, [](int d) { auto& v = cfg().renameFmt; v = (v + d + 4) % 4; saveCfg(); });
            opt(std::vector<std::string>{FMTS[0], FMTS[1], FMTS[2], FMTS[3]}, [] { return std::clamp(cfg().renameFmt, 0, 3); }, [](int v) { cfg().renameFmt = v; saveCfg(); });
        } else if (sub == "seq") {
            sec("SEQUENCE SETTINGS");
            tog("Random Order", c.seqRandom, [](bool v) { cfg().seqRandom = v; saveCfg(); });
            pills({{"Clear Sequence", [this] { cfg().seq.clear(); saveCfg(); this->later(false); }}});
            if (c.seq.empty()) txt("The sequence is empty. Use +Seq on a macro in the Macros tab.");
            for (size_t i = 0; i < c.seq.size(); ++i)
                custom([this, i](CCMenu* menu, float y, float w) {
                    auto l = mkLabel(fmt::format("{}. {}", i + 1, cfg().seq[i]).c_str());
                    fit(l, w - 110.f, .4f, .1f); l->setAnchorPoint({0, .5f}); l->setPosition({12, y});
                    menu->addChild(l);
                    auto rm = makePill("Remove", false, [this, i] {
                        if (i < cfg().seq.size()) cfg().seq.erase(cfg().seq.begin() + i);
                        saveCfg(); this->later(false); });
                    rm->setPosition({w - 36.f, y}); menu->addChild(rm);
                });
        } else if (sub == "stepper") {
            sec("FRAME STEPPER SETTINGS");
            tog("On-Screen Controls", c.stepBar, [](bool v) { cfg().stepBar = v; saveCfg(); });
            num("Steps Per +N Button", [] { return fmt::format("{}", cfg().stepN); }, [](int d) { cfg().stepN = std::clamp(cfg().stepN + d, 1, 240); saveCfg(); });
            num("Hold Speed (ticks/frame)", [] { return fmt::format("{}", cfg().holdRate); }, [](int d) { cfg().holdRate = std::clamp(cfg().holdRate + d, 1, 16); saveCfg(); });
        } else if (sub == "ssb") {
            sec("SSB FIX SETTINGS");
            tog("Invert Correction", c.ssbInvert, [](bool v) { cfg().ssbInvert = v; saveCfg(); });
        } else if (sub == "legship") {
            sec("LEGACY SHIP SETTINGS");
            num("Extra Weight (%)", [] { return fmt::format("{}", cfg().legacyShipPct); }, [](int d) { cfg().legacyShipPct = std::clamp(cfg().legacyShipPct + d, 0, 100); saveCfg(); });
        } else if (sub == "hitbox") {
            sec("HITBOX SETTINGS");
            tog("Player", c.hbPlayer, [](bool v) { cfg().hbPlayer = v; saveCfg(); });
            tog("Solid / Hazard", c.hbSolid && c.hbHazard, [](bool v) { cfg().hbSolid = cfg().hbHazard = v; saveCfg(); });
            tog("Special / Rotated", c.hbSpecial && c.hbRotated, [](bool v) { cfg().hbSpecial = cfg().hbRotated = v; saveCfg(); });
        } else if (sub == "trail") {
            sec("TRAIL SETTINGS");
            tog("Clear Trail On Death", c.trailClear, [](bool v) { cfg().trailClear = v; saveCfg(); });
            num("Trail Length", [] { return fmt::format("{}", cfg().trailLen); }, [](int d) { cfg().trailLen = std::clamp(cfg().trailLen + d * 50, 10, 2000); saveCfg(); });
        } else if (sub == "traj") {
            sec("TRAJECTORY SETTINGS");
            num("Length", [] { return fmt::format("{}", cfg().trajLen); }, [](int d) { cfg().trajLen = std::clamp(cfg().trajLen + d * 30, 10, 2000); saveCfg(); });
        } else if (sub == "pd") {
            sec("PREVENT DEATH SETTINGS");
            num("Lookahead Ticks", [] { return fmt::format("{}", cfg().pdLook); }, [](int d) { cfg().pdLook = std::clamp(cfg().pdLook + d, 1, 10); saveCfg(); });
        } else if (sub == "logging") {
            sec("LOGGING SETTINGS");
            tog("Position Logging", c.logPositions, [](bool v) { cfg().logPositions = v; saveCfg(); });
            num("Position Interval", [] { return fmt::format("{}", cfg().logPosEvery); }, [](int d) { cfg().logPosEvery = std::clamp(cfg().logPosEvery + d, 1, 240); saveCfg(); });
            pills({{"Open Logs Folder", [] { openFolderOrShow(persistDir("logs")); }}});
        } else if (sub == "autosave") {
            sec("AUTOSAVE SETTINGS");
            num("Interval (s)", [] { return fmt::format("{}", cfg().autosaveSec); }, [](int d) { cfg().autosaveSec = std::clamp(cfg().autosaveSec + d * 30, 10, 3600); saveCfg(); });
        } else if (sub == "ac0" || sub == "ac1") {
            int p = sub == "ac1";
            sec(p ? "AUTOCLICKER P2 SETTINGS" : "AUTOCLICKER P1 SETTINGS");
            tog("Swift Clicks", c.acSwift[p], [p](bool v) { cfg().acSwift[p] = v; saveCfg(); });
            numS("Hold Ticks", [p] { return fmt::format("{}", cfg().acHold[p]); }, [p](int d) { cfg().acHold[p] = std::clamp(cfg().acHold[p] + d, 1, 240); saveCfg(); }, [p](double v) { cfg().acHold[p] = std::clamp((int)std::lround(v), 1, 240); saveCfg(); });
            numS("Release Ticks", [p] { return fmt::format("{}", cfg().acRel[p]); }, [p](int d) { cfg().acRel[p] = std::clamp(cfg().acRel[p] + d, 1, 240); saveCfg(); }, [p](double v) { cfg().acRel[p] = std::clamp((int)std::lround(v), 1, 240); saveCfg(); });
            numS("Clicks Per Hold", [p] { return fmt::format("{}", cfg().acClicks[p]); }, [p](int d) { cfg().acClicks[p] = std::clamp(cfg().acClicks[p] + d, 1, 10); saveCfg(); }, [p](double v) { cfg().acClicks[p] = std::clamp((int)std::lround(v), 1, 10); saveCfg(); });
        } else if (sub == "label" && m_sel >= 0) {
            size_t i = (size_t)m_sel;
            sec(labels::name(i));
            tog("Enabled", labels::cfg(i).on, [i](bool v) { labels::cfg(i).on = v; labels::save(i); });
            num("Anchor", [i] { static char const* a[] = {"Top Left", "Top Right", "Bottom Left", "Bottom Right"}; return std::string(a[std::clamp(labels::cfg(i).anchor, 0, 3)]); },
                [i](int d) { auto& a = labels::cfg(i).anchor; a = (a + d + 4) % 4; labels::save(i); });
            opt(std::vector<std::string>{"Top Left", "Top Right", "Bottom Left", "Bottom Right"}, [i] { return std::clamp(labels::cfg(i).anchor, 0, 3); }, [i](int v) { labels::cfg(i).anchor = v; labels::save(i); });
            numS("Size", [i] { return fmt::format("{:.2f}", labels::cfg(i).scale); }, [i](int d) { auto& s = labels::cfg(i).scale; s = std::clamp(s + d * 0.05f, 0.1f, 2.f); labels::save(i); }, [i](double v) { labels::cfg(i).scale = std::clamp((float)v, 0.1f, 2.f); labels::save(i); });
            rng(0.1, 2.0, [i] { return (double)labels::cfg(i).scale; });
            numS("Opacity", [i] { return fmt::format("{:.2f}", labels::cfg(i).opacity); }, [i](int d) { auto& o = labels::cfg(i).opacity; o = std::clamp(o + d * 0.1f, 0.1f, 1.f); labels::save(i); }, [i](double v) { labels::cfg(i).opacity = std::clamp((float)v, 0.1f, 1.f); labels::save(i); });
            rng(0.1, 1.0, [i] { return (double)labels::cfg(i).opacity; });
            sec("COLOR");
            labelChips([i](int k) { auto& l = labels::cfg(i); return l.r == LABEL_COLORS[k][0] && l.g == LABEL_COLORS[k][1] && l.b == LABEL_COLORS[k][2]; },
                       [i](int k) { auto& l = labels::cfg(i); l.r = LABEL_COLORS[k][0]; l.g = LABEL_COLORS[k][1]; l.b = LABEL_COLORS[k][2]; labels::save(i); });
            auto& l = labels::cfg(i);
            colorRows(&l.r, &l.g, &l.b, [i] { labels::save(i); }, "Label color");
        }
    } else switch (m_tab) {
    case 0: {
        std::string st = r.mode == Mode::Record ? "Recording" : r.mode == Mode::Play ? "Playing" : "Idle";
        auto playMacro = [this] {
            auto& r = rt();
            if (r.mode == Mode::Play) { stopAll(false); this->later(false); return; }
            auto n = !r.pendingTmp.empty() ? r.pendingTmp : cleanMacroName(r.macroName);
            if (n.empty()) return;
            if (!startPlay(n)) Notification::create("Cannot play macro", NotificationIcon::Error)->show();
            else this->onClose(nullptr);
        };
        if (m_paper) {
            custom([](CCMenu* menu, float y, float w) {
                auto in = TextInput::create((w - 64.f) / .85f, "Macro name", xyz::draw::fontOf());
                in->setScale(.85f);
                in->setString(rt().macroName);
                in->setCallback([](std::string const& v) { rt().macroName = v; });
                in->setPosition({(w - 40.f) / 2.f, y});
                menu->addChild(in);
                auto fb = makeRound("...", [] { openFolderOrShow(macroDir()); });
                fb->setPosition({w - 20.f, y});
                menu->addChild(fb);
            });
            custom([this, playMacro](CCMenu* menu, float y, float w) {
                float bw = (w - 30.f) / 2.f;
                bool recording = rt().mode == Mode::Record, playing = rt().mode == Mode::Play;
                auto rb = makePill(recording ? "Stop" : "Record", !recording, [this] {
                    if (rt().mode == Mode::Record) stopAll(true); else startRecord();
                    this->later(false); }, bw);
                rb->setPosition({10.f + bw / 2.f, y});
                menu->addChild(rb);
                auto pb = makePill(playing ? "Stop" : "Play", playing, playMacro, bw);
                pb->setPosition({20.f + bw * 1.5f, y});
                menu->addChild(pb);
            });
            txt(fmt::format("State: {}    TPS: {:.1f}    Speed: {:.3f}", st, (double)curTps(), c.speed ? (double)c.speedVal : 1.0));
            pills({{"Back", [] { stepBack(1); }}, {"Step", [] { stepOnce(); }}, {"Save", [this] {
                auto& r = rt();
                if (r.pendingTmp.empty()) { Notification::create("No recording is waiting for a name", NotificationIcon::Info)->show(); return; }
                auto name = cleanMacroName(r.macroName);
                if (name.empty()) name = macroFileName();
                if (cfg().noOverwrite) name = uniqueMacroName(name);
                std::error_code ec;
                auto from = macroDir() / (r.pendingTmp + ".xyz"), to = macroDir() / (name + ".xyz");
                if (from != to) { std::filesystem::remove(to, ec); ec.clear(); std::filesystem::rename(from, to, ec); }
                if (ec) { Notification::create("Could not rename the macro", NotificationIcon::Error)->show(); return; }
                r.pendingTmp.clear(); r.macroName = name;
                this->later(false); }}});
        } else {
        sec("BOT");
        txt("State: " + st);
        pills({{"Record", [this] { startRecord(); this->later(false); }}, {"Stop", [this] { stopAll(true); this->later(false); }}, {"Back", [] { stepBack(1); }}, {"Step", [] { stepOnce(); }}});
        sec("MACRO");
        custom([](CCMenu* menu, float y, float w) {
            auto in = TextInput::create((w - 24.f) / .85f, "Macro name", xyz::draw::fontOf());
            in->setScale(.85f);
            in->setString(rt().macroName);
            in->setCallback([](std::string const& v) { rt().macroName = v; });
            in->setPosition({w / 2.f, y});
            menu->addChild(in);
        });
        pills({{"Play", [this] {
            auto& r = rt();
            auto n = !r.pendingTmp.empty() ? r.pendingTmp : cleanMacroName(r.macroName);
            if (n.empty()) return;
            if (!startPlay(n)) Notification::create("Cannot play macro", NotificationIcon::Error)->show();
            else this->onClose(nullptr);
        }}, {"Save", [this] {
            auto& r = rt();
            if (r.pendingTmp.empty()) return;
            auto name = cleanMacroName(r.macroName);
            if (name.empty()) name = macroFileName();
            if (cfg().noOverwrite) name = uniqueMacroName(name);
            std::error_code ec;
            auto from = macroDir() / (r.pendingTmp + ".xyz"), to = macroDir() / (name + ".xyz");
            if (from != to) {
                std::filesystem::remove(to, ec);
                std::filesystem::rename(from, to, ec);
            }
            if (ec) Notification::create("Could not rename the macro", NotificationIcon::Error)->show();
            if (!ec) { r.pendingTmp.clear(); r.macroName = name; }
            this->later(false);
        }}});
        }
        tog("Frame Stepper", c.stepper, [](bool v) { cfg().stepper = v; }, "stepper");
        tog("Noclip", c.noclip, [](bool v) { cfg().noclip = v; saveCfg(); }, "noclip");
        sec("ACCURACY");
        tog("CBF (click between steps)", c.cbf, [](bool v) { cfg().cbf = v; saveCfg(); });
        // with CBF the fix modes are off: the row shows what is really used and can't be changed
        num("Accuracy", [] { return std::string(ACCURACY[std::clamp(accEff(), 0, 3)]); }, [](int d) { if (cfg().cbf) return; auto& v = cfg().accuracy; v = (v + d + 4) % 4; saveCfg(); });
        opt(std::vector<std::string>{ACCURACY[0], ACCURACY[1], ACCURACY[2], ACCURACY[3]}, [] { return std::clamp(accEff(), 0, 3); }, [](int v) { if (cfg().cbf) return; cfg().accuracy = v; saveCfg(); });
        tog("Ignore Level Check", c.ignoreLevel, [](bool v) { cfg().ignoreLevel = v; saveCfg(); });
        tog("Block Inputs In Playback", c.blockInputs, [](bool v) { cfg().blockInputs = v; saveCfg(); });
        sec("RECORDING");
        num("Record Players", [] { return std::string(PLAYERS[std::clamp(cfg().recPlayers, 0, 2)]); }, [](int d) { auto& v = cfg().recPlayers; v = (v + d + 3) % 3; saveCfg(); });
        opt(std::vector<std::string>{PLAYERS[0], PLAYERS[1], PLAYERS[2]}, [] { return std::clamp(cfg().recPlayers, 0, 2); }, [](int v) { cfg().recPlayers = v; saveCfg(); });
        tog("Auto Save", c.autoSave, [](bool v) { cfg().autoSave = v; saveCfg(); });
        tog("Auto Name Replay", c.autoName, [](bool v) { cfg().autoName = v; saveCfg(); }, "naming");
        tog("No Overwrite", c.noOverwrite, [](bool v) { cfg().noOverwrite = v; saveCfg(); });
        sec("PLAYBACK");
        num("Play Players", [] { return std::string(PLAYERS[std::clamp(cfg().playPlayers, 0, 2)]); }, [](int d) { auto& v = cfg().playPlayers; v = (v + d + 3) % 3; saveCfg(); });
        opt(std::vector<std::string>{PLAYERS[0], PLAYERS[1], PLAYERS[2]}, [] { return std::clamp(cfg().playPlayers, 0, 2); }, [](int v) { cfg().playPlayers = v; saveCfg(); });
        tog("Sequence", c.seqOn, [](bool v) { cfg().seqOn = v; saveCfg(); }, "seq");
        sec("AUTOMATION");
        tog("Autosave While Recording", c.autosave, [](bool v) { cfg().autosave = v; saveCfg(); }, "autosave");
        tog("Auto Exit On Finish", c.autoExit, [](bool v) { cfg().autoExit = v; saveCfg(); });
        break;
    }
    case 1:
        sec("TPS");
        tog("TPS Bypass", c.tps, [](bool v) { cfg().tps = v; saveCfg(); }, "tps");
        sec("SPEEDHACK");
        tog("Speedhack", c.speed, [](bool v) { cfg().speed = v; saveCfg(); }, "speed");
        sec("SCROLL SPEED BUG");
        tog("SSB Fix (song sync)", c.ssbFix, [](bool v) { cfg().ssbFix = v; saveCfg(); }, "ssb");
        sec("LEGACY PHYSICS");
        tog("Legacy Ship Weight (2.1)", c.legacyShip, [](bool v) { cfg().legacyShip = v; saveCfg(); }, "legship");
        sec("PRECISION");
        tog("Velocity Unrounding", c.velUnround, [](bool v) { cfg().velUnround = v; precision::applyVelUnround(v); saveCfg(); });
        tog("X Position Extension", c.xExt, [](bool v) { cfg().xExt = v; saveCfg(); });
        txt(precision::status());
        break;
    case 2:
        sec("HITBOXES");
        tog("Show Hitboxes", c.hitboxes, [](bool v) { cfg().hitboxes = v; saveCfg(); }, "hitbox");
        tog("Hitbox Trail", c.trail, [](bool v) { cfg().trail = v; saveCfg(); }, "trail");
        sec("TRAJECTORY");
        tog("Show Trajectory", c.traj, [](bool v) { cfg().traj = v; saveCfg(); }, "traj");
        sec("FIXES");
        tog("Fix Rotation Bug", c.fixRotation, [](bool v) { cfg().fixRotation = v; saveCfg(); });
        tog("Wave Drag Fix", c.waveDragFix, [](bool v) { cfg().waveDragFix = v; saveCfg(); });
        sec("LOGGING");
        tog("Logging", c.logging, [](bool v) { cfg().logging = v; saveCfg(); }, "logging");
        sec("LEVEL");
        tog("Layout Mode (reload level)", c.layout, [](bool v) { cfg().layout = v; saveCfg(); }, "layout");
        sec("EFFECTS");
        tog("No Death Effect", c.noDeathEffect, [](bool v) { cfg().noDeathEffect = v; saveCfg(); });
        tog("No Effects", c.noEffect, [](bool v) { cfg().noEffect = v; saveCfg(); });
        tog("Hide Endscreen", c.hideEndscreen, [](bool v) { cfg().hideEndscreen = v; saveCfg(); });
        tog("Hide New Best", c.hideNewBest, [](bool v) { cfg().hideNewBest = v; saveCfg(); });
        tog("Stacked Orb Optimization", c.optStacked, [](bool v) { cfg().optStacked = v; saveCfg(); });
        tog("Restart First Frame", c.restartFirst, [](bool v) { cfg().restartFirst = v; saveCfg(); });
        sec("FADES");
        tog("Level Fade In/Out", c.fadeLevel, [](bool v) { cfg().fadeLevel = v; saveCfg(); }, "fade");
        tog("Audio Fade In/Out", c.fadeAudio, [](bool v) { cfg().fadeAudio = v; saveCfg(); }, "afade");
        break;
    case 3: {
        sec("MACROS");
        pills({{"Record", [this] { startRecord(); this->onClose(nullptr); }}, {"Stop", [this] { stopAll(true); this->later(false); }},
               {"Import", [this] {
                    int n = 0, bad = 0; std::string why; std::error_code ec;
                    for (auto& e : std::filesystem::directory_iterator(macroDir(), ec)) {
                        if (!canImport(e.path())) continue;
                        auto name = utils::string::pathToString(e.path().filename());
                        auto low = name; std::transform(low.begin(), low.end(), low.begin(), [](unsigned char ch) { return (char)std::tolower(ch); });
                        if (low.size() > 9 && low.compare(low.size() - 9, 9, ".gdr.json") == 0) name.resize(name.size() - 9);
                        else name = utils::string::pathToString(e.path().stem());
                        Macro m;
                        if (importMacro(m, e.path()) && saveMacro(m, name)) n++;
                        else { bad++; why = importError(); }
                    }
                    auto msg = fmt::format("Imported {}", n);
                    if (bad) msg += fmt::format(", {} failed{}", bad, why.empty() ? "" : " (" + why + ")");
                    if (bad) Notification::create(msg, NotificationIcon::Warning)->show();
                    this->later(false); }}});
        custom([this](CCMenu* menu, float y, float w) {
            float iw = w - 190.f;
            auto in = TextInput::create(iw / .8f, "Search macros", xyz::draw::fontOf());
            in->setScale(.8f);
            in->setString(m_query, false);
            in->setCallback([this](std::string const& t) { m_query = t; });
            in->setPosition({12.f + iw / 2.f, y});
            menu->addChild(in);
            auto go = makePill("Search", true, [this] { this->later(false); }, 60.f);
            go->setPosition({w - 12.f - 50.f - 8.f - 30.f, y});
            menu->addChild(go);
            auto cl = makePill("Clear", false, [this] { m_query.clear(); this->later(false); }, 50.f);
            cl->setPosition({w - 12.f - 25.f, y});
            menu->addChild(cl);
        });
        sec("LIBRARY");
        auto names = listMacrosRecent();
        size_t allCount = names.size();
        if (!m_query.empty()) {
            auto lower = [](std::string s) { std::transform(s.begin(), s.end(), s.begin(), [](unsigned char ch) { return (char)std::tolower(ch); }); return s; };
            auto q = lower(m_query); size_t total = names.size();
            names.erase(std::remove_if(names.begin(), names.end(), [&](std::string const& n) { return lower(n).find(q) == std::string::npos; }), names.end());
            txt(fmt::format("Search \"{}\": {} of {}", m_query, names.size(), total));
        }
        if (!m_mergeBase.empty()) txt("Merge base: " + m_mergeBase + ". Tap Merge on another macro, or on it again to cancel.");
        if (m_query.empty() && !names.empty()) txt(fmt::format("{} macros, newest first. Type a new name and press Rename.", allCount));
        size_t hidden = names.size() > (size_t)m_libShown ? names.size() - (size_t)m_libShown : 0;
        if (hidden) names.resize((size_t)m_libShown);
        if (names.empty()) txt(m_query.empty() ? "No macros yet. Drop .gdr, .gdr.json, .xd, .slc or .txt files into the macros folder and press Import." : "No macro matches.");
        for (auto& n : names) {
            custom([this, n](CCMenu* menu, float y, float w) {
                std::error_code ec;
                auto sz = std::filesystem::file_size(macroDir() / (n + ".xyz"), ec);
                float nameW = std::max(70.f, w - 262.f);
                auto typed = std::make_shared<std::string>(n);
                auto in = TextInput::create(nameW / .6f, "name", xyz::draw::fontOf());
                in->setScale(.6f);
                in->setMaxCharCount(80);
                in->setString((m_mergeBase == n ? "* " : "") + n, false);
                in->setCallback([typed](std::string const& t) { *typed = t; });
                in->setPosition({12.f + nameW / 2.f, y + 3.f});
                menu->addChild(in);
                Ref<XyzMenu> selfRef = this;
                auto s = mkLabel(ec ? "" : (sz >= 1024 ? fmt::format("{:.1f} KB", sz / 1024.0) : fmt::format("{} B", sz)).c_str());
                sc(s, .26f); s->setOpacity(130); s->setAnchorPoint({0, .5f}); s->setPosition({12, y - 9});
                menu->addChild(s);
                std::vector<CCMenuItemSpriteExtra*> ps;
                ps.push_back(makePill("Rename", false, [selfRef, n, typed] {
                    auto v = *typed;
                    if (v.compare(0, 2, "* ") == 0) v = v.substr(2);
                    v = cleanMacroName(v);
                    if (v.empty() || v == n) return;
                    std::error_code ec;
                    if (std::filesystem::exists(macroDir() / (v + ".xyz"), ec)) { Notification::create("A macro with that name already exists", NotificationIcon::Error)->show(); return; }
                    std::filesystem::rename(macroDir() / (n + ".xyz"), macroDir() / (v + ".xyz"), ec);
                    if (ec) { Notification::create("Could not rename the macro", NotificationIcon::Error)->show(); return; }
                    if (rt().pendingTmp == n) rt().pendingTmp = v;
                    selfRef->later(false); }));
                ps.push_back(makePill("Del", false, [this, n] { std::error_code e2; std::filesystem::remove(macroDir() / (n + ".xyz"), e2); this->later(false); }));
                ps.push_back(makePill("Edit", false, [n] { xyz::openMacroEditor(n); }));
                ps.push_back(makePill("Merge", false, [this, n] {
                    if (m_mergeBase.empty()) {
                        m_mergeBase = n;
                        Notification::create("Merge base set. Tap Merge on another macro.", NotificationIcon::Info)->show();
                    } else if (m_mergeBase == n) {
                        m_mergeBase.clear();
                    } else {
                        std::string out;
                        bool ok = mergeMacros(m_mergeBase, n, out);
                        if (!ok) Notification::create("Merge failed (different TPS?)", NotificationIcon::Error)->show();
                        m_mergeBase.clear();
                    }
                    this->later(false); }));
                ps.push_back(makePill("+Seq", false, [this, n] {
                    cfg().seq.push_back(n); saveCfg(); }));
                ps.push_back(makePill("Play", true, [this, n] {
                    if (!startPlay(n)) Notification::create("Cannot play macro", NotificationIcon::Error)->show();
                    else this->onClose(nullptr); }));
                float x = w - 8.f;
                for (auto p : ps) {
                    float pw = p->getContentSize().width;
                    p->setPosition({x - pw / 2.f, y}); menu->addChild(p);
                    x -= pw + 5.f;
                }
            });
        }
        if (hidden) pills({{fmt::format("Show more ({} left)", hidden), [this] { m_libShown += 40; this->later(false); }}});
        break;
    }
    case 4:
        sec("LABELS");
        tog("Labels Enabled", c.labels, [](bool v) { cfg().labels = v; saveCfg(); labels::invalidate(); });
        sec("ALL LABELS COLOR");
        labelChips([](int) { return false; }, [](int k) { labels::setAllColor(LABEL_COLORS[k][0], LABEL_COLORS[k][1], LABEL_COLORS[k][2]); });
        sec("EACH LABEL");
        for (size_t i = 0; i < labels::count(); ++i)
            custom([this, i](CCMenu* menu, float y, float w) {
                auto l = mkLabel(labels::name(i).c_str());
                fit(l, w - 150.f, .4f, .1f); l->setAnchorPoint({0, .5f}); l->setPosition({12, y});
                menu->addChild(l);
                if (!labels::help(i).empty()) {
                    auto hb = makeHelp(labels::name(i), labels::help(i));
                    hb->setPosition({12.f + l->getScaledContentSize().width + 13.f, y}); menu->addChild(hb);
                }
                auto sw = makeSwitch(labels::cfg(i).on, [i](bool v) { labels::cfg(i).on = v; labels::save(i); });
                sw->setPosition({w - 32, y}); menu->addChild(sw);
                auto g = makeGear([this, i] { m_sel = (int)i; m_sub = "label"; this->later(true); });
                g->setPosition({w - 76, y}); menu->addChild(g);
            });
        break;
    case 5:
        sec("SURVIVAL");
        tog("Prevent Death", c.preventDeath, [](bool v) { cfg().preventDeath = v; saveCfg(); }, "pd");
        tog("Trajectory Autoclicker", c.trajAuto, [](bool v) { cfg().trajAuto = v; saveCfg(); });
        num("Click Lookahead", [] { return fmt::format("{}", cfg().trajLook); }, [](int d) { cfg().trajLook = std::clamp(cfg().trajLook + d, 1, 240); saveCfg(); });
        sec("INPUT");
        tog("Maintain Gravity", c.maintainGravity, [](bool v) { cfg().maintainGravity = v; saveCfg(); });
        tog("Mirror Inputs", c.mirror, [](bool v) { cfg().mirror = v; saveCfg(); });
        tog("Mirror Inverted", c.mirrorInv, [](bool v) { cfg().mirrorInv = v; saveCfg(); });
        tog("No Mirror Portals", c.noMirror, [](bool v) { cfg().noMirror = v; saveCfg(); });
        tog("Swap Player Input", c.swapInput, [](bool v) { cfg().swapInput = v; saveCfg(); });
        tog("Inverse Input P1", c.inverse[0], [](bool v) { cfg().inverse[0] = v; saveCfg(); });
        tog("Inverse Input P2", c.inverse[1], [](bool v) { cfg().inverse[1] = v; saveCfg(); });
        sec("ORBS AND PADS");
        tog("Click Green Dash Orbs", c.clickDash, [](bool v) { cfg().clickDash = v; saveCfg(); });
        tog("Click Black Orbs", c.clickBlack, [](bool v) { cfg().clickBlack = v; saveCfg(); });
        tog("Easy Black Orb UFO", c.blackUfo, [](bool v) { cfg().blackUfo = v; saveCfg(); });
        tog("Click Jump Pads", c.clickPads, [](bool v) { cfg().clickPads = v; saveCfg(); });
        tog("Click Gravity Pads", c.clickGravPads, [](bool v) { cfg().clickGravPads = v; saveCfg(); });
        sec("STABILIZERS");
        tog("Auto Straight Fly", c.sFly, [](bool v) { cfg().sFly = v; saveCfg(); }, "sfly");
        tog("Auto Straight UFO", c.sUfo, [](bool v) { cfg().sUfo = v; saveCfg(); }, "sufo");
        sec("CLICKS");
        tog("Auto Swift", c.autoSwift, [](bool v) { cfg().autoSwift = v; saveCfg(); });
        tog("Extra Clicks", c.extraClick, [](bool v) { cfg().extraClick = v; saveCfg(); }, "extra");
        sec("DEATH");
        tog("Flip Input On Death", c.flipDeath, [](bool v) { cfg().flipDeath = v; saveCfg(); }, "flip");
        sec("AUTOCLICKER");
        tog("Autoclicker P1", c.acOn[0], [](bool v) { cfg().acOn[0] = v; saveCfg(); }, "ac0");
        tog("Autoclicker P2", c.acOn[1], [](bool v) { cfg().acOn[1] = v; saveCfg(); }, "ac1");
        break;
    case 6: {
        static char const* CODEC_NAMES[] = {"Auto", "libx264", "h264_mediacodec", "libopenh264", "mpeg4"};
        tog("Auto Height (match screen)", c.renderAutoH, [](bool v) {
            auto& c = cfg();
            if (!v && c.renderAutoH) c.renderH = std::clamp(autoRenderHeight(), 320, 3840) & ~1;
            c.renderAutoH = v;
            saveCfg();
        });
        sec("RENDER");
        txt(isRendering() ? "Rendering... press Stop Render to finish." : renderAvailable() ? "Ready. Open a level, then press Start Render." : "This build has no FFmpeg libraries, rendering is off.");
        pills({{"Start Render", [this] {
                    auto err = startRender();
                    if (!err.empty()) { Notification::create(err, NotificationIcon::Error)->show(); return; }
                    this->onClose(nullptr); }},
               {"Stop Render", [this] { renderStop(true); this->later(false); }}});
        sec("SOURCE");
        num("Macro To Render", [] { return cfg().renderMacro.empty() ? std::string("None (manual)") : cfg().renderMacro; }, [](int d) {
            auto names = listMacros();
            int idx = -1;
            for (size_t i = 0; i < names.size(); ++i) if (names[i] == cfg().renderMacro) idx = (int)i;
            int count = (int)names.size() + 1;
            int slot = (idx + 1 + d + count) % count;
            cfg().renderMacro = slot == 0 ? std::string() : names[slot - 1];
            saveCfg(); });
        sec("VIDEO");
        num("Width", [] { return fmt::format("{}", cfg().renderW); }, [](int d) { cfg().renderW = std::clamp(cfg().renderW + d * 160, 320, 3840); saveCfg(); });
        num("Video Height", [] {
            return cfg().renderAutoH
                ? fmt::format("Auto ({} px)", autoRenderHeight())
                : fmt::format("{} px", cfg().renderH);
        }, [](int d) {
            auto& c = cfg();
            if (c.renderAutoH) {
                c.renderH = std::clamp(autoRenderHeight(), 320, 3840) & ~1;
                c.renderAutoH = false;
            }
            c.renderH = std::clamp(c.renderH + d * 120, 320, 3840) & ~1;
            saveCfg();
        });
        rng(320, 3840, [] {
            auto& c = cfg();
            return (double)(c.renderAutoH ? autoRenderHeight() : c.renderH);
        });
        if (c.renderAutoH) txt(fmt::format("Automatic height: {} px", autoRenderHeight()));
        num("FPS", [] { return fmt::format("{}", cfg().renderFps); }, [](int d) { cfg().renderFps = std::clamp(cfg().renderFps + d * 10, 10, 240); saveCfg(); });
        num("Bitrate (Mbps)", [] { return fmt::format("{}", cfg().renderBitrate); }, [](int d) { cfg().renderBitrate = std::clamp(cfg().renderBitrate + d, 1, 200); saveCfg(); });
        num("Codec", [] { return std::string(CODEC_NAMES[std::clamp(cfg().renderCodec, 0, 4)]); }, [](int d) { auto& v = cfg().renderCodec; v = (v + d + 5) % 5; saveCfg(); });
        opt(std::vector<std::string>{CODEC_NAMES[0], CODEC_NAMES[1], CODEC_NAMES[2], CODEC_NAMES[3], CODEC_NAMES[4]}, [] { return std::clamp(cfg().renderCodec, 0, 4); }, [](int v) { cfg().renderCodec = v; saveCfg(); });
        num("End Tail (s)", [] { return fmt::format("{}", cfg().renderTail); }, [](int d) { cfg().renderTail = std::clamp(cfg().renderTail + d, 0, 30); saveCfg(); });
        tog("Include Song Audio", c.renderAudio, [](bool v) { cfg().renderAudio = v; saveCfg(); });
        break;
    }
    case 7: {
        sec("LUA SCRIPTS");
        txt(lua::status());
        pills({{"Reload", [this] { lua::reload(); this->later(false); }},
               {"Open Folder", [] { openFolderOrShow(lua::dir()); }}});
        sec("FILES (turn on, then Run or Reload)");
        custom([this](CCMenu* menu, float y, float w) {
            float iw = w - 110.f;
            auto in = TextInput::create(iw / .8f, "New script name", xyz::draw::fontOf());
            in->setScale(.8f);
            in->setString(s_newScript, false);
            in->setCallback([](std::string const& t) { s_newScript = t; });
            in->setPosition({12.f + iw / 2.f, y});
            menu->addChild(in);
            auto mk = makePill("Create", true, [this] {
                auto v = cleanMacroName(s_newScript);
                if (v.size() > 4 && v.compare(v.size() - 4, 4, ".lua") == 0) v.resize(v.size() - 4);
                if (v.empty()) { Notification::create("Type a name first", NotificationIcon::Info)->show(); return; }
                auto file = v + ".lua";
                std::error_code ec;
                if (!std::filesystem::exists(lua::dir() / file, ec)) {
                    std::ofstream f(lua::dir() / file, std::ios::binary);
                    f << "-- " << v << "\n-- Turn it on in the Scripts tab, then press Run in the editor.\n\nfunction on_tick(tick)\n\nend\n";
                }
                s_newScript.clear();
                XyzLuaEditor::open(file);
                this->later(false);
            }, 70.f);
            mk->setPosition({w - 12.f - 35.f, y});
            menu->addChild(mk);
        });
        auto names = lua::files();
        if (names.empty()) txt("No scripts yet. Tap New Script to write one here.");
        for (auto& n : names)
            custom([this, n](CCMenu* menu, float y, float w) {
                auto l = mkLabel(n.c_str());
                fit(l, w - 150.f, .4f, .1f); l->setAnchorPoint({0, m_paper ? .5f : .5f}); l->setPosition({m_paper ? 38.f : 12.f, y});
                menu->addChild(l);
                auto sw = makeSwitch(lua::enabled(n), [n](bool v) { lua::setEnabled(n, v); });
                sw->setPosition({m_paper ? 22.f : w - 24.f, y}); menu->addChild(sw);
                auto ed = makePill("Edit", true, [n] { XyzLuaEditor::open(n); }, 46.f);
                ed->setPosition({w - 8.f - ed->getContentWidth() / 2.f, y}); menu->addChild(ed);
            });
        auto const& o = lua::output();
        if (!o.empty()) {
            sec("OUTPUT");
            size_t from = o.size() > 8 ? o.size() - 8 : 0;
            for (size_t i = from; i < o.size(); ++i) txt(o[i]);
        }
        break;
    }
    case 8: {
        sec("CLICKBOT");
        txt(clickbot::status());
        tog("Clickbot", clickbot::on(), [](bool v) { clickbot::setOn(v); });
        tog("Click On My Inputs", clickbot::liveClicks(), [](bool v) { clickbot::setLiveClicks(v); });
        tog("Mix Into Render", clickbot::mixRender(), [](bool v) { clickbot::setMixRender(v); });
        num("Volume (%)", [] { return fmt::format("{}", clickbot::volume()); }, [](int d) { clickbot::setVolume(clickbot::volume() + d * 10); });
        pills({{"Reload", [this] { clickbot::reload(); this->later(false); }},
               {"Open Folder", [] { openFolderOrShow(clickbot::dir()); }}});
        sec("CLICKPACKS (folders and .zip)");
        auto packs = clickbot::packs();
        if (packs.empty()) txt("No clickpacks yet. Put folders or .zip files in the folder.");
        for (auto& n : packs) tog(n, clickbot::current() == n, [this, n](bool v) { if (v) { clickbot::select(n); this->later(false); } });
        break;
    }
    default: {
        sec("DISCORD (PC)");
        tog("Discord Rich Presence", discord::on(), [this](bool v) { discord::setOn(v); this->later(false); });
        txt(discord::status());
        sec("INTERFACE");
        tog("Button In Levels", c.btnInLevel, [](bool v) { cfg().btnInLevel = v; saveCfg(); });
        tog("Animations", c.animations, [this](bool v) { cfg().animations = v; saveCfg(); this->later(false); });
        num("UI Style", [] { return std::string(cfg().uiStyle ? "Paper" : "Glass"); }, [this](int) {
            cfg().uiStyle = 1 - cfg().uiStyle; saveCfg();
            this->restyle(); });
        sec("ACCENT COLOR");
        colorRows(&c.accR, &c.accG, &c.accB, [this] { cfg().themeIdx = -1; saveCfg(); this->defer([this] { this->themeLive(); }); }, "Accent");
        sec("PANEL TINT");
        colorRows(&c.tintR, &c.tintG, &c.tintB, [this] { cfg().themeIdx = -1; saveCfg(); this->defer([this] { this->themeLive(); }); }, "Panel tint");
        pills({{"Reset Theme", [this] {
            auto& c2 = cfg();
            c2.themeIdx = -1; c2.accR = 46; c2.accG = 134; c2.accB = 255; c2.tintR = 13; c2.tintG = 20; c2.tintB = 28;
            saveCfg();
            this->defer([this] { this->themeFull(); }); }}});
        break;
    }
    }

    if (m_paper) {
        std::vector<Row> keep;
        bool closed = false;
        for (auto& rw : rows) {
            if (rw.k == Row::Section) {
                auto key = fmt::format("{}:{}:{}", m_tab, m_sub, rw.label);
                closed = closedSecs().count(key) > 0;
                rw.on = closed; rw.help = key;
                keep.push_back(std::move(rw));
            } else if (!closed) keep.push_back(std::move(rw));
        }
        rows = std::move(keep);
    }
    float cw = m_cw, ch = m_ch;
    float rowH = m_paper ? 26.f : ROW, secH = m_paper ? 24.f : 22.f;
    auto hOf = [&](Row const& r) {
        if (r.k == Row::Section) return secH;
        if (!m_paper && r.k == Row::Num && r.setv && r.hi > r.lo && r.val) return rowH + 2.f + 14.f;
        return rowH + 2.f;
    };
    std::vector<float> rowX(rows.size(), 0.f), rowW(rows.size(), cw), rowY(rows.size(), 0.f);
    float total = 8.f;
    if (m_paper ? cw < 500.f : cw < 330.f) {
        for (auto& rw : rows) total += hOf(rw);
        total = std::max(total + 6.f, ch);
        float yy = total - 4.f;
        for (size_t i = 0; i < rows.size(); ++i) { float h = hOf(rows[i]); rowY[i] = yy - h / 2.f; yy -= h; }
    } else {
        float gap = 10.f, colW = (cw - gap) / 2.f;
        struct G { size_t a, b; bool wide; };
        std::vector<G> gs;
        for (size_t i = 0; i < rows.size();) {
            size_t j = i + 1;
            while (j < rows.size() && rows[j].k != Row::Section) ++j;
            bool wide = false;
            for (size_t k = i; k < j; ++k) {
                auto& r = rows[k];
                if (r.k == Row::Custom || r.k == Row::Text) wide = true;
                if (r.k == Row::Pills && (float)r.pills.size() * 72.f + 12.f > colW) wide = true;
            }
            gs.push_back({i, j, wide});
            i = j;
        }
        float colY[2] = {4.f, 4.f};
        bool first = true;
        for (auto& g : gs) {
            float extra = first ? 0.f : 6.f;
            first = false;
            int c = 0; float x = 0.f, w = cw;
            if (g.wide) { float st = std::max(colY[0], colY[1]) + extra; colY[0] = colY[1] = st; }
            else { c = colY[1] < colY[0] ? 1 : 0; colY[c] += extra; x = c ? colW + gap : 0.f; w = colW; }
            for (size_t k = g.a; k < g.b; ++k) {
                float h = hOf(rows[k]);
                rowX[k] = x; rowW[k] = w; rowY[k] = -(colY[c] + h / 2.f);
                colY[c] += h;
            }
            if (g.wide) colY[0] = colY[1] = colY[c];
        }
        total = std::max(std::max(colY[0], colY[1]) + 10.f, ch);
        for (auto& yv : rowY) yv += total;
    }
    content->setContentSize({cw, total});

    int idx = 0;
    for (auto& rw : rows) {
        float h = hOf(rw);
        float cy = rowY[idx];
        float const px = rowX[idx];
        float const cw = rowW[idx];
        auto rm = CCMenu::create();
        rm->setPosition({px, cy});
        rm->setContentSize({cw, h});
        content->addChild(rm);

        if (rw.k == Row::Section && m_paper) {
            auto node = CCNode::create();
            node->setContentSize({cw, secH});
            auto tri = CCDrawNode::create();
            poly(tri, {{-4.f, 3.2f}, {4.f, 3.2f}, {0.f, -3.6f}}, pGray(0.85f));
            tri->setPosition({14, secH / 2});
            tri->setRotation(rw.on ? -90.f : 0.f);
            node->addChild(tri);
            auto l = mkLabelBold(rw.label.c_str());
            sc(l, .36f); l->setAnchorPoint({0, .5f}); l->setPosition({28, secH / 2});
            node->addChild(l);
            auto key = rw.help;
            auto it = CCMenuItemExt::createSpriteExtra(node, [this, key](auto) {
                auto& s = closedSecs();
                if (!s.erase(key)) s.insert(key);
                this->later(false);
            });
            it->setPosition({cw / 2, 0});
            rm->addChild(it);
        } else if (rw.k == Row::Section) {
            auto l = mkLabel(rw.label.c_str());
            sc(l, .38f); l->setOpacity(235); l->setAnchorPoint({0, .5f}); l->setPosition({4, -2});
            rm->addChild(l);
        } else {
            auto bg = CCDrawNode::create();
            if (m_paper) bg->drawSegment({0, -rowH / 2.f}, {cw, -rowH / 2.f}, 0.5f, pm(ccc4f(1, 1, 1, 0.07f)));
            else rrect(bg, {0, -(h - 2.f) / 2.f}, {cw, h - 2.f}, 6, ccc4f(0, 0, 0, 0.26f), 1.f, ccc4f(1, 1, 1, 0.05f));
            rm->addChild(bg, -1);
            switch (rw.k) {
            case Row::Toggle: {
                auto l = mkLabel(rw.label.c_str());
                float lx = m_paper ? 38.f : 12.f;
                fit(l, m_paper ? cw - (rw.gear.empty() ? 120.f : 150.f) : cw - 24.f - 48.f - (rw.gear.empty() ? 0.f : 28.f) - (rw.help.empty() ? 0.f : 14.f), .4f, .1f); l->setAnchorPoint({0, .5f}); l->setPosition({lx, 0});
                rm->addChild(l);
                if (!rw.help.empty()) {
                    auto hb = makeHelp(rw.label, rw.help);
                    hb->setPosition({lx + l->getScaledContentSize().width + (m_paper ? 16.f : 13.f), 0}); rm->addChild(hb);
                }
                auto sw = makeSwitch(rw.on, rw.setb);
                sw->setPosition({m_paper ? 22.f : cw - 24.f, 0}); rm->addChild(sw);
                if (!rw.gear.empty()) {
                    auto key = rw.gear;
                    auto g = makeGear([this, key] { m_sub = key; this->later(true); });
                    g->setPosition({m_paper ? cw - 20.f : cw - 58.f, 0}); rm->addChild(g);
                }
                break;
            }
            case Row::Num: {
                auto l = mkLabel(rw.label.c_str());
                bool sl = !m_paper && rw.setv && rw.hi > rw.lo && (bool)rw.val;
                bool dd = !m_paper && !rw.opts.empty();
                bool narrow = !m_paper && cw < 250.f;
                bool showPM = !(narrow && rw.setv) && !sl && !dd;
                float ty = sl ? 11.f : 0.f;
                float vx = sl ? cw - 40.f : (narrow ? (rw.setv ? cw - 42.f : cw - 47.f) : cw - 74.f);
                float vw = sl ? 50.f : ((narrow && !rw.setv) ? 30.f : 70.f);
                float boxW = sl ? 60.f : 74.f;
                float minX = narrow ? cw - 80.f : cw - 122.f, plX = narrow ? cw - 14.f : cw - 26.f;
                float labLim = !narrow ? cw - 215.f : (rw.setv ? cw - 99.f : cw - 100.f) - (rw.help.empty() ? 0.f : 14.f);
                if (sl) labLim = cw - 12.f - 70.f - (rw.help.empty() ? 0.f : 14.f);
                if (dd) labLim = cw - 12.f - 98.f - (rw.help.empty() ? 0.f : 14.f);
                fit(l, labLim, .4f, .1f); l->setAnchorPoint({0, .5f}); l->setPosition({12, ty});
                rm->addChild(l);
                if (!rw.help.empty()) {
                    auto hb = makeHelp(rw.label, rw.help);
                    hb->setPosition({12.f + l->getScaledContentSize().width + 13.f, ty}); rm->addChild(hb);
                }
                if (dd) {
                    float bw = 86.f;
                    auto box = CCNode::create();
                    box->setContentSize({bw, 20});
                    auto bd = CCDrawNode::create();
                    rrect(bd, {0, 0}, {bw, 20}, 5, ccc4f(0, 0, 0, 0.30f), 1.f, ccc4f(1, 1, 1, 0.14f));
                    poly(bd, {{bw - 15.f, 12.f}, {bw - 7.f, 12.f}, {bw - 11.f, 7.5f}}, ccc4f(1, 1, 1, 0.85f));
                    box->addChild(bd);
                    auto cur = mkLabel(rw.get().c_str());
                    sc(cur, .36f); fit(cur, bw - 28.f, .36f, .1f);
                    cur->setAnchorPoint({0, .5f}); cur->setPosition({8, 10});
                    box->addChild(cur);
                    auto title = rw.label; auto opts = rw.opts; auto curf = rw.cur; auto pick = rw.pick;
                    Ref<XyzMenu> self = this;
                    auto bi = CCMenuItemExt::createSpriteExtra(box, [self, title, opts, curf, pick](auto) {
                        XyzChoicePopup::open(title, opts, curf(), [self, pick](int v) { pick(v); self->later(false); });
                    });
                    bi->setPosition({cw - 12.f - bw / 2.f, 0});
                    rm->addChild(bi);
                    break;
                }
                auto v = mkLabel(rw.get().c_str());
                if (m_paper) v->setColor(to3(accL()));
                sc(v, .42f); v->setPosition({vx, ty});
                fit(v, vw, .42f, .1f);
                rm->addChild(v);
                auto get = rw.get; auto step = rw.step;
                Ref<CCLabelBMFont> vr = v;
                Ref<TextInput> inRef;
                auto numOnly = [](std::string const& t) {
                    std::string o;
                    for (char ch : t) if (ch && std::strchr("0123456789.-", ch)) o += ch;
                    return o;
                };
                if (rw.setv) {
                    // type the value straight into the box
                    v->setVisible(false);
                    auto in = TextInput::create(boxW / .6f, "0", xyz::draw::fontOf());
                    in->setScale(.6f);
                    in->setFilter("0123456789.-");
                    in->setMaxCharCount(12);
                    in->setString(numOnly(get()), false);
                    auto setv = rw.setv;
                    in->setCallback([setv](std::string const& t) {
                        auto r = geode::utils::numFromString<double>(t);
                        if (r.isOk()) setv(r.unwrap());
                    });
                    in->setPosition({vx, ty});
                    rm->addChild(in, 1);
                    inRef = in;
                }
                auto fix = [vr, get, vw, inRef, numOnly] {
                    vr->setString(get().c_str()); fit(vr, vw, .42f, .1f);
                    if (inRef) inRef->setString(numOnly(get()), false);
                };
                if (showPM) {
                    auto mi = makeRound("-", [step, fix] { step(-1); fix(); });
                    mi->setPosition({minX, 0}); rm->addChild(mi);
                    auto pl = makeRound("+", [step, fix] { step(1); fix(); });
                    pl->setPosition({plX, 0}); rm->addChild(pl);
                }
                if (sl) {
                    double lo = rw.lo, hi = rw.hi; auto setv = rw.setv; auto valf = rw.val;
                    float frac0 = (float)std::clamp((valf() - lo) / (hi - lo), 0.0, 1.0);
                    auto slider = XyzSlider::create(cw - 24.f, frac0, m_scroll, [lo, hi, setv, fix](float f) {
                        double step = (hi - lo) >= 50.0 ? 1.0 : ((hi - lo) >= 10.0 ? 0.1 : 0.01);
                        double v = lo + (double)f * (hi - lo);
                        v = std::round(v / step) * step;
                        setv(v); fix();
                    });
                    if (slider) { slider->setPosition({12.f, -h / 2.f + 4.f}); rm->addChild(slider, 2); }
                }
                break;
            }
            case Row::Text: {
                auto l = mkLabel(rw.label.c_str());
                sc(l, .4f); l->setOpacity(190); l->setAnchorPoint({0, .5f}); l->setPosition({12, 0});
                l->setWidth((cw - 24.f) / (.4f * kOf(l)));
                rm->addChild(l);
                break;
            }
            case Row::Pills: {
                float x = 10.f;
                for (auto& [t, f] : rw.pills) {
                    auto p = makePill(t, t == "Record" || t == "Step", f, 64.f);
                    p->setAnchorPoint({0, .5f});
                    p->setPosition({x, 0}); rm->addChild(p);
                    x += p->getContentSize().width + 8.f;
                }
                break;
            }
            case Row::Custom: rw.custom(rm, 0.f, cw); break;
            default: break;
            }
        }

        if (animate) {
            rm->setPositionX(px + 9.f);
            rm->setCascadeOpacityEnabled(true);
            rm->setOpacity(0);
            rm->runAction(CCSequence::create(
                CCDelayTime::create(std::min(idx, 8) * 0.01f),
                CCSpawn::create(CCEaseExponentialOut::create(CCMoveTo::create(.16f, {px, cy})), CCFadeIn::create(.14f), nullptr),
                nullptr));
        }
        ++idx;
    }
    m_scroll->moveToTop();
    this->noAnim(content);
}

#if !defined(GEODE_IS_IOS)
class $modify(XyzKeys, CCKeyboardDispatcher) {
    bool dispatchKeyboardMSG(enumKeyCodes key, bool down, bool repeat, double ts) {
        if (down && !repeat) {
            if (key == KEY_F6) { XyzMenu::toggle(); return true; }
            if (key == KEY_F7 && PlayLayer::get()) { stepOnce(); return true; }
            if (key == KEY_F9 && PlayLayer::get()) { stepBack(1); return true; }
        }
        return CCKeyboardDispatcher::dispatchKeyboardMSG(key, down, repeat, ts);
    }
};
#endif

$on_mod(Loaded) { loadCfg(); }
