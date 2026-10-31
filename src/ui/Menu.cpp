#include "Menu.hpp"
#include "Glass.hpp"
#include "Draw.hpp"
#include "../core/State.hpp"
#include "../vis/Vis.hpp"
#include <Geode/modify/PauseLayer.hpp>
#if !defined(GEODE_IS_IOS)
#include <Geode/modify/CCKeyboardDispatcher.hpp>
#endif
#include <memory>

using namespace geode::prelude;
using namespace xyz;
using namespace xyz::draw;

static XyzMenu* s_open = nullptr;
static constexpr float SW = 98.f;
static constexpr float ROW = 30.f;

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
    auto lbl = lbl(text.c_str());
    lbl->setScale(.34f);
    float w = std::max(minW, lbl->getScaledContentSize().width + 18.f);
    auto node = CCNode::create();
    node->setContentSize({w, 20});
    auto d = CCDrawNode::create();
    rrect(d, {0, 0}, {w, 20}, 10, accent ? acc(0.9f, 0.75f) : ccc4f(1, 1, 1, 0.12f), 1.f, accent ? accL(0.8f) : ccc4f(1, 1, 1, 0.25f));
    node->addChild(d);
    lbl->setPosition({w / 2, 10});
    node->addChild(lbl);
    return CCMenuItemExt::createSpriteExtra(node, [cb](auto) { cb(); });
}

static CCMenuItemSpriteExtra* makeRound(char const* glyph, std::function<void()> cb) {
    auto node = CCNode::create();
    node->setContentSize({20, 20});
    auto d = CCDrawNode::create();
    disc(d, {10, 10}, 10.f, ccc4f(1, 1, 1, 0.13f), 1.f, ccc4f(1, 1, 1, 0.3f), 24);
    node->addChild(d);
    auto l = lbl(glyph);
    l->setScale(.42f); l->setPosition({10, 10});
    node->addChild(l);
    return CCMenuItemExt::createSpriteExtra(node, [cb](auto) { cb(); });
}

