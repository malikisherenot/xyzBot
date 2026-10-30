#include "Menu.hpp"
#include "Glass.hpp"
#include "../core/State.hpp"
#include "../vis/Vis.hpp"
#include <Geode/modify/PauseLayer.hpp>
#if !defined(GEODE_IS_IOS)
#include <Geode/modify/CCKeyboardDispatcher.hpp>
#endif
#include <memory>

using namespace geode::prelude;
using namespace xyz;

static XyzMenu* s_open = nullptr;
static constexpr float SW = 98.f;
static constexpr float ROW = 30.f;
static ccColor4F const ACC = {0.f, 0.85f, 0.6f, 1.f};

static void rrect(CCDrawNode* d, CCPoint o, CCSize s, float r, ccColor4F fill, float bw = 0.f, ccColor4F border = {0, 0, 0, 0}) {
    std::vector<CCPoint> pts;
    auto arc = [&](CCPoint c, float a0) {
        for (int i = 0; i <= 6; ++i) {
            float a = (a0 + i * 15.f) * 3.14159265f / 180.f;
            pts.push_back({c.x + std::cos(a) * r, c.y + std::sin(a) * r});
        }
    };
    arc({o.x + s.width - r, o.y + s.height - r}, 0);
    arc({o.x + r, o.y + s.height - r}, 90);
    arc({o.x + r, o.y + r}, 180);
    arc({o.x + s.width - r, o.y + r}, 270);
    d->drawPolygon(pts.data(), (unsigned)pts.size(), fill, bw, border);
}

static CCMenuItemSpriteExtra* makeSwitch(bool on, std::function<void(bool)> cb) {
    auto node = CCNode::create();
    node->setContentSize({42, 22});
    auto d = CCDrawNode::create();
    node->addChild(d);
    auto st = std::make_shared<bool>(on);
    auto draw = [d, st] {
        d->clear();
        bool v = *st;
        rrect(d, {0, 2}, {42, 18}, 9, v ? ccc4f(0.f, 0.7f, 0.5f, 0.95f) : ccc4f(1, 1, 1, 0.14f), 1.f, v ? ccc4f(0.45f, 1.f, 0.8f, 0.9f) : ccc4f(1, 1, 1, 0.3f));
        d->drawDot({v ? 32.f : 10.f, 11.f}, 7.f, v ? ccc4f(1, 1, 1, 1) : ccc4f(1, 1, 1, 0.75f));
    };
    draw();
    return CCMenuItemExt::createSpriteExtra(node, [st, draw, cb](auto) { *st = !*st; draw(); cb(*st); });
}

static CCMenuItemSpriteExtra* makePill(std::string const& text, bool accent, std::function<void()> cb, float minW = 0.f) {
    auto lbl = CCLabelBMFont::create(text.c_str(), "bigFont.fnt");
    lbl->setScale(.34f);
    float w = std::max(minW, lbl->getScaledContentSize().width + 18.f);
    auto node = CCNode::create();
    node->setContentSize({w, 20});
    auto d = CCDrawNode::create();
    rrect(d, {0, 0}, {w, 20}, 10, accent ? ccc4f(0.f, 0.65f, 0.45f, 0.9f) : ccc4f(1, 1, 1, 0.12f), 1.f, accent ? ccc4f(0.5f, 1.f, 0.85f, 0.8f) : ccc4f(1, 1, 1, 0.25f));
    node->addChild(d);
    lbl->setPosition({w / 2, 10});
    node->addChild(lbl);
    return CCMenuItemExt::createSpriteExtra(node, [cb](auto) { cb(); });
}

