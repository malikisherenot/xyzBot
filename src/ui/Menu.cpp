#include "Menu.hpp"
#include "../core/Updater.hpp"
#include <set>
#include "Glass.hpp"
#include "Draw.hpp"
#include "../core/State.hpp"
#include "../vis/Vis.hpp"
#if !defined(GEODE_IS_IOS)
#include <Geode/modify/CCKeyboardDispatcher.hpp>
#endif
#include <memory>
#include <Geode/ui/TextInput.hpp>
#include <map>

using namespace geode::prelude;
using namespace xyz;
using namespace xyz::draw;

static XyzMenu* s_open = nullptr;
static constexpr float SW = 98.f;
static constexpr float ROW = 30.f;
static int s_startTab = 0;
static bool paper() { return cfg().uiStyle == 1; }
static std::set<std::string>& closedSecs() { static std::set<std::string> s; return s; }
static ccColor4F pGray(float v, float a = 1.f) { return {v, v, v, a}; }

static ccColor4F acc(float a = 1.f, float mul = 1.f) {
    auto& c = cfg();
    return {c.accR / 255.f * mul, c.accG / 255.f * mul, c.accB / 255.f * mul, a};
}
static ccColor4F accL(float a = 1.f) { return mix(acc(a), {1, 1, 1, a}, 0.5f); }
static ccColor3B to3(ccColor4F c) { return {(GLubyte)(c.r * 255), (GLubyte)(c.g * 255), (GLubyte)(c.b * 255)}; }
static ccColor3B tint3() { auto& c = cfg(); return {(GLubyte)c.tintR, (GLubyte)c.tintG, (GLubyte)c.tintB}; }