static CCMenuItemSpriteExtra* makeGear(std::function<void()> cb) {
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

XyzMenu* XyzMenu::create() {
    auto r = new XyzMenu();
    if (r->init()) { r->autorelease(); return r; }
    delete r;
    return nullptr;
}

void XyzMenu::toggle() {
    if (s_open) { s_open->onClose(nullptr); return; }
    s_open = create();
    if (s_open) s_open->show();
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
    m_w = std::min(ws.width - 30.f, 470.f);
    m_h = std::min(ws.height - 24.f, 280.f);
    m_glass = glass::create({m_w, m_h}, 16.f);   // snapshot before this popup joins the scene
    m_noElasticity = true;
    if (!Popup::init(m_w, m_h)) return false;
    if (m_bgSprite) m_bgSprite->setVisible(false);
    if (m_closeBtn) m_closeBtn->setVisible(false);

    if (m_glass) {
        this->addChild(m_glass, -5);
        m_glass->setPosition(ws / 2);
        glass::setAlpha(m_glass, 0.f);
        glass::setTheme(m_glass, to3(acc()), tint3());
    } else {
        auto d = CCDrawNode::create();
        auto t = tint3();
        rrect(d, {0, 0}, {m_w, m_h}, 16, {t.r / 255.f, t.g / 255.f, t.b / 255.f, 0.9f}, 1.2f, accL(0.5f));
        m_mainLayer->addChild(d, -1);
    }

    auto title = lbl("xyzBot");
    title->setScale(.5f); title->setAnchorPoint({0, .5f}); title->setPosition({18, m_h - 20});
    m_mainLayer->addChild(title);
    auto sub = lbl(fmt::format("v{}", Mod::get()->getVersion().toVString().substr(1)).c_str());
    sub->setScale(.5f); sub->setOpacity(150); sub->setAnchorPoint({0, .5f});
    sub->setPosition({title->getPositionX() + title->getScaledContentSize().width + 6, m_h - 22});
    m_mainLayer->addChild(sub);

    auto top = CCMenu::create();
    top->setPosition({0, 0});
    m_mainLayer->addChild(top, 5);
    auto x = makeRound("x", [this] { this->onClose(nullptr); });
    x->setPosition({m_w - 20, m_h - 20});
    top->addChild(x);

    float cw = m_w - SW - 30.f, ch = m_h - 52.f;
    m_scroll = ScrollLayer::create({cw, ch});
    m_scroll->setPosition({SW + 20.f, 12.f});
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
    return true;
}

void XyzMenu::fadeStep(float dt) {
    m_fade = std::clamp(m_fade + dt * (m_closing ? -8.f : 6.f), 0.f, 1.f);
    if (m_glass) glass::setAlpha(m_glass, m_fade);
    if (!m_closing && m_fade >= 1.f) this->unschedule(schedule_selector(XyzMenu::fadeStep));
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

static char const* TABS[] = {"Bot", "Physics", "Visuals", "Macros", "Labels", "Assist", "Theme"};
static constexpr int NTABS = 7;

void XyzMenu::moveHl(bool animate) {
    if (!m_hl) return;
    float h2 = m_h - 52.f;
    CCPoint p = {16.f, 12.f + h2 - 6.f - 26.f - m_tab * 29.f};
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
        auto l = lbl(TABS[i]);
        l->setScale(.4f); l->setPosition({(SW - 12) / 2, 13});
        node->addChild(l);
        m_tabLabels.push_back(l);
        auto b = CCMenuItemExt::createSpriteExtra(node, [this, i](auto) {
            if (m_tab == i && m_sub.empty()) return;
            m_tab = i; m_sel = -1; m_sub.clear();
            this->moveHl(true);
            this->later(true);
        });
        b->setPosition({10 + SW / 2, 12.f + h2 - 6.f - 13.f - i * 29.f});
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
    std::function<void(bool)> setb;
    std::function<std::string()> get;
    std::function<void(int)> step;
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
    auto tog = [&](std::string t, bool v, std::function<void(bool)> f, std::string gear = "") { Row x; x.k = Row::Toggle; x.label = std::move(t); x.on = v; x.setb = std::move(f); x.gear = std::move(gear); rows.push_back(std::move(x)); };
    auto num = [&](std::string t, std::function<std::string()> g, std::function<void(int)> s) { Row x; x.k = Row::Num; x.label = std::move(t); x.get = std::move(g); x.step = std::move(s); rows.push_back(std::move(x)); };
    auto txt = [&](std::string t) { Row x; x.k = Row::Text; x.label = std::move(t); rows.push_back(std::move(x)); };
    auto pills = [&](std::vector<std::pair<std::string, std::function<void()>>> p) { Row x; x.k = Row::Pills; x.pills = std::move(p); rows.push_back(std::move(x)); };
    auto custom = [&](std::function<void(CCMenu*, float, float)> f) { Row x; x.k = Row::Custom; x.custom = std::move(f); rows.push_back(std::move(x)); };

    auto colorRows = [&](int* cr, int* cg, int* cb, std::function<void()> changed) {
        num("Red", [cr] { return fmt::format("{}", *cr); }, [cr, changed](int d) { *cr = std::clamp(*cr + d * 15, 0, 255); changed(); });
        num("Green", [cg] { return fmt::format("{}", *cg); }, [cg, changed](int d) { *cg = std::clamp(*cg + d * 15, 0, 255); changed(); });
        num("Blue", [cb] { return fmt::format("{}", *cb); }, [cb, changed](int d) { *cb = std::clamp(*cb + d * 15, 0, 255); changed(); });
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
        } else if (sub == "speed") {
            sec("SPEEDHACK SETTINGS");
            num("Speed", [] { return fmt::format("{:.2f}", cfg().speedVal); }, [](int d) { cfg().speedVal = std::clamp(cfg().speedVal + d * 0.1f, 0.05f, 100.f); saveCfg(); });
            tog("Speedhack Audio", c.speedAudio, [](bool v) { cfg().speedAudio = v; saveCfg(); });
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
            num("Hold Ticks", [p] { return fmt::format("{}", cfg().acHold[p]); }, [p](int d) { cfg().acHold[p] = std::clamp(cfg().acHold[p] + d, 1, 240); saveCfg(); });
            num("Release Ticks", [p] { return fmt::format("{}", cfg().acRel[p]); }, [p](int d) { cfg().acRel[p] = std::clamp(cfg().acRel[p] + d, 1, 240); saveCfg(); });
            num("Clicks Per Hold", [p] { return fmt::format("{}", cfg().acClicks[p]); }, [p](int d) { cfg().acClicks[p] = std::clamp(cfg().acClicks[p] + d, 1, 10); saveCfg(); });
        } else if (sub == "label" && m_sel >= 0) {
            size_t i = (size_t)m_sel;
            sec(labels::name(i));
            tog("Enabled", labels::cfg(i).on, [i](bool v) { labels::cfg(i).on = v; labels::save(i); });
            num("Anchor", [i] { static char const* a[] = {"Top Left", "Top Right", "Bottom Left", "Bottom Right"}; return std::string(a[std::clamp(labels::cfg(i).anchor, 0, 3)]); },
                [i](int d) { auto& a = labels::cfg(i).anchor; a = (a + d + 4) % 4; labels::save(i); });
            num("Font", [i] { return std::string(labels::cfg(i).font == 0 ? "GD Big" : "Arial"); }, [i](int) { auto& f = labels::cfg(i).font; f = 1 - f; labels::save(i); });
            num("Size", [i] { return fmt::format("{:.2f}", labels::cfg(i).scale); }, [i](int d) { auto& s = labels::cfg(i).scale; s = std::clamp(s + d * 0.05f, 0.1f, 2.f); labels::save(i); });
            num("Opacity", [i] { return fmt::format("{:.2f}", labels::cfg(i).opacity); }, [i](int d) { auto& o = labels::cfg(i).opacity; o = std::clamp(o + d * 0.1f, 0.1f, 1.f); labels::save(i); });
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
        pills({{"Record", [this] { startRecord(); this->later(false); }}, {"Stop", [this] { stopAll(true); this->later(false); }}, {"Step", [] { stepOnce(); }}});
        tog("Frame Stepper", c.stepper, [](bool v) { cfg().stepper = v; });
        tog("Noclip", c.noclip, [](bool v) { cfg().noclip = v; saveCfg(); });
        sec("ACCURACY");
        tog("Frame Fixes", c.fixes, [](bool v) { cfg().fixes = v; saveCfg(); });
        tog("Ignore Level Check", c.ignoreLevel, [](bool v) { cfg().ignoreLevel = v; saveCfg(); });
        tog("Block Inputs In Playback", c.blockInputs, [](bool v) { cfg().blockInputs = v; saveCfg(); });
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
        break;
    case 2:
        sec("HITBOXES");
        tog("Show Hitboxes", c.hitboxes, [](bool v) { cfg().hitboxes = v; saveCfg(); }, "hitbox");
        tog("Hitbox Trail", c.trail, [](bool v) { cfg().trail = v; saveCfg(); }, "trail");
        sec("TRAJECTORY");
        tog("Show Trajectory", c.traj, [](bool v) { cfg().traj = v; saveCfg(); }, "traj");
        sec("LEVEL");
        tog("Layout Mode (reload level)", c.layout, [](bool v) { cfg().layout = v; saveCfg(); });
        break;
    case 3: {
        sec("MACROS");
        pills({{"Record", [this] { startRecord(); this->onClose(nullptr); }}, {"Stop", [this] { stopAll(true); this->later(false); }},
               {"Import", [this] {
                    int n = 0; std::error_code ec;
                    for (auto& e : std::filesystem::directory_iterator(macroDir(), ec)) {
                        auto ext = e.path().extension().string();
                        if (ext != ".gdr" && ext != ".txt" && ext != ".csv") continue;
                        Macro m;
                        if (importMacro(m, e.path()) && saveMacro(m, e.path().stem().string())) n++;
                    }
                    Notification::create(fmt::format("Imported {}", n), NotificationIcon::Info)->show();
                    this->later(false); }}});
        auto names = listMacros();
        if (names.empty()) txt("No macros yet. Drop .gdr/.txt files into the macros folder and press Import.");
        for (auto& n : names) {
            custom([this, n](CCMenu* menu, float y, float w) {
                std::error_code ec;
                auto sz = std::filesystem::file_size(macroDir() / (n + ".xyz"), ec);
                auto l = lbl(n.c_str());
                l->limitLabelWidth(w - 170.f, .4f, .1f); l->setAnchorPoint({0, .5f}); l->setPosition({12, y + 4});
                menu->addChild(l);
                auto s = lbl(ec ? "" : (sz >= 1024 ? fmt::format("{:.1f} KB", sz / 1024.0) : fmt::format("{} B", sz)).c_str());
                s->setScale(.5f); s->setOpacity(140); s->setAnchorPoint({0, .5f}); s->setPosition({12, y - 8});
                menu->addChild(s);
                auto p = makePill("Play", true, [this, n] {
                    if (!startPlay(n)) Notification::create("Cannot play macro", NotificationIcon::Error)->show();
                    else this->onClose(nullptr); });
                p->setPosition({w - 82, y}); menu->addChild(p);
                auto dl = makePill("Del", false, [this, n] { std::error_code e2; std::filesystem::remove(macroDir() / (n + ".xyz"), e2); this->later(false); });
                dl->setPosition({w - 30, y}); menu->addChild(dl);
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
                auto l = lbl(labels::name(i).c_str());
                l->limitLabelWidth(w - 120.f, .4f, .1f); l->setAnchorPoint({0, .5f}); l->setPosition({12, y});
                menu->addChild(l);
                auto sw = makeSwitch(labels::cfg(i).on, [i](bool v) { labels::cfg(i).on = v; labels::save(i); });
                sw->setPosition({w - 32, y}); menu->addChild(sw);
                auto g = makeGear([this, i] { m_sel = (int)i; m_sub = "label"; this->later(true); });
                g->setPosition({w - 66, y}); menu->addChild(g);
            });
        break;
    case 5:
        sec("SURVIVAL");
        tog("Prevent Death", c.preventDeath, [](bool v) { cfg().preventDeath = v; saveCfg(); }, "pd");
        sec("INPUT");
        tog("Maintain Gravity", c.maintainGravity, [](bool v) { cfg().maintainGravity = v; saveCfg(); });
        tog("Mirror Inputs", c.mirror, [](bool v) { cfg().mirror = v; saveCfg(); });
        tog("Mirror Inverted", c.mirrorInv, [](bool v) { cfg().mirrorInv = v; saveCfg(); });
        tog("No Mirror Portals", c.noMirror, [](bool v) { cfg().noMirror = v; saveCfg(); });
        sec("AUTOCLICKER");
        tog("Autoclicker P1", c.acOn[0], [](bool v) { cfg().acOn[0] = v; saveCfg(); }, "ac0");
        tog("Autoclicker P2", c.acOn[1], [](bool v) { cfg().acOn[1] = v; saveCfg(); }, "ac1");
        break;
    default: {
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

    float cw = m_w - SW - 30.f, ch = m_h - 52.f;
    float total = 8.f;
    for (auto& rw : rows) total += (rw.k == Row::Section ? 22.f : ROW + 2.f);
    total = std::max(total + 6.f, ch);
    content->setContentSize({cw, total});

    float y = total - 4.f;
    int idx = 0;
    for (auto& rw : rows) {
        float h = rw.k == Row::Section ? 22.f : ROW + 2.f;
        float cy = y - h / 2.f;
        auto rm = CCMenu::create();
        rm->setPosition({0, cy});
        rm->setContentSize({cw, h});
        content->addChild(rm);

        if (rw.k == Row::Section) {
            auto l = lbl(rw.label.c_str());
            l->setScale(.33f); l->setColor(to3(accL())); l->setAnchorPoint({0, .5f}); l->setPosition({8, -2});
            rm->addChild(l);
        } else {
            auto bg = CCDrawNode::create();
            rrect(bg, {0, -ROW / 2.f}, {cw, ROW}, 9, ccc4f(1, 1, 1, 0.055f), 1.f, ccc4f(1, 1, 1, 0.10f));
            rm->addChild(bg, -1);
            switch (rw.k) {
            case Row::Toggle: {
                auto l = lbl(rw.label.c_str());
                l->limitLabelWidth(cw - (rw.gear.empty() ? 90.f : 120.f), .4f, .1f); l->setAnchorPoint({0, .5f}); l->setPosition({12, 0});
                rm->addChild(l);
                auto sw = makeSwitch(rw.on, rw.setb);
                sw->setPosition({cw - 32, 0}); rm->addChild(sw);
                if (!rw.gear.empty()) {
                    auto key = rw.gear;
                    auto g = makeGear([this, key] { m_sub = key; this->later(true); });
                    g->setPosition({cw - 66, 0}); rm->addChild(g);
                }
                break;
            }
            case Row::Num: {
                auto l = lbl(rw.label.c_str());
                l->limitLabelWidth(cw - 190.f, .4f, .1f); l->setAnchorPoint({0, .5f}); l->setPosition({12, 0});
                rm->addChild(l);
                auto v = lbl(rw.get().c_str());
                v->setColor(to3(accL()));
                v->setScale(.42f); v->setPosition({cw - 74, 0});
                v->limitLabelWidth(70.f, .42f, .1f);
                rm->addChild(v);
                auto get = rw.get; auto step = rw.step;
                auto fix = [v, get] { v->setString(get().c_str()); v->limitLabelWidth(70.f, .42f, .1f); };
                auto mi = makeRound("-", [step, fix] { step(-1); fix(); });
                mi->setPosition({cw - 122, 0}); rm->addChild(mi);
                auto pl = makeRound("+", [step, fix] { step(1); fix(); });
                pl->setPosition({cw - 26, 0}); rm->addChild(pl);
                break;
            }
            case Row::Text: {
                auto l = lbl(rw.label.c_str());
                l->setScale(.6f); l->setOpacity(190); l->setAnchorPoint({0, .5f}); l->setPosition({12, 0});
                l->setWidth((cw - 24.f) / .6f);
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

class $modify(XyzPause, PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();
        auto menu = this->getChildByID("left-button-menu");
        if (!menu) return;
        auto spr = ButtonSprite::create("XYZ", fontFile().c_str(), "GJ_button_04.png", .6f);
        auto b = CCMenuItemExt::createSpriteExtra(spr, [](auto) { XyzMenu::toggle(); });
        b->setID("xyz-button"_spr);
        menu->addChild(b);
        menu->updateLayout();
    }
};

#if !defined(GEODE_IS_IOS)
class $modify(XyzKeys, CCKeyboardDispatcher) {
    bool dispatchKeyboardMSG(enumKeyCodes key, bool down, bool repeat, double ts) {
        if (down && !repeat) {
            if (key == KEY_F6) { XyzMenu::toggle(); return true; }
            if (key == KEY_F7 && PlayLayer::get()) { stepOnce(); return true; }
        }
        return CCKeyboardDispatcher::dispatchKeyboardMSG(key, down, repeat, ts);
    }
};
#endif

$on_mod(Loaded) { loadCfg(); }