static CCMenuItemSpriteExtra* makeRound(char const* glyph, std::function<void()> cb) {
    auto node = CCNode::create();
    node->setContentSize({20, 20});
    auto d = CCDrawNode::create();
    d->drawDot({10, 10}, 10, ccc4f(1, 1, 1, 0.13f));
    d->drawCircle({10, 10}, 9.5f, {0, 0, 0, 0}, 1.f, ccc4f(1, 1, 1, 0.3f), 24);
    node->addChild(d);
    auto l = CCLabelBMFont::create(glyph, "bigFont.fnt");
    l->setScale(.42f); l->setPosition({10, 10});
    node->addChild(l);
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
    } else {
        auto d = CCDrawNode::create();
        rrect(d, {0, 0}, {m_w, m_h}, 16, ccc4f(0.05f, 0.08f, 0.11f, 0.88f), 1.2f, ccc4f(0.5f, 1.f, 0.85f, 0.5f));
        m_mainLayer->addChild(d, -1);
    }

    auto title = CCLabelBMFont::create("xyzBot", "bigFont.fnt");
    title->setScale(.5f); title->setAnchorPoint({0, .5f}); title->setPosition({18, m_h - 20});
    m_mainLayer->addChild(title);
    auto sub = CCLabelBMFont::create(fmt::format("v{}", Mod::get()->getVersion().toVString().substr(1)).c_str(), "chatFont.fnt");
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
    this->rebuild();

    m_mainLayer->setScale(.94f);
    m_mainLayer->runAction(CCEaseExponentialOut::create(CCScaleTo::create(.22f, 1.f)));
    this->schedule(schedule_selector(XyzMenu::fadeIn));
    return true;
}

void XyzMenu::fadeIn(float dt) {
    m_fade = std::min(1.f, m_fade + dt * 6.f);
    if (m_glass) glass::setAlpha(m_glass, m_fade);
    if (m_fade >= 1.f) this->unschedule(schedule_selector(XyzMenu::fadeIn));
}

void XyzMenu::buildSide() {
    m_side->removeAllChildren();
    auto d = CCDrawNode::create();
    rrect(d, {10, 12}, {SW, m_h - 52}, 12, ccc4f(1, 1, 1, 0.05f), 1.f, ccc4f(1, 1, 1, 0.12f));
    m_side->addChild(d);
    auto menu = CCMenu::create();
    menu->setPosition({0, 0});
    m_side->addChild(menu);
    char const* names[] = {"Bot", "Physics", "Visuals", "Macros", "Labels", "Assist"};
    float y = m_h - 52.f - 6.f;
    for (int i = 0; i < 6; ++i) {
        bool sel = i == m_tab;
        auto node = CCNode::create();
        node->setContentSize({SW - 12, 26});
        auto bg = CCDrawNode::create();
        if (sel) rrect(bg, {0, 0}, {SW - 12, 26}, 10, ccc4f(0.f, 0.65f, 0.45f, 0.75f), 1.f, ccc4f(0.5f, 1.f, 0.85f, 0.7f));
        else rrect(bg, {0, 0}, {SW - 12, 26}, 10, ccc4f(1, 1, 1, 0.04f));
        node->addChild(bg);
        auto l = CCLabelBMFont::create(names[i], "bigFont.fnt");
        l->setScale(.4f); l->setPosition({(SW - 12) / 2, 13});
        if (!sel) l->setOpacity(200);
        node->addChild(l);
        auto b = CCMenuItemExt::createSpriteExtra(node, [this, i](auto) { m_tab = i; m_sel = -1; this->buildSide(); this->rebuild(); });
        b->setPosition({10 + SW / 2, y - 13 - i * 31.f + 0.f});
        menu->addChild(b);
    }
}

namespace {
struct Row {
    enum K { Section, Toggle, Num, Pills, Text, Custom } k = Text;
    std::string label;
    bool on = false;
    std::function<void(bool)> setb;
    std::function<std::string()> get;
    std::function<void(int)> step;
    std::vector<std::pair<std::string, std::function<void()>>> pills;
    std::function<void(CCMenu*, float, float)> custom;  // menu, y (row center), width
};
}