struct Preset { char const* n; int ar, ag, ab, tr, tg, tb; };
static Preset const PRESETS[] = {
    {"Emerald", 0, 217, 153, 13, 20, 28},   {"Ocean", 60, 170, 255, 10, 18, 32},  {"Violet", 170, 110, 255, 20, 14, 34},
    {"Sunset", 255, 140, 60, 30, 16, 14},   {"Rose", 255, 90, 150, 30, 12, 22},   {"Gold", 255, 205, 60, 26, 22, 10},
    {"Mono", 210, 215, 225, 18, 18, 20},
};
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
            rrect(bx, {0, 0}, {18, 18}, 3, pGray(0.2f), 1.f, *st ? accL(1.f) : pGray(0.42f));
            if (*st) {
                ck->drawSegment({4.5f, 9.f}, {7.8f, 5.4f}, 1.1f, pm(accL(1.f)));
                ck->drawSegment({7.8f, 5.4f}, {13.5f, 12.6f}, 1.1f, pm(accL(1.f)));
            }
        };
        draw();
        return CCMenuItemExt::createSpriteExtra(node, [st, draw, cb, node](auto) {
            *st = !*st; draw();
            node->stopAllActions();
            node->setScale(.8f);
            node->runAction(CCEaseBackOut::create(CCScaleTo::create(.22f, 1.f)));
            cb(*st);
        });
    }
    auto node = CCNode::create();
    node->setContentSize({42, 22});
    auto tr = CCDrawNode::create(); node->addChild(tr);
    auto kn = CCDrawNode::create(); node->addChild(kn, 1);
    auto st = std::make_shared<bool>(on);
    auto draw = [tr, kn, st] {
        bool v = *st;
        tr->clear(); kn->clear();
        rrect(tr, {0, 2}, {42, 18}, 9, v ? acc(0.95f, 0.8f) : ccc4f(0.03f, 0.04f, 0.06f, 0.97f), 1.4f, v ? accL(0.95f) : ccc4f(1, 1, 1, 0.55f));
        disc(kn, {0, 0}, 7.f, v ? ccc4f(1, 1, 1, 1) : ccc4f(0.6f, 0.64f, 0.7f, 1.f), 0.f, {0, 0, 0, 0}, 20);
    };
    draw();
    kn->setPosition({on ? 32.f : 10.f, 11.f});
    return CCMenuItemExt::createSpriteExtra(node, [st, draw, cb, kn](auto) {
        *st = !*st; draw();
        kn->stopAllActions();
        kn->runAction(CCEaseBackOut::create(CCMoveTo::create(.18f, {*st ? 32.f : 10.f, 11.f})));
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
    if (paper()) rrect(d, {0, 0}, {w, 20}, 3, accent ? acc(1.f, 0.6f) : pGray(0.2f), 1.f, accent ? accL(0.9f) : pGray(0.42f));
    else rrect(d, {0, 0}, {w, 20}, 10, accent ? acc(0.9f, 0.75f) : ccc4f(1, 1, 1, 0.12f), 1.f, accent ? accL(0.8f) : ccc4f(1, 1, 1, 0.25f));
    node->addChild(d);
    lbl->setPosition({w / 2, 10});
    node->addChild(lbl);
    return CCMenuItemExt::createSpriteExtra(node, [cb](auto) { cb(); });
}

static CCMenuItemSpriteExtra* makeRound(char const* glyph, std::function<void()> cb) {
    auto node = CCNode::create();
    node->setContentSize({20, 20});
    auto d = CCDrawNode::create();
    if (paper()) rrect(d, {0, 0}, {20, 20}, 3, pGray(0.2f), 1.f, pGray(0.42f));
    else disc(d, {10, 10}, 10.f, ccc4f(1, 1, 1, 0.13f), 1.f, ccc4f(1, 1, 1, 0.3f), 24);
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
        rrect(d, {0, 0}, {22, 20}, 3, pGray(0.2f), 1.f, pGray(0.42f));
        poly(d, {{8.f, 5.5f}, {8.f, 14.5f}, {14.5f, 10.f}}, pGray(0.85f));
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
        {"Frame Stepper", "Freezes the game and only advances when you step. On touch devices use the on-screen controls, on desktop press F7."},
        {"On-Screen Controls", "Shows Step, +N, Hold and Run buttons at the bottom of the screen while the stepper is on."},
        {"Steps Per +N Button", "How many ticks the +N button advances at once."},
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
        {"Speed", "Game speed multiplier. 1.00 is normal."},
        {"Speedhack Audio", "Changes the music pitch together with the speed."},
        {"SSB Fix (song sync)", "While the scroll speed bug is happening, changes the song speed so it stays in sync with the level."},
        {"Invert Correction", "Flips the direction of the song correction. Turn on if the song drifts further off instead of back."},
        {"Legacy Ship Weight (2.1)", "Makes the upside-down ship heavier, like in 2.1 and earlier."},
        {"Extra Weight (%)", "How much extra weight the upside-down ship gets. Starts at 6."},
        {"Show Hitboxes", "Draws the hitboxes of the player and the level's objects."},
        {"Hitbox Trail", "Draws the player's hitboxes from the last few moments."},
        {"Trail Length", "How many past ticks the hitbox trail keeps."},
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
        {"Font", "Arial or the game's own font."},
        {"Size", "How large the label is."},
        {"Opacity", "How see-through the label is."},
        {"Red", "Red amount, from 0 to 255."}, {"Green", "Green amount, from 0 to 255."}, {"Blue", "Blue amount, from 0 to 255."},
        {"Take Over On Input", "While a macro plays, your first tap takes control: the macro stops there and keeps recording what you do. The part already played is kept."},
        {"Continue After Finish", "When a macro reaches its end, it keeps recording instead of stopping, so you can add more to it."},
        {"Fix Rotation Bug", "Fixes the ship, UFO, wave and swing tilt looking wrong at high TPS. Only changes how it looks, not the physics."},
        {"Wave Drag Fix", "Fixes the wave trail on D blocks (drag), slopes, portals and spider orbs, also at high TPS. Only changes the trail, not the physics."},
        {"Trajectory Autoclicker", "Every tick it simulates what happens if the current input state continues. If that dies within the lookahead, it holds, releases or taps instead, whichever survives longest. Works while recording, so the clicks are saved in the macro. Orbs, pads and portals are not simulated yet."},
        {"Click Lookahead", "How many ticks ahead the Trajectory Autoclicker looks. 1 only reacts to death on the next tick (Echo default). Higher reacts earlier but costs more per tick."},
        {"Profiler", "Times what a tick costs and where it goes, and estimates how much faster running many ticks in one update call could be. Show the results with the Profiler labels (Labels page). Costs a little speed itself, turn it off when you are done."},
        {"Check For Updates On Start", "Once a day, when the game starts, checks GitHub for a newer xyzBot and tells you if there is one. Nothing is downloaded until you press Download."},
        {"UI Style", "Glass is the blurred panel with the tabs on the side. Paper is a plain flat panel with the tabs on top. The menu reopens when you switch."},
        {"Accuracy", "Vanilla: no position corrections. Input Fixes: the player position is stored and restored at every click. Frame Fixes: stored and restored every tick. A macro plays with whatever fixes it contains, so macros from xdBot load in any mode."},
        {"Fast Sub-Ticks", "At high TPS, skips the progress bar update on the ticks of a frame that are never drawn. Does not change the physics."},
        {"Skip Visibility On Sub-Ticks", "Experimental. Also skips the object visibility refresh on ticks that are never drawn. Faster on big levels, turn off if anything looks wrong."},
        {"Record Players", "Which players get recorded. Pick one player to record the other later and merge the two macros."},
        {"Play Players", "Which players the macro controls during playback. You control the other player yourself."},
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
        {"Swift Clicks", "The autoclicker taps (press and release at once) instead of holding."},
        {"Straight Fly P1", "Use Auto Straight Fly on Player 1."}, {"Straight Fly P2", "Use Auto Straight Fly on Player 2."},
        {"Fly Threshold P1", "How much vertical speed is allowed before the ship is corrected."}, {"Fly Threshold P2", "How much vertical speed is allowed before the ship is corrected."},
        {"Straight UFO P1", "Use Auto Straight UFO on Player 1."}, {"Straight UFO P2", "Use Auto Straight UFO on Player 2."},
        {"UFO Target Y P1", "The height Player 1's UFO is held at."}, {"UFO Target Y P2", "The height Player 2's UFO is held at."},
        {"UFO Threshold P1", "How far Player 1's UFO may drift from the target."}, {"UFO Threshold P2", "How far Player 2's UFO may drift from the target."},
        {"Macro To Render", "The macro that plays while rendering. 'None' lets you play the level yourself."},
        {"Width", "Video width in pixels. The height follows the shape of your screen."},
        {"FPS", "Frames per second of the video."},
        {"Bitrate (Mbps)", "Video quality. Higher looks better and makes a bigger file."},
        {"Codec", "Video encoder. Auto picks the best one your device offers."},
        {"End Tail (s)", "How many seconds keep recording after the level ends."},
        {"Include Song Audio", "Adds the level's song to the video afterwards. It starts at the beginning of the song, so it may be off for StartPos or practice runs."},
        {"Menu Font", "Switches the menu text between GD's own font (old) and Arial (new)."},
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
    if (k == "Steps Per +N Button") return I(&c.stepN, 1, 240);
    if (k == "Hold Speed (ticks/frame)") return I(&c.holdRate, 1, 16);
    if (k == "Extra Weight (%)") return I(&c.legacyShipPct, 0, 100);
    if (k == "Trail Length") return I(&c.trailLen, 10, 2000);
    if (k == "Length") return I(&c.trajLen, 10, 2000);
    if (k == "Lookahead Ticks") return I(&c.pdLook, 1, 10);
    if (k == "Click Lookahead") return I(&c.trajLook, 1, 240);
    if (k == "Interval (s)") return I(&c.autosaveSec, 10, 3600);
    if (k == "Width") return I(&c.renderW, 320, 3840);
    if (k == "FPS") return I(&c.renderFps, 10, 240);
    if (k == "Bitrate (Mbps)") return I(&c.renderBitrate, 1, 200);
    if (k == "End Tail (s)") return I(&c.renderTail, 0, 30);
    return nullptr;
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
        m_input = TextInput::create(180.f, "Value", "bigFont.fnt");
        m_input->setFilter("0123456789.-");
        m_input->setString(cur);
        m_input->setPosition({125.f, 72.f});
        m_mainLayer->addChild(m_input);
        auto menu = CCMenu::create();
        menu->setPosition({0, 0});
        m_mainLayer->addChild(menu);
        auto ok = CCMenuItemExt::createSpriteExtra(ButtonSprite::create("Apply"), [this](auto) {
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

class XyzTextPopup : public geode::Popup {
    TextInput* m_input = nullptr;
    std::function<void(std::string)> m_cb;
public:
    static void open(std::string title, std::string cur, std::function<void(std::string)> cb) {
        auto p = new XyzTextPopup();
        if (p->setup(title, cur, std::move(cb))) { p->autorelease(); p->show(); }
        else delete p;
    }
    bool setup(std::string const& title, std::string const& cur, std::function<void(std::string)> cb) {
        m_cb = std::move(cb);
        if (!Popup::init(250.f, 135.f)) return false;
        this->setTitle(title);
        m_input = TextInput::create(180.f, "Type to search", "bigFont.fnt");
        m_input->setString(cur);
        m_input->setPosition({125.f, 72.f});
        m_mainLayer->addChild(m_input);
        auto menu = CCMenu::create();
        menu->setPosition({0, 0});
        m_mainLayer->addChild(menu);
        auto ok = CCMenuItemExt::createSpriteExtra(ButtonSprite::create("Search"), [this](auto) {
            m_cb(m_input->getString());
            this->onClose(nullptr);
        });
        ok->setPosition({125.f, 28.f});
        menu->addChild(ok);
        return true;
    }
};

XyzMenu* XyzMenu::create() {
    auto r = new XyzMenu();
    if (r->init()) { r->autorelease(); return r; }
    delete r;
    return nullptr;
}

XyzMenu::~XyzMenu() {
    if (s_open == this) s_open = nullptr;
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

void XyzMenu::drawPanel() {
    if (!m_panel) return;
    m_panel->clear();
    float h = m_collapsed ? 34.f : m_h;
    rrect(m_panel, {0, m_h - h}, {m_w, h}, 5, ccc4f(0.105f, 0.105f, 0.105f, 0.98f), 1.f, pGray(0.28f));
    if (!m_collapsed) {
        m_panel->drawSegment({1.f, m_h - 34.f}, {m_w - 1.f, m_h - 34.f}, 0.5f, pm(pGray(0.28f)));
        m_panel->drawSegment({1.f, m_h - 64.f}, {m_w - 1.f, m_h - 64.f}, 0.5f, pm(pGray(0.2f)));
    }
}

void XyzMenu::collapse() {
    m_collapsed = !m_collapsed;
    if (m_arrow) m_arrow->runAction(CCEaseExponentialOut::create(CCRotateTo::create(.22f, m_collapsed ? -90.f : 0.f)));
    this->drawPanel();
    m_scroll->setVisible(!m_collapsed);
    m_scroll->setTouchEnabled(!m_collapsed);
    m_side->setVisible(!m_collapsed);
}

void XyzMenu::defer(std::function<void()> f) {
    Ref<XyzMenu> self = this;
    queueInMainThread([self, f] { if (s_open == self.data()) f(); });
}
void XyzMenu::later(bool animate) { this->defer([this, animate] { this->rebuild(animate); }); }

void XyzMenu::themeLive() {
    if (m_glass) glass::setTheme(m_glass, to3(acc()), tint3());
    this->buildSide();
}
void XyzMenu::themeFull() { this->themeLive(); this->rebuild(false); }

bool XyzMenu::init() {
    auto ws = CCDirector::get()->getWinSize();
    m_paper = paper();
    m_tab = std::clamp(s_startTab, 0, 7);
    s_startTab = 0;
    m_w = std::min(ws.width - 30.f, 470.f);
    m_h = std::min(ws.height - 24.f, 280.f);
    m_cw = m_paper ? m_w - 20.f : m_w - SW - 30.f;
    m_ch = m_paper ? m_h - 34.f - 30.f - 8.f : m_h - 52.f;
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
        m_panel = CCDrawNode::create();
        m_mainLayer->addChild(m_panel, -1);
        this->drawPanel();
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
    m_scroll->setPosition(m_paper ? CCPoint{10.f, 8.f} : CCPoint{SW + 20.f, 12.f});
    m_mainLayer->addChild(m_scroll);

    m_side = CCNode::create();
    m_mainLayer->addChild(m_side);

    s_open = this;
    this->buildSide();
    this->rebuild(true);

    m_mainLayer->setScale(.92f);
    m_mainLayer->setPositionY(m_mainLayer->getPositionY() - 10.f);
    m_mainLayer->runAction(CCEaseExponentialOut::create(CCScaleTo::create(.26f, 1.f)));
    m_mainLayer->runAction(CCEaseExponentialOut::create(CCMoveBy::create(.26f, {0, 10.f})));
    this->schedule(schedule_selector(XyzMenu::fadeStep));
    this->schedule(schedule_selector(XyzMenu::pollUpdate), 0.4f);
    return true;
}

void XyzMenu::buildHead() {
    m_head->removeAllChildren();
    if (m_paper) {
        m_arrow = nullptr;
        auto title = mkLabel(fmt::format("xyzBot v{}", Mod::get()->getVersion().toVString().substr(1)).c_str());
        sc(title, .42f); title->setPosition({m_w / 2, m_h - 17}); title->setOpacity(210);
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
        ab->setPosition({20, m_h - 17});
        top->addChild(ab);
        auto x = makeRound("x", [this] { this->onClose(nullptr); });
        x->setPosition({m_w - 20, m_h - 17});
        top->addChild(x);
        return;
    }
    auto title = mkLabel("xyzBot");
    sc(title, .5f); title->setAnchorPoint({0, .5f}); title->setPosition({18, m_h - 20});
    m_head->addChild(title);
    auto sub = mkLabel(fmt::format("v{}", Mod::get()->getVersion().toVString().substr(1)).c_str());
    sc(sub, .5f); sub->setOpacity(150); sub->setAnchorPoint({0, .5f});
    sub->setPosition({title->getPositionX() + title->getScaledContentSize().width + 6, m_h - 22});
    m_head->addChild(sub);
    auto top = CCMenu::create();
    top->setPosition({0, 0});
    m_head->addChild(top);
    auto x = makeRound("x", [this] { this->onClose(nullptr); });
    x->setPosition({m_w - 20, m_h - 20});
    top->addChild(x);
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

void XyzMenu::pollUpdate(float) {
    int st = (int)updater::state();
    if (st == m_updSeen) return;
    m_updSeen = st;
    if (m_sub == "update" || (m_tab == 0 && m_sub.empty())) this->later(true);
}

void XyzMenu::onClose(CCObject*) {
    if (m_closing) return;
    m_closing = true;
    s_open = nullptr;
    m_mainLayer->stopAllActions();
    m_mainLayer->runAction(CCEaseExponentialIn::create(CCScaleTo::create(.14f, .93f)));
    this->schedule(schedule_selector(XyzMenu::fadeStep));
    this->scheduleOnce(schedule_selector(XyzMenu::doClose), .15f);
}
void XyzMenu::doClose(float) { Popup::onClose(nullptr); }

static char const* TABS[] = {"Bot", "Physics", "Visuals", "Macros", "Labels", "Assist", "Theme", "Render"};
static constexpr int NTABS = 8;
static char const* PLAYERS[] = {"Both", "Player 1 only", "Player 2 only"};
static char const* ACCURACY[] = {"Vanilla", "Input Fixes", "Frame Fixes"};
static float tabPitch(float menuH) { return std::min(29.f, (menuH - 52.f - 8.f) / NTABS); }

void XyzMenu::moveHl(bool animate) {
    if (!m_hl) return;
    if (m_paper && m_tab < (int)m_tabX.size()) {
        CCPoint p = {m_tabX[m_tab], m_h - 34.f - 26.f};
        float sx = m_tabW[m_tab] / 100.f;
        m_hl->stopAllActions();
        if (animate) m_hl->runAction(CCSpawn::create(CCEaseExponentialOut::create(CCMoveTo::create(.28f, p)), CCEaseExponentialOut::create(CCScaleTo::create(.28f, sx, 1.f)), nullptr));
        else { m_hl->setPosition(p); m_hl->setScaleX(sx); }
        for (int i = 0; i < (int)m_tabLabels.size(); ++i) {
            m_tabLabels[i]->stopAllActions();
            m_tabLabels[i]->runAction(CCFadeTo::create(.15f, i == m_tab ? 255 : 175));
        }
        return;
    }
    float h2 = m_h - 52.f;
    CCPoint p = {16.f, 12.f + h2 - 6.f - 26.f - m_tab * tabPitch(m_h)};
    m_hl->stopAllActions();
    if (animate) m_hl->runAction(CCEaseExponentialOut::create(CCMoveTo::create(.28f, p)));
    else m_hl->setPosition(p);
    for (int i = 0; i < (int)m_tabLabels.size(); ++i) {
        m_tabLabels[i]->stopAllActions();
        m_tabLabels[i]->runAction(CCFadeTo::create(.15f, i == m_tab ? 255 : 170));
    }
}

void XyzMenu::buildSide() {
    m_side->removeAllChildren();
    m_tabLabels.clear();
    if (m_paper) {
        m_tabX.clear(); m_tabW.clear();
        float s = .36f, avail = m_w - 20.f - (NTABS - 1) * 3.f, tot = 0.f;
        auto measure = [&](float sc0) {
            tot = 0.f;
            for (int i = 0; i < NTABS; ++i) {
                auto l = mkLabel(TABS[i]);
                sc(l, sc0);
                tot += l->getScaledContentSize().width + 16.f;
            }
        };
        measure(s);
        while (tot > avail && s > .2f) { s -= .01f; measure(s); }
        auto hl = CCDrawNode::create();
        rrect(hl, {0, 0}, {100, 22}, 3, acc(1.f, 0.6f), 1.f, accL(0.85f));
        m_side->addChild(hl);
        m_hl = hl;
        auto menu = CCMenu::create();
        menu->setPosition({0, 0});
        m_side->addChild(menu, 2);
        float x = 10.f, y = m_h - 34.f - 15.f;
        for (int i = 0; i < NTABS; ++i) {
            auto l = mkLabel(TABS[i]);
            sc(l, s);
            float w = l->getScaledContentSize().width + 16.f;
            auto node = CCNode::create();
            node->setContentSize({w, 22});
            l->setPosition({w / 2, 11});
            node->addChild(l);
            m_tabLabels.push_back(l);
            m_tabX.push_back(x); m_tabW.push_back(w);
            auto b = CCMenuItemExt::createSpriteExtra(node, [this, i](auto) {
                if (m_tab == i && m_sub.empty()) return;
                m_tab = i; m_sel = -1; m_sub.clear();
                this->moveHl(true);
                this->later(true);
            });
            b->setPosition({x + w / 2, y});
            menu->addChild(b);
            x += w + 3.f;
        }
        this->moveHl(false);
        return;
    }
    float h2 = m_h - 52.f;
    auto d = CCDrawNode::create();
    rrect(d, {10, 12}, {SW, h2}, 12, ccc4f(1, 1, 1, 0.05f), 1.f, ccc4f(1, 1, 1, 0.12f));
    m_side->addChild(d);
    auto hl = CCDrawNode::create();
    rrect(hl, {0, 0}, {SW - 12, 26}, 10, acc(0.75f, 0.8f), 1.f, accL(0.7f));
    m_side->addChild(hl);
    m_hl = hl;
    auto menu = CCMenu::create();
    menu->setPosition({0, 0});
    m_side->addChild(menu, 2);
    for (int i = 0; i < NTABS; ++i) {
        auto node = CCNode::create();
        node->setContentSize({SW - 12, 26});
        auto l = mkLabel(TABS[i]);
        sc(l, .4f); l->setPosition({(SW - 12) / 2, 13});
        node->addChild(l);
        m_tabLabels.push_back(l);
        auto b = CCMenuItemExt::createSpriteExtra(node, [this, i](auto) {
            if (m_tab == i && m_sub.empty()) return;
            m_tab = i; m_sel = -1; m_sub.clear();
            this->moveHl(true);
            this->later(true);
        });
        b->setPosition({10 + SW / 2, 12.f + h2 - 6.f - 13.f - i * tabPitch(m_h)});
        menu->addChild(b);
    }
    this->moveHl(false);
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
};
}

void XyzMenu::rebuild(bool animate) {
    auto content = m_scroll->m_contentLayer;
    content->removeAllChildren();
    auto& c = cfg(); auto& r = rt();
    std::vector<Row> rows;

    auto sec = [&](std::string t) { Row x; x.k = Row::Section; x.label = std::move(t); rows.push_back(std::move(x)); };
    auto tog = [&](std::string t, bool v, std::function<void(bool)> f, std::string gear = "") { Row x; x.k = Row::Toggle; x.label = std::move(t); x.on = v; x.setb = std::move(f); x.gear = std::move(gear); x.help = helpText(x.label); rows.push_back(std::move(x)); };
    auto numS = [&](std::string t, std::function<std::string()> g, std::function<void(int)> s, std::function<void(double)> sv) { Row x; x.k = Row::Num; x.label = std::move(t); x.get = std::move(g); x.step = std::move(s); x.setv = std::move(sv); x.help = helpText(x.label); rows.push_back(std::move(x)); };
    auto num = [&](std::string t, std::function<std::string()> g, std::function<void(int)> s) { auto sv = numSetter(t); numS(std::move(t), std::move(g), std::move(s), std::move(sv)); };
    auto txt = [&](std::string t) { Row x; x.k = Row::Text; x.label = std::move(t); rows.push_back(std::move(x)); };
    auto pills = [&](std::vector<std::pair<std::string, std::function<void()>>> p) { Row x; x.k = Row::Pills; x.pills = std::move(p); rows.push_back(std::move(x)); };
    auto custom = [&](std::function<void(CCMenu*, float, float)> f) { Row x; x.k = Row::Custom; x.custom = std::move(f); rows.push_back(std::move(x)); };

    auto colorRows = [&](int* cr, int* cg, int* cb, std::function<void()> changed) {
        numS("Red", [cr] { return fmt::format("{}", *cr); }, [cr, changed](int d) { *cr = std::clamp(*cr + d * 15, 0, 255); changed(); }, [cr, changed](double v) { *cr = std::clamp((int)std::lround(v), 0, 255); changed(); });
        numS("Green", [cg] { return fmt::format("{}", *cg); }, [cg, changed](int d) { *cg = std::clamp(*cg + d * 15, 0, 255); changed(); }, [cg, changed](double v) { *cg = std::clamp((int)std::lround(v), 0, 255); changed(); });
        numS("Blue", [cb] { return fmt::format("{}", *cb); }, [cb, changed](int d) { *cb = std::clamp(*cb + d * 15, 0, 255); changed(); }, [cb, changed](double v) { *cb = std::clamp((int)std::lround(v), 0, 255); changed(); });
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
            sec("OPTIMIZATION");
            tog("Fast Sub-Ticks", c.fastSub, [](bool v) { cfg().fastSub = v; saveCfg(); });
            tog("Skip Visibility On Sub-Ticks", c.skipVis, [](bool v) { cfg().skipVis = v; saveCfg(); });
            sec("PROFILER");
            tog("Profiler", c.profiler, [](bool v) { cfg().profiler = v; rt().prof = Prof{}; saveCfg(); });
        } else if (sub == "update") {
            auto st = updater::state();
            sec("UPDATE " + updater::latest());
            txt(updater::message());
            if (st == updater::State::Available || st == updater::State::Failed) {
                std::vector<std::pair<std::string, std::function<void()>>> p;
                if (st == updater::State::Available) p.push_back({"Download", [this] { updater::download(); this->later(true); }});
                p.push_back({"Open Releases", [] { geode::utils::web::openLinkInBrowser(updater::releasesUrl()); }});
                pills(std::move(p));
            } else if (st == updater::State::Done) {
                pills({{"Restart Game", [] { updater::restart(); }}});
            }
            sec("CHANGELOG");
            for (auto const& l : updater::notes()) txt(l);
        } else if (sub == "speed") {
            sec("SPEEDHACK SETTINGS");
            num("Speed", [] { return fmt::format("{:.2f}", cfg().speedVal); }, [](int d) { cfg().speedVal = std::clamp(cfg().speedVal + d * 0.1f, 0.05f, 100.f); saveCfg(); });
            tog("Speedhack Audio", c.speedAudio, [](bool v) { cfg().speedAudio = v; saveCfg(); });
        } else if (sub == "noclip") {
            sec("NOCLIP SETTINGS");
            tog("Noclip Player 1", c.noclipP[0], [](bool v) { cfg().noclipP[0] = v; saveCfg(); });
            tog("Noclip Player 2", c.noclipP[1], [](bool v) { cfg().noclipP[1] = v; saveCfg(); });
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
            colorRows(&c.layoutBg[0], &c.layoutBg[1], &c.layoutBg[2], [] { saveCfg(); });
            sec("GROUND COLOR");
            colorRows(&c.layoutGr[0], &c.layoutGr[1], &c.layoutGr[2], [] { saveCfg(); });
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
            num("Trail Length", [] { return fmt::format("{}", cfg().trailLen); }, [](int d) { cfg().trailLen = std::clamp(cfg().trailLen + d * 50, 10, 2000); saveCfg(); });
        } else if (sub == "traj") {
            sec("TRAJECTORY SETTINGS");
            num("Length", [] { return fmt::format("{}", cfg().trajLen); }, [](int d) { cfg().trajLen = std::clamp(cfg().trajLen + d * 30, 10, 2000); saveCfg(); });
        } else if (sub == "pd") {
            sec("PREVENT DEATH SETTINGS");
            num("Lookahead Ticks", [] { return fmt::format("{}", cfg().pdLook); }, [](int d) { cfg().pdLook = std::clamp(cfg().pdLook + d, 1, 10); saveCfg(); });
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
            num("Font", [i] { return std::string(labels::cfg(i).font == 0 ? "GD Big" : "Arial"); }, [i](int) { auto& f = labels::cfg(i).font; f = 1 - f; labels::save(i); });
            numS("Size", [i] { return fmt::format("{:.2f}", labels::cfg(i).scale); }, [i](int d) { auto& s = labels::cfg(i).scale; s = std::clamp(s + d * 0.05f, 0.1f, 2.f); labels::save(i); }, [i](double v) { labels::cfg(i).scale = std::clamp((float)v, 0.1f, 2.f); labels::save(i); });
            numS("Opacity", [i] { return fmt::format("{:.2f}", labels::cfg(i).opacity); }, [i](int d) { auto& o = labels::cfg(i).opacity; o = std::clamp(o + d * 0.1f, 0.1f, 1.f); labels::save(i); }, [i](double v) { labels::cfg(i).opacity = std::clamp((float)v, 0.1f, 1.f); labels::save(i); });
            sec("COLOR");
            labelChips([i](int k) { auto& l = labels::cfg(i); return l.r == LABEL_COLORS[k][0] && l.g == LABEL_COLORS[k][1] && l.b == LABEL_COLORS[k][2]; },
                       [i](int k) { auto& l = labels::cfg(i); l.r = LABEL_COLORS[k][0]; l.g = LABEL_COLORS[k][1]; l.b = LABEL_COLORS[k][2]; labels::save(i); });
            auto& l = labels::cfg(i);
            colorRows(&l.r, &l.g, &l.b, [i] { labels::save(i); });
        }
    } else switch (m_tab) {
    case 0: {
        std::string st = r.mode == Mode::Record ? "Recording" : r.mode == Mode::Play ? "Playing" : "Idle";
        sec("BOT");
        txt("State: " + st);
        pills({{"Record", [this] { startRecord(); this->later(false); }}, {"Stop", [this] { stopAll(true); this->later(false); }}, {"Step", [] { stepOnce(); }},
               {"Take Over", [this] {
                    if (takeOverNow()) this->onClose(nullptr);
                    else Notification::create("Nothing is playing", NotificationIcon::Error)->show(); }}});
        tog("Frame Stepper", c.stepper, [](bool v) { cfg().stepper = v; }, "stepper");
        tog("Noclip", c.noclip, [](bool v) { cfg().noclip = v; saveCfg(); }, "noclip");
        sec("ACCURACY");
        num("Accuracy", [] { return std::string(ACCURACY[std::clamp(cfg().accuracy, 0, 2)]); }, [](int d) { auto& v = cfg().accuracy; v = (v + d + 3) % 3; saveCfg(); });
        tog("Ignore Level Check", c.ignoreLevel, [](bool v) { cfg().ignoreLevel = v; saveCfg(); });
        tog("Block Inputs In Playback", c.blockInputs, [](bool v) { cfg().blockInputs = v; saveCfg(); });
        sec("RECORDING");
        num("Record Players", [] { return std::string(PLAYERS[std::clamp(cfg().recPlayers, 0, 2)]); }, [](int d) { auto& v = cfg().recPlayers; v = (v + d + 3) % 3; saveCfg(); });
        tog("Auto Name Replay", c.autoName, [](bool v) { cfg().autoName = v; saveCfg(); }, "naming");
        tog("No Overwrite", c.noOverwrite, [](bool v) { cfg().noOverwrite = v; saveCfg(); });
        sec("PLAYBACK");
        num("Play Players", [] { return std::string(PLAYERS[std::clamp(cfg().playPlayers, 0, 2)]); }, [](int d) { auto& v = cfg().playPlayers; v = (v + d + 3) % 3; saveCfg(); });
        tog("Take Over On Input", c.takeOver, [](bool v) { cfg().takeOver = v; saveCfg(); });
        tog("Continue After Finish", c.continueEnd, [](bool v) { cfg().continueEnd = v; saveCfg(); });
        tog("Sequence", c.seqOn, [](bool v) { cfg().seqOn = v; saveCfg(); }, "seq");
        sec("AUTOMATION");
        tog("Autosave While Recording", c.autosave, [](bool v) { cfg().autosave = v; saveCfg(); }, "autosave");
        tog("Auto Exit On Finish", c.autoExit, [](bool v) { cfg().autoExit = v; saveCfg(); });
        sec("UPDATES");
        {
            auto st = updater::state();
            txt(st == updater::State::Idle ? std::string("Not checked yet.") : updater::message());
            std::vector<std::pair<std::string, std::function<void()>>> p;
            p.push_back({"Check Now", [this] { updater::check(true); this->later(false); }});
            if (st == updater::State::Available) p.push_back({"Changelog & Download", [this] { m_sub = "update"; this->later(true); }});
            if (st == updater::State::Done) p.push_back({"Restart Game", [] { updater::restart(); }});
            pills(std::move(p));
        }
        tog("Check For Updates On Start", c.autoUpdate, [](bool v) { cfg().autoUpdate = v; saveCfg(); });
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
                        auto name = e.path().filename().string();
                        auto low = name; std::transform(low.begin(), low.end(), low.begin(), [](unsigned char ch) { return (char)std::tolower(ch); });
                        if (low.size() > 9 && low.compare(low.size() - 9, 9, ".gdr.json") == 0) name.resize(name.size() - 9);
                        else name = e.path().stem().string();
                        Macro m;
                        if (importMacro(m, e.path()) && saveMacro(m, name)) n++;
                        else { bad++; why = importError(); }
                    }
                    auto msg = fmt::format("Imported {}", n);
                    if (bad) msg += fmt::format(", {} failed{}", bad, why.empty() ? "" : " (" + why + ")");
                    Notification::create(msg, bad ? NotificationIcon::Warning : NotificationIcon::Success)->show();
                    this->later(false); }}});
        pills({{"Search", [this] { XyzTextPopup::open("Search Macros", m_query, [this](std::string q) { m_query = std::move(q); this->later(false); }); }},
               {"Clear Search", [this] { m_query.clear(); this->later(false); }}});
        sec("LIBRARY");
        auto names = listMacros();
        if (!m_query.empty()) {
            auto lower = [](std::string s) { std::transform(s.begin(), s.end(), s.begin(), [](unsigned char ch) { return (char)std::tolower(ch); }); return s; };
            auto q = lower(m_query); size_t total = names.size();
            names.erase(std::remove_if(names.begin(), names.end(), [&](std::string const& n) { return lower(n).find(q) == std::string::npos; }), names.end());
            txt(fmt::format("Search \"{}\": {} of {}", m_query, names.size(), total));
        }
        if (!m_mergeBase.empty()) txt("Merge base: " + m_mergeBase + ". Tap Merge on another macro, or on it again to cancel.");
        if (names.empty()) txt(m_query.empty() ? "No macros yet. Drop .gdr, .gdr.json, .xd, .slc or .txt files into the macros folder and press Import." : "No macro matches.");
        for (auto& n : names) {
            custom([this, n](CCMenu* menu, float y, float w) {
                std::error_code ec;
                auto sz = std::filesystem::file_size(macroDir() / (n + ".xyz"), ec);
                auto l = mkLabel(((m_mergeBase == n ? "* " : "") + n).c_str());
                fit(l, w - 215.f, .4f, .1f); l->setAnchorPoint({0, .5f}); l->setPosition({12, y + 4});
                menu->addChild(l);
                auto s = mkLabel(ec ? "" : (sz >= 1024 ? fmt::format("{:.1f} KB", sz / 1024.0) : fmt::format("{} B", sz)).c_str());
                sc(s, .26f); s->setOpacity(130); s->setAnchorPoint({0, .5f}); s->setPosition({12, y - 9});
                menu->addChild(s);
                std::vector<CCMenuItemSpriteExtra*> ps;
                ps.push_back(makePill("Del", false, [this, n] { std::error_code e2; std::filesystem::remove(macroDir() / (n + ".xyz"), e2); this->later(false); }));
                ps.push_back(makePill("Merge", false, [this, n] {
                    if (m_mergeBase.empty()) {
                        m_mergeBase = n;
                        Notification::create("Merge base set. Tap Merge on another macro.", NotificationIcon::Info)->show();
                    } else if (m_mergeBase == n) {
                        m_mergeBase.clear();
                    } else {
                        std::string out;
                        bool ok = mergeMacros(m_mergeBase, n, out);
                        Notification::create(ok ? "Merged into " + out : std::string("Merge failed (different TPS?)"), ok ? NotificationIcon::Success : NotificationIcon::Error)->show();
                        m_mergeBase.clear();
                    }
                    this->later(false); }));
                ps.push_back(makePill("+Seq", false, [this, n] {
                    cfg().seq.push_back(n); saveCfg();
                    Notification::create("Added to sequence", NotificationIcon::Success)->show(); }));
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
    case 7: {
        static char const* CODEC_NAMES[] = {"Auto", "libx264", "h264_mediacodec", "libopenh264", "mpeg4"};
        sec("RENDER");
        txt(isRendering() ? "Rendering... press Stop Render to finish." : renderAvailable() ? "Ready. Open a level, then press Start Render." : "Needs the FFmpeg API mod (eclipse.ffmpeg-api).");
        pills({{"Start Render", [this] {
                    auto err = startRender();
                    if (!err.empty()) { Notification::create(err, NotificationIcon::Error)->show(); return; }
                    Notification::create("Render started. Resume the level.", NotificationIcon::Info)->show();
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
        txt(fmt::format("Height: {} (follows the shape of your screen)", [] {
            auto ws = CCDirector::get()->getWinSize();
            return (int)std::lround(std::clamp(cfg().renderW, 320, 3840) / 2 * 2 * ws.height / ws.width) / 2 * 2;
        }()));
        num("Width", [] { return fmt::format("{}", cfg().renderW); }, [](int d) { cfg().renderW = std::clamp(cfg().renderW + d * 160, 320, 3840); saveCfg(); });
        num("FPS", [] { return fmt::format("{}", cfg().renderFps); }, [](int d) { cfg().renderFps = std::clamp(cfg().renderFps + d * 10, 10, 240); saveCfg(); });
        num("Bitrate (Mbps)", [] { return fmt::format("{}", cfg().renderBitrate); }, [](int d) { cfg().renderBitrate = std::clamp(cfg().renderBitrate + d, 1, 200); saveCfg(); });
        num("Codec", [] { return std::string(CODEC_NAMES[std::clamp(cfg().renderCodec, 0, 4)]); }, [](int d) { auto& v = cfg().renderCodec; v = (v + d + 5) % 5; saveCfg(); });
        num("End Tail (s)", [] { return fmt::format("{}", cfg().renderTail); }, [](int d) { cfg().renderTail = std::clamp(cfg().renderTail + d, 0, 30); saveCfg(); });
        tog("Include Song Audio", c.renderAudio, [](bool v) { cfg().renderAudio = v; saveCfg(); });
        break;
    }
    default: {
        sec("INTERFACE");
        tog("Button In Levels", c.btnInLevel, [](bool v) { cfg().btnInLevel = v; saveCfg(); });
        num("UI Style", [] { return std::string(cfg().uiStyle ? "Paper" : "Glass"); }, [this](int) {
            cfg().uiStyle = 1 - cfg().uiStyle; saveCfg();
            this->restyle(); });
        num("Menu Font", [] { return std::string(menuFontNow() ? "Arial (new)" : "GD (old)"); }, [this](int) {
            auto& f = cfg().uiStyle ? cfg().fontPaper : cfg().fontGlass;
            f = 1 - f; saveCfg();
            this->defer([this] { this->applyFont(); }); });
        sec("PRESETS");
        custom([this](CCMenu* menu, float y, float w) {
            int n = (int)(sizeof(PRESETS) / sizeof(PRESETS[0]));
            for (int i = 0; i < n; ++i) {
                auto& p = PRESETS[i];
                auto ch = makeChip({(GLubyte)p.ar, (GLubyte)p.ag, (GLubyte)p.ab}, cfg().themeIdx == i, [this, i] {
                    auto& p2 = PRESETS[i]; auto& c2 = cfg();
                    c2.themeIdx = i; c2.accR = p2.ar; c2.accG = p2.ag; c2.accB = p2.ab; c2.tintR = p2.tr; c2.tintG = p2.tg; c2.tintB = p2.tb;
                    saveCfg();
                    this->defer([this] { this->themeFull(); });
                });
                ch->setPosition({22.f + i * ((w - 44.f) / (n - 1)), y});
                menu->addChild(ch);
            }
        });
        sec("ACCENT COLOR");
        colorRows(&c.accR, &c.accG, &c.accB, [this] { cfg().themeIdx = -1; saveCfg(); this->defer([this] { this->themeLive(); }); });
        sec("PANEL TINT");
        colorRows(&c.tintR, &c.tintG, &c.tintB, [this] { cfg().themeIdx = -1; saveCfg(); this->defer([this] { this->themeLive(); }); });
        pills({{"Reset Theme", [this] {
            auto& p = PRESETS[0]; auto& c2 = cfg();
            c2.themeIdx = 0; c2.accR = p.ar; c2.accG = p.ag; c2.accB = p.ab; c2.tintR = p.tr; c2.tintG = p.tg; c2.tintB = p.tb;
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
    float total = 8.f;
    for (auto& rw : rows) total += (rw.k == Row::Section ? secH : rowH + 2.f);
    total = std::max(total + 6.f, ch);
    content->setContentSize({cw, total});

    float y = total - 4.f;
    int idx = 0;
    for (auto& rw : rows) {
        float h = rw.k == Row::Section ? secH : rowH + 2.f;
        float cy = y - h / 2.f;
        auto rm = CCMenu::create();
        rm->setPosition({0, cy});
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
            auto l = mkLabel(rw.label.c_str());
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
            sc(l, .33f); l->setColor(to3(accL())); l->setAnchorPoint({0, .5f}); l->setPosition({8, -2});
            rm->addChild(l);
        } else {
            auto bg = CCDrawNode::create();
            if (m_paper) bg->drawSegment({0, -rowH / 2.f}, {cw, -rowH / 2.f}, 0.5f, pm(ccc4f(1, 1, 1, 0.07f)));
            else rrect(bg, {0, -ROW / 2.f}, {cw, ROW}, 9, ccc4f(1, 1, 1, 0.055f), 1.f, ccc4f(1, 1, 1, 0.10f));
            rm->addChild(bg, -1);
            switch (rw.k) {
            case Row::Toggle: {
                auto l = mkLabel(rw.label.c_str());
                float lx = m_paper ? 38.f : 12.f;
                fit(l, cw - (rw.gear.empty() ? 120.f : 150.f), .4f, .1f); l->setAnchorPoint({0, .5f}); l->setPosition({lx, 0});
                rm->addChild(l);
                if (!rw.help.empty()) {
                    auto hb = makeHelp(rw.label, rw.help);
                    hb->setPosition({lx + l->getScaledContentSize().width + (m_paper ? 16.f : 13.f), 0}); rm->addChild(hb);
                }
                auto sw = makeSwitch(rw.on, rw.setb);
                sw->setPosition({m_paper ? 22.f : cw - 32, 0}); rm->addChild(sw);
                if (!rw.gear.empty()) {
                    auto key = rw.gear;
                    auto g = makeGear([this, key] { m_sub = key; this->later(true); });
                    g->setPosition({m_paper ? cw - 20.f : cw - 76, 0}); rm->addChild(g);
                }
                break;
            }
            case Row::Num: {
                auto l = mkLabel(rw.label.c_str());
                fit(l, cw - 215.f, .4f, .1f); l->setAnchorPoint({0, .5f}); l->setPosition({12, 0});
                rm->addChild(l);
                if (!rw.help.empty()) {
                    auto hb = makeHelp(rw.label, rw.help);
                    hb->setPosition({12.f + l->getScaledContentSize().width + 13.f, 0}); rm->addChild(hb);
                }
                auto v = mkLabel(rw.get().c_str());
                v->setColor(to3(accL()));
                sc(v, .42f); v->setPosition({cw - 74, 0});
                fit(v, 70.f, .42f, .1f);
                rm->addChild(v);
                auto get = rw.get; auto step = rw.step;
                Ref<CCLabelBMFont> vr = v;
                auto fix = [vr, get] { vr->setString(get().c_str()); fit(vr, 70.f, .42f, .1f); };
                if (rw.setv) {
                    auto box = CCNode::create();
                    box->setContentSize({74, 20});
                    auto bd = CCDrawNode::create();
                    if (m_paper) rrect(bd, {0, 0}, {74, 20}, 3, pGray(0.15f), 1.f, pGray(0.38f));
                    else rrect(bd, {0, 0}, {74, 20}, 7, ccc4f(1, 1, 1, 0.07f), 1.f, ccc4f(1, 1, 1, 0.28f));
                    box->addChild(bd);
                    auto title = rw.label; auto setv = rw.setv;
                    auto bi = CCMenuItemExt::createSpriteExtra(box, [title, get, setv, fix](auto) {
                        XyzValuePopup::open(title, get(), [setv, fix](double val) { setv(val); fix(); });
                    });
                    bi->setPosition({cw - 74, 0});
                    rm->addChild(bi, 0);
                    v->setZOrder(1);
                }
                auto mi = makeRound("-", [step, fix] { step(-1); fix(); });
                mi->setPosition({cw - 122, 0}); rm->addChild(mi);
                auto pl = makeRound("+", [step, fix] { step(1); fix(); });
                pl->setPosition({cw - 26, 0}); rm->addChild(pl);
                break;
            }
            case Row::Text: {
                auto l = mkLabel(rw.label.c_str());
                sc(l, .6f); l->setOpacity(190); l->setAnchorPoint({0, .5f}); l->setPosition({12, 0});
                l->setWidth((cw - 24.f) / (.6f * kOf(l)));
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
            rm->setPositionX(28.f);
            rm->setCascadeOpacityEnabled(true);
            rm->setOpacity(0);
            rm->runAction(CCSequence::create(
                CCDelayTime::create(std::min(idx, 12) * 0.03f),
                CCSpawn::create(CCEaseExponentialOut::create(CCMoveTo::create(.3f, {0, cy})), CCFadeIn::create(.25f), nullptr),
                nullptr));
        }
        y -= h; ++idx;
    }
    m_scroll->moveToTop();
}

#if !defined(GEODE_IS_IOS)
class $modify(XyzKeys, CCKeyboardDispatcher) {
    bool dispatchKeyboardMSG(enumKeyCodes key, bool down, bool repeat, double ts) {
        if (down && !repeat) {
            if (key == KEY_F6) { XyzMenu::toggle(); return true; }
            if (key == KEY_F7 && PlayLayer::get()) { stepOnce(); return true; }
            if (key == KEY_F8) { takeOverNow(); return true; }
        }
        return CCKeyboardDispatcher::dispatchKeyboardMSG(key, down, repeat, ts);
    }
};
#endif

$on_mod(Loaded) { loadCfg(); }