void XyzMenu::rebuild() {
    auto content = m_scroll->m_contentLayer;
    content->removeAllChildren();
    auto& c = cfg(); auto& r = rt();
    std::vector<Row> rows;

    auto sec = [&](std::string t) { Row x; x.k = Row::Section; x.label = std::move(t); rows.push_back(std::move(x)); };
    auto tog = [&](std::string t, bool v, std::function<void(bool)> f) { Row x; x.k = Row::Toggle; x.label = std::move(t); x.on = v; x.setb = std::move(f); rows.push_back(std::move(x)); };
    auto num = [&](std::string t, std::function<std::string()> g, std::function<void(int)> s) { Row x; x.k = Row::Num; x.label = std::move(t); x.get = std::move(g); x.step = std::move(s); rows.push_back(std::move(x)); };
    auto txt = [&](std::string t) { Row x; x.k = Row::Text; x.label = std::move(t); rows.push_back(std::move(x)); };
    auto pills = [&](std::vector<std::pair<std::string, std::function<void()>>> p) { Row x; x.k = Row::Pills; x.pills = std::move(p); rows.push_back(std::move(x)); };
    auto custom = [&](std::function<void(CCMenu*, float, float)> f) { Row x; x.k = Row::Custom; x.custom = std::move(f); rows.push_back(std::move(x)); };

    switch (m_tab) {
    case 0: {
        std::string st = r.mode == Mode::Record ? "Recording" : r.mode == Mode::Play ? "Playing" : "Idle";
        sec("BOT");
        txt("State: " + st);
        pills({{"Record", [this] { startRecord(); this->rebuild(); }}, {"Stop", [this] { stopAll(true); this->rebuild(); }}, {"Step", [] { stepOnce(); }}});
        tog("Frame Stepper", c.stepper, [](bool v) { cfg().stepper = v; });
        tog("Noclip", c.noclip, [](bool v) { cfg().noclip = v; saveCfg(); });
        sec("ACCURACY");
        tog("Frame Fixes", c.fixes, [](bool v) { cfg().fixes = v; saveCfg(); });
        tog("Ignore Level Check", c.ignoreLevel, [](bool v) { cfg().ignoreLevel = v; saveCfg(); });
        tog("Block Inputs In Playback", c.blockInputs, [](bool v) { cfg().blockInputs = v; saveCfg(); });
        sec("AUTOMATION");
        tog("Autosave While Recording", c.autosave, [](bool v) { cfg().autosave = v; saveCfg(); });
        num("Autosave Interval (s)", [] { return fmt::format("{}", cfg().autosaveSec); }, [](int d) { cfg().autosaveSec = std::clamp(cfg().autosaveSec + d * 30, 10, 3600); saveCfg(); });
        tog("Auto Exit On Finish", c.autoExit, [](bool v) { cfg().autoExit = v; saveCfg(); });
        break;
    }
    case 1:
        sec("TPS");
        tog("TPS Bypass", c.tps, [](bool v) { cfg().tps = v; saveCfg(); });
        num("TPS", [] { return fmt::format("{}", (int)cfg().tpsVal); }, [](int d) {
            auto& v = cfg().tpsVal; float s = v >= 10000 ? 5000 : v >= 1000 ? 500 : v >= 240 ? 60 : 10;
            v = std::clamp(v + d * s, 1.f, 1000000.f); saveCfg(); });
        num("Frame Budget (ms)", [] { return fmt::format("{}", (int)cfg().budgetMs); }, [](int d) { cfg().budgetMs = std::clamp(cfg().budgetMs + d, 1.f, 100.f); saveCfg(); });
        sec("SPEEDHACK");
        tog("Speedhack", c.speed, [](bool v) { cfg().speed = v; saveCfg(); });
        num("Speed", [] { return fmt::format("{:.2f}", cfg().speedVal); }, [](int d) { cfg().speedVal = std::clamp(cfg().speedVal + d * 0.1f, 0.05f, 100.f); saveCfg(); });
        tog("Speedhack Audio", c.speedAudio, [](bool v) { cfg().speedAudio = v; saveCfg(); });
        break;
    case 2:
        sec("HITBOXES");
        tog("Show Hitboxes", c.hitboxes, [](bool v) { cfg().hitboxes = v; saveCfg(); });
        tog("Player", c.hbPlayer, [](bool v) { cfg().hbPlayer = v; saveCfg(); });
        tog("Solid / Hazard", c.hbSolid && c.hbHazard, [](bool v) { cfg().hbSolid = cfg().hbHazard = v; saveCfg(); });
        tog("Special / Rotated", c.hbSpecial && c.hbRotated, [](bool v) { cfg().hbSpecial = cfg().hbRotated = v; saveCfg(); });
        tog("Hitbox Trail", c.trail, [](bool v) { cfg().trail = v; saveCfg(); });
        num("Trail Length", [] { return fmt::format("{}", cfg().trailLen); }, [](int d) { cfg().trailLen = std::clamp(cfg().trailLen + d * 50, 10, 2000); saveCfg(); });
        sec("TRAJECTORY");
        tog("Show Trajectory", c.traj, [](bool v) { cfg().traj = v; saveCfg(); });
        num("Length", [] { return fmt::format("{}", cfg().trajLen); }, [](int d) { cfg().trajLen = std::clamp(cfg().trajLen + d * 30, 10, 2000); saveCfg(); });
        sec("LEVEL");
        tog("Layout Mode (reload level)", c.layout, [](bool v) { cfg().layout = v; saveCfg(); });
        break;
    case 3: {
        sec("MACROS");
        pills({{"Record", [this] { startRecord(); this->onClose(nullptr); }}, {"Stop", [this] { stopAll(true); this->rebuild(); }},
               {"Import", [this] {
                    int n = 0; std::error_code ec;
                    for (auto& e : std::filesystem::directory_iterator(macroDir(), ec)) {
                        auto ext = e.path().extension().string();
                        if (ext != ".gdr" && ext != ".txt" && ext != ".csv") continue;
                        Macro m;
                        if (importMacro(m, e.path()) && saveMacro(m, e.path().stem().string())) n++;
                    }
                    Notification::create(fmt::format("Imported {}", n), NotificationIcon::Info)->show();
                    this->rebuild(); }}});
        auto names = listMacros();
        if (names.empty()) txt("No macros yet. Drop .gdr/.txt files into the macros folder and press Import.");
        for (auto& n : names) {
            custom([this, n](CCMenu* menu, float y, float w) {
                std::error_code ec;
                auto sz = std::filesystem::file_size(macroDir() / (n + ".xyz"), ec);
                auto l = CCLabelBMFont::create(n.c_str(), "bigFont.fnt");
                l->limitLabelWidth(w - 170.f, .4f, .1f); l->setAnchorPoint({0, .5f}); l->setPosition({12, y + 4});
                menu->addChild(l);
                auto s = CCLabelBMFont::create(ec ? "" : (sz >= 1024 ? fmt::format("{:.1f} KB", sz / 1024.0) : fmt::format("{} B", sz)).c_str(), "chatFont.fnt");
                s->setScale(.5f); s->setOpacity(140); s->setAnchorPoint({0, .5f}); s->setPosition({12, y - 8});
                menu->addChild(s);
                auto p = makePill("Play", true, [this, n] {
                    if (!startPlay(n)) Notification::create("Cannot play macro", NotificationIcon::Error)->show();
                    else this->onClose(nullptr); });
                p->setPosition({w - 82, y}); menu->addChild(p);
                auto dl = makePill("Del", false, [this, n] { std::error_code e2; std::filesystem::remove(macroDir() / (n + ".xyz"), e2); this->rebuild(); });
                dl->setPosition({w - 30, y}); menu->addChild(dl);
            });
        }
        break;
    }
    case 4:
        if (m_sel < 0) {
            sec("LABELS");
            tog("Labels Enabled", c.labels, [](bool v) { cfg().labels = v; saveCfg(); labels::invalidate(); });
            for (size_t i = 0; i < labels::count(); ++i)
                custom([this, i](CCMenu* menu, float y, float w) {
                    auto l = CCLabelBMFont::create(labels::name(i).c_str(), "bigFont.fnt");
                    l->limitLabelWidth(w - 150.f, .4f, .1f); l->setAnchorPoint({0, .5f}); l->setPosition({12, y});
                    menu->addChild(l);
                    auto sw = makeSwitch(labels::cfg(i).on, [i](bool v) { labels::cfg(i).on = v; labels::save(i); });
                    sw->setPosition({w - 84, y}); menu->addChild(sw);
                    auto ed = makePill("Edit", false, [this, i] { m_sel = (int)i; this->rebuild(); });
                    ed->setPosition({w - 30, y}); menu->addChild(ed);
                });
        } else {
            size_t i = (size_t)m_sel;
            pills({{"< Back", [this] { m_sel = -1; this->rebuild(); }}});
            sec(labels::name(i));
            tog("Enabled", labels::cfg(i).on, [i](bool v) { labels::cfg(i).on = v; labels::save(i); });
            num("Anchor", [i] { static char const* a[] = {"Top Left", "Top Right", "Bottom Left", "Bottom Right"}; return std::string(a[std::clamp(labels::cfg(i).anchor, 0, 3)]); },
                [i](int d) { auto& a = labels::cfg(i).anchor; a = (a + d + 4) % 4; labels::save(i); });
            num("Font", [i] { return std::string(labels::cfg(i).font == 0 ? "Big" : "Chat"); }, [i](int) { auto& f = labels::cfg(i).font; f = 1 - f; labels::save(i); });
            num("Size", [i] { return fmt::format("{:.2f}", labels::cfg(i).scale); }, [i](int d) { auto& s = labels::cfg(i).scale; s = std::clamp(s + d * 0.05f, 0.1f, 2.f); labels::save(i); });
            num("Opacity", [i] { return fmt::format("{:.2f}", labels::cfg(i).opacity); }, [i](int d) { auto& o = labels::cfg(i).opacity; o = std::clamp(o + d * 0.1f, 0.1f, 1.f); labels::save(i); });
        }
        break;
    default:
        sec("SURVIVAL");
        tog("Prevent Death", c.preventDeath, [](bool v) { cfg().preventDeath = v; saveCfg(); });
        num("Lookahead Ticks", [] { return fmt::format("{}", cfg().pdLook); }, [](int d) { cfg().pdLook = std::clamp(cfg().pdLook + d, 1, 10); saveCfg(); });
        sec("INPUT");
        tog("Maintain Gravity", c.maintainGravity, [](bool v) { cfg().maintainGravity = v; saveCfg(); });
        tog("Mirror Inputs", c.mirror, [](bool v) { cfg().mirror = v; saveCfg(); });
        tog("Mirror Inverted", c.mirrorInv, [](bool v) { cfg().mirrorInv = v; saveCfg(); });
        tog("No Mirror Portals", c.noMirror, [](bool v) { cfg().noMirror = v; saveCfg(); });
        for (int p = 0; p < 2; ++p) {
            std::string pn = p ? "P2" : "P1";
            sec("AUTOCLICKER " + pn);
            tog("Enabled", c.acOn[p], [p](bool v) { cfg().acOn[p] = v; saveCfg(); });
            num("Hold Ticks", [p] { return fmt::format("{}", cfg().acHold[p]); }, [p](int d) { cfg().acHold[p] = std::clamp(cfg().acHold[p] + d, 1, 240); saveCfg(); });
            num("Release Ticks", [p] { return fmt::format("{}", cfg().acRel[p]); }, [p](int d) { cfg().acRel[p] = std::clamp(cfg().acRel[p] + d, 1, 240); saveCfg(); });
            num("Clicks Per Hold", [p] { return fmt::format("{}", cfg().acClicks[p]); }, [p](int d) { cfg().acClicks[p] = std::clamp(cfg().acClicks[p] + d, 1, 10); saveCfg(); });
        }
        break;
    }

    float cw = m_w - SW - 30.f, ch = m_h - 52.f;
    float total = 8.f;
    for (auto& rw : rows) total += (rw.k == Row::Section ? 22.f : ROW + 2.f);
    total = std::max(total + 6.f, ch);
    content->setContentSize({cw, total});

    auto menu = CCMenu::create();
    menu->setPosition({0, 0});
    menu->setContentSize({cw, total});
    content->addChild(menu, 2);
    auto bg = CCDrawNode::create();
    content->addChild(bg, 1);

    float y = total - 4.f;
    for (auto& rw : rows) {
        float h = rw.k == Row::Section ? 22.f : ROW + 2.f;
        float cy = y - h / 2.f;
        if (rw.k == Row::Section) {
            auto l = CCLabelBMFont::create(rw.label.c_str(), "bigFont.fnt");
            l->setScale(.33f); l->setColor({90, 255, 200}); l->setAnchorPoint({0, .5f}); l->setPosition({8, cy - 2});
            menu->addChild(l);
        } else {
            rrect(bg, {0, y - h + 2}, {cw, ROW}, 9, ccc4f(1, 1, 1, 0.055f), 1.f, ccc4f(1, 1, 1, 0.10f));
            switch (rw.k) {
            case Row::Toggle: {
                auto l = CCLabelBMFont::create(rw.label.c_str(), "bigFont.fnt");
                l->limitLabelWidth(cw - 90.f, .4f, .1f); l->setAnchorPoint({0, .5f}); l->setPosition({12, cy});
                menu->addChild(l);
                auto sw = makeSwitch(rw.on, rw.setb);
                sw->setPosition({cw - 32, cy}); menu->addChild(sw);
                break;
            }
            case Row::Num: {
                auto l = CCLabelBMFont::create(rw.label.c_str(), "bigFont.fnt");
                l->limitLabelWidth(cw - 190.f, .4f, .1f); l->setAnchorPoint({0, .5f}); l->setPosition({12, cy});
                menu->addChild(l);
                auto v = CCLabelBMFont::create(rw.get().c_str(), "goldFont.fnt");
                v->setScale(.42f); v->setPosition({cw - 74, cy});
                menu->addChild(v);
                auto get = rw.get; auto step = rw.step;
                auto fix = [v, get] { v->setString(get().c_str()); v->limitLabelWidth(70.f, .42f, .1f); };
                auto mi = makeRound("-", [step, fix] { step(-1); fix(); });
                mi->setPosition({cw - 122, cy}); menu->addChild(mi);
                auto pl = makeRound("+", [step, fix] { step(1); fix(); });
                pl->setPosition({cw - 26, cy}); menu->addChild(pl);
                break;
            }
            case Row::Text: {
                auto l = CCLabelBMFont::create(rw.label.c_str(), "chatFont.fnt");
                l->setScale(.6f); l->setOpacity(190); l->setAnchorPoint({0, .5f}); l->setPosition({12, cy});
                l->setWidth((cw - 24.f) / .6f);
                menu->addChild(l);
                break;
            }
            case Row::Pills: {
                float x = 10.f;
                for (auto& [t, f] : rw.pills) {
                    auto p = makePill(t, t == "Record" || t == "Step", f, 64.f);
                    p->setAnchorPoint({0, .5f});
                    p->setPosition({x, cy}); menu->addChild(p);
                    x += p->getContentSize().width + 8.f;
                }
                break;
            }
            case Row::Custom: rw.custom(menu, cy, cw); break;
            default: break;
            }
        }
        y -= h;
    }
    m_scroll->moveToTop();
}

void XyzMenu::onClose(CCObject* s) {
    s_open = nullptr;
    Popup::onClose(s);
}

class $modify(XyzPause, PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();
        auto menu = this->getChildByID("left-button-menu");
        if (!menu) return;
        auto spr = ButtonSprite::create("XYZ", "goldFont.fnt", "GJ_button_04.png", .6f);
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
