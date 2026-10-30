#include "Menu.hpp"
#include "../core/State.hpp"
#include "../vis/Vis.hpp"
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/ui/OverlayManager.hpp>
#if !defined(GEODE_IS_IOS)
#include <Geode/modify/CCKeyboardDispatcher.hpp>
#endif

using namespace geode::prelude;
using namespace xyz;

static XyzMenu* s_open = nullptr;
static constexpr float W = 340.f, H = 250.f;

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
    if (!Popup::init(W, H)) return false;
    this->setTitle("xyzBot");
    auto tabs = CCMenu::create();
    tabs->setPosition({0, 0});
    m_mainLayer->addChild(tabs);
    char const* names[] = {"Bot", "TPS", "Vis", "Macro", "Label", "Assist"};
    for (int i = 0; i < 6; ++i) {
        auto b = CCMenuItemExt::createSpriteExtra(ButtonSprite::create(names[i], 44, true, "bigFont.fnt", "GJ_button_04.png", 22.f, .4f), [this, i](auto) {
            m_tab = i; m_page = 0; m_sel = -1; this->build();
        });
        b->setPosition({32.f + i * 55.f, H - 40.f});
        tabs->addChild(b);
    }
    s_open = this;
    this->build();
    return true;
}

void XyzMenu::build() {
    if (m_content) m_content->removeFromParent();
    auto menu = CCMenu::create();
    menu->setPosition({0, 0});
    menu->setContentSize({W, H});
    m_mainLayer->addChild(menu);
    m_content = menu;
    auto& c = cfg(); auto& r = rt();
    using Item = std::function<void(float)>;
    std::vector<Item> items;
    auto tog = [&](char const* n, bool v, std::function<void(bool)> f) { items.push_back([=, this](float y) { this->row(y, n, v, f); }); };
    auto nm = [&](char const* n, std::function<std::string()> g, std::function<void(int)> st) { items.push_back([=, this](float y) { this->num(y, n, g, st); }); };
    int per = 6;

    if (m_tab == 0) {
        std::string st = r.mode == Mode::Record ? "Recording" : r.mode == Mode::Play ? "Playing" : "Idle";
        items.push_back([=, this](float y) {
            auto l = CCLabelBMFont::create(("State: " + st).c_str(), "bigFont.fnt");
            l->setScale(.5f); l->setPosition({W / 2, y}); m_content->addChild(l);
        });
        items.push_back([=, this](float y) {
            this->btn(70, y, "Record", [this] { startRecord(); this->build(); });
            this->btn(170, y, "Stop", [this] { stopAll(true); this->build(); });
            this->btn(270, y, "Step", [] { stepOnce(); });
        });
        tog("Frame Stepper", c.stepper, [](bool v) { cfg().stepper = v; });
        tog("Noclip", c.noclip, [](bool v) { cfg().noclip = v; saveCfg(); });
        tog("Frame Fixes (accuracy)", c.fixes, [](bool v) { cfg().fixes = v; saveCfg(); });
        tog("Ignore Level Check", c.ignoreLevel, [](bool v) { cfg().ignoreLevel = v; saveCfg(); });
        tog("Block Inputs In Playback", c.blockInputs, [](bool v) { cfg().blockInputs = v; saveCfg(); });
        tog("Autosave While Recording", c.autosave, [](bool v) { cfg().autosave = v; saveCfg(); });
        nm("Autosave Interval (s)", [] { return fmt::format("{}", cfg().autosaveSec); }, [](int d) { cfg().autosaveSec = std::clamp(cfg().autosaveSec + d * 30, 10, 3600); saveCfg(); });
        tog("Auto Exit On Finish", c.autoExit, [](bool v) { cfg().autoExit = v; saveCfg(); });
    } else if (m_tab == 1) {
        tog("TPS Bypass", c.tps, [](bool v) { cfg().tps = v; saveCfg(); });
        nm("TPS", [] { return fmt::format("{}", (int)cfg().tpsVal); }, [](int d) {
            auto& v = cfg().tpsVal; float s = v >= 10000 ? 5000 : v >= 1000 ? 500 : v >= 240 ? 60 : 10;
            v = std::clamp(v + d * s, 1.f, 1000000.f); saveCfg(); });
        nm("Frame Budget ms", [] { return fmt::format("{}", (int)cfg().budgetMs); }, [](int d) { cfg().budgetMs = std::clamp(cfg().budgetMs + d, 1.f, 100.f); saveCfg(); });
        tog("Speedhack", c.speed, [](bool v) { cfg().speed = v; saveCfg(); });
        nm("Speed", [] { return fmt::format("{:.2f}", cfg().speedVal); }, [](int d) { cfg().speedVal = std::clamp(cfg().speedVal + d * 0.1f, 0.05f, 100.f); saveCfg(); });
        tog("Speedhack Audio", c.speedAudio, [](bool v) { cfg().speedAudio = v; saveCfg(); });
    } else if (m_tab == 2) {
        tog("Hitboxes", c.hitboxes, [](bool v) { cfg().hitboxes = v; saveCfg(); });
        tog("Player", c.hbPlayer, [](bool v) { cfg().hbPlayer = v; saveCfg(); });
        tog("Solid / Hazard", c.hbSolid && c.hbHazard, [](bool v) { cfg().hbSolid = cfg().hbHazard = v; saveCfg(); });
        tog("Special / Rotated", c.hbSpecial && c.hbRotated, [](bool v) { cfg().hbSpecial = cfg().hbRotated = v; saveCfg(); });
        tog("Hitbox Trail", c.trail, [](bool v) { cfg().trail = v; saveCfg(); });
        nm("Trail Length", [] { return fmt::format("{}", cfg().trailLen); }, [](int d) { cfg().trailLen = std::clamp(cfg().trailLen + d * 50, 10, 2000); saveCfg(); });
        tog("Show Trajectory", c.traj, [](bool v) { cfg().traj = v; saveCfg(); });
        nm("Trajectory Length", [] { return fmt::format("{}", cfg().trajLen); }, [](int d) { cfg().trajLen = std::clamp(cfg().trajLen + d * 30, 10, 2000); saveCfg(); });
        tog("Layout Mode (restart lvl)", c.layout, [](bool v) { cfg().layout = v; saveCfg(); });
    } else if (m_tab == 3) {
        auto names = listMacros();
        per = 5;
        for (auto& n : names) {
            items.push_back([=, this](float y) {
                auto l = CCLabelBMFont::create(n.c_str(), "bigFont.fnt");
                l->limitLabelWidth(170.f, .45f, .1f); l->setPosition({110, y}); m_content->addChild(l);
                this->btn(250, y, "Play", [this, n] {
                    if (!startPlay(n)) Notification::create("Cannot play macro", NotificationIcon::Error)->show();
                    else this->onClose(nullptr);
                });
                this->btn(310, y, "Del", [this, n] { std::error_code ec; std::filesystem::remove(macroDir() / (n + ".xyz"), ec); this->build(); });
            });
        }
    } else if (m_tab == 4) {
        if (m_sel < 0) {
            tog("Labels Enabled", c.labels, [](bool v) { cfg().labels = v; saveCfg(); labels::invalidate(); });
            for (size_t i = 0; i < labels::count(); ++i)
                items.push_back([=, this](float y) {
                    auto l = CCLabelBMFont::create(labels::name(i).c_str(), "bigFont.fnt");
                    l->limitLabelWidth(150.f, .45f, .1f); l->setAnchorPoint({0, .5f}); l->setPosition({30, y}); m_content->addChild(l);
                    auto t = CCMenuItemExt::createTogglerWithStandardSprites(.5f, [i](CCMenuItemToggler* t) { labels::cfg(i).on = !t->isToggled(); labels::save(i); });
                    t->toggle(labels::cfg(i).on); t->setPosition({220, y}); m_content->addChild(t);
                    this->btn(285, y, "Edit", [this, i] { m_sel = (int)i; m_page = 0; this->build(); });
                });
        } else {
            size_t i = (size_t)m_sel;
            items.push_back([=, this](float y) {
                auto l = CCLabelBMFont::create(labels::name(i).c_str(), "goldFont.fnt");
                l->setScale(.55f); l->setPosition({W / 2 - 40, y}); m_content->addChild(l);
                this->btn(W - 50, y, "Back", [this] { m_sel = -1; this->build(); });
            });
            tog("Enabled", labels::cfg(i).on, [i](bool v) { labels::cfg(i).on = v; labels::save(i); });
            nm("Anchor", [i] { static char const* a[] = {"Top Left", "Top Right", "Bottom Left", "Bottom Right"}; return std::string(a[std::clamp(labels::cfg(i).anchor, 0, 3)]); },
               [i](int d) { auto& a = labels::cfg(i).anchor; a = (a + d + 4) % 4; labels::save(i); });
            nm("Font", [i] { return std::string(labels::cfg(i).font == 0 ? "Big" : "Chat"); }, [i](int) { auto& f = labels::cfg(i).font; f = 1 - f; labels::save(i); });
            nm("Size", [i] { return fmt::format("{:.2f}", labels::cfg(i).scale); }, [i](int d) { auto& s = labels::cfg(i).scale; s = std::clamp(s + d * 0.05f, 0.1f, 2.f); labels::save(i); });
            nm("Opacity", [i] { return fmt::format("{:.2f}", labels::cfg(i).opacity); }, [i](int d) { auto& o = labels::cfg(i).opacity; o = std::clamp(o + d * 0.1f, 0.1f, 1.f); labels::save(i); });
        }
    } else {
        tog("Prevent Death", c.preventDeath, [](bool v) { cfg().preventDeath = v; saveCfg(); });
        nm("Lookahead Ticks", [] { return fmt::format("{}", cfg().pdLook); }, [](int d) { cfg().pdLook = std::clamp(cfg().pdLook + d, 1, 10); saveCfg(); });
        tog("Maintain Gravity", c.maintainGravity, [](bool v) { cfg().maintainGravity = v; saveCfg(); });
        tog("Mirror Inputs", c.mirror, [](bool v) { cfg().mirror = v; saveCfg(); });
        tog("Mirror Inverted", c.mirrorInv, [](bool v) { cfg().mirrorInv = v; saveCfg(); });
        tog("No Mirror Portals", c.noMirror, [](bool v) { cfg().noMirror = v; saveCfg(); });
        for (int p = 0; p < 2; ++p) {
            std::string pn = p ? "P2" : "P1";
            items.push_back([=, this](float y) {
                this->row(y, std::string("Autoclicker " + pn), cfg().acOn[p], [p](bool v) { cfg().acOn[p] = v; saveCfg(); });
            });
            items.push_back([=, this](float y) {
                this->num(y, std::string("AC Hold Ticks " + pn), [p] { return fmt::format("{}", cfg().acHold[p]); }, [p](int d) { cfg().acHold[p] = std::clamp(cfg().acHold[p] + d, 1, 240); saveCfg(); });
            });
            items.push_back([=, this](float y) {
                this->num(y, std::string("AC Release Ticks " + pn), [p] { return fmt::format("{}", cfg().acRel[p]); }, [p](int d) { cfg().acRel[p] = std::clamp(cfg().acRel[p] + d, 1, 240); saveCfg(); });
            });
            items.push_back([=, this](float y) {
                this->num(y, std::string("AC Clicks/Hold " + pn), [p] { return fmt::format("{}", cfg().acClicks[p]); }, [p](int d) { cfg().acClicks[p] = std::clamp(cfg().acClicks[p] + d, 1, 10); saveCfg(); });
            });
        }
    }

    int pages = std::max(1, (int)((items.size() + per - 1) / per));
    m_page = std::clamp(m_page, 0, pages - 1);
    float y = H - 75.f;
    for (int i = m_page * per; i < std::min<int>(items.size(), (m_page + 1) * per); ++i) { items[i](y); y -= 28.f; }

    float fy = 22.f;
    if (pages > 1) {
        this->btn(60, fy, "<", [this] { m_page--; this->build(); });
        this->btn(110, fy, ">", [this] { m_page++; this->build(); });
        auto pl = CCLabelBMFont::create(fmt::format("{}/{}", m_page + 1, pages).c_str(), "goldFont.fnt");
        pl->setScale(.45f); pl->setPosition({160, fy}); m_content->addChild(pl);
    }
    if (m_tab == 3) {
        this->btn(250, fy, "Import", [this] {
            int n = 0; std::error_code ec;
            for (auto& e : std::filesystem::directory_iterator(macroDir(), ec)) {
                auto ext = e.path().extension().string();
                if (ext != ".gdr" && ext != ".txt" && ext != ".csv") continue;
                Macro m;
                if (importMacro(m, e.path()) && saveMacro(m, e.path().stem().string())) n++;
            }
            Notification::create(fmt::format("Imported {}", n), NotificationIcon::Info)->show();
            this->build();
        });
    }
}

void XyzMenu::row(float y, std::string label, bool on, std::function<void(bool)> cb) {
    auto l = CCLabelBMFont::create(label.c_str(), "bigFont.fnt");
    l->setScale(.45f); l->setAnchorPoint({0, .5f}); l->setPosition({30, y});
    m_content->addChild(l);
    auto t = CCMenuItemExt::createTogglerWithStandardSprites(.6f, [cb](CCMenuItemToggler* t) { cb(!t->isToggled()); });
    t->toggle(on);
    t->setPosition({W - 40, y});
    m_content->addChild(t);
}

void XyzMenu::num(float y, std::string label, std::function<std::string()> get, std::function<void(int)> step) {
    auto l = CCLabelBMFont::create(label.c_str(), "bigFont.fnt");
    l->setScale(.45f); l->setAnchorPoint({0, .5f}); l->setPosition({30, y});
    m_content->addChild(l);
    auto v = CCLabelBMFont::create(get().c_str(), "goldFont.fnt");
    v->setScale(.5f); v->setPosition({W - 100, y});
    m_content->addChild(v);
    auto mk = [&](char const* t, float x, int d) {
        auto b = CCMenuItemExt::createSpriteExtra(ButtonSprite::create(t, 20, true, "bigFont.fnt", "GJ_button_04.png", 20.f, .5f), [step, get, v, d](auto) {
            step(d); v->setString(get().c_str()); });
        b->setPosition({x, y});
        m_content->addChild(b);
    };
    mk("-", W - 150, -1);
    mk("+", W - 50, 1);
}

void XyzMenu::btn(float x, float y, char const* text, std::function<void()> cb) {
    auto b = CCMenuItemExt::createSpriteExtra(ButtonSprite::create(text, 50, true, "bigFont.fnt", "GJ_button_01.png", 22.f, .45f), [cb](auto) { cb(); });
    b->setPosition({x, y});
    m_content->addChild(b);
}

class FloatBtn : public CCLayer {
    CCMenuItemSpriteExtra* m_b = nullptr;
    CCPoint m_start; bool m_moved = false;
public:
    static FloatBtn* create() {
        auto r = new FloatBtn();
        if (r->init()) { r->autorelease(); return r; }
        delete r; return nullptr;
    }
    bool init() override {
        if (!CCLayer::init()) return false;
        auto spr = CCSprite::create("xyz-button.png"_spr);
        if (!spr) spr = CCSprite::createWithSpriteFrameName("GJ_optionsBtn_001.png");
        spr->setScale(.7f);
        spr->setOpacity(200);
        this->addChild(spr);
        this->setContentSize(spr->getContentSize() * .7f);
        spr->setPosition(this->getContentSize() / 2);
        this->setPosition({40.f, CCDirector::get()->getWinSize().height - 40.f});
        this->setTouchEnabled(true);
        this->schedule(schedule_selector(FloatBtn::tick), .25f);
        return true;
    }
    void registerWithTouchDispatcher() override {
        CCTouchDispatcher::get()->addTargetedDelegate(this, -600, true);
    }
    void tick(float) {
        auto pl = PlayLayer::get();
        this->setVisible(Mod::get()->getSettingValue<bool>("floating-button") && (!pl || pl->m_isPaused));
    }
    bool hit(CCTouch* t) {
        auto p = this->convertTouchToNodeSpace(t);
        auto s = this->getContentSize();
        return this->isVisible() && p.x >= 0 && p.y >= 0 && p.x <= s.width && p.y <= s.height;
    }
    bool ccTouchBegan(CCTouch* t, CCEvent*) override {
        if (!hit(t)) return false;
        m_start = t->getLocation(); m_moved = false;
        return true;
    }
    void ccTouchMoved(CCTouch* t, CCEvent*) override {
        auto d = t->getLocation() - t->getPreviousLocation();
        if (ccpDistance(t->getLocation(), m_start) > 8.f) m_moved = true;
        if (m_moved) this->setPosition(this->getPosition() + d);
    }
    void ccTouchEnded(CCTouch*, CCEvent*) override {
        if (!m_moved) XyzMenu::toggle();
    }
};

class $modify(XyzMenuLayer, MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) return false;
        static bool added = false;
        if (!added) {
            if (auto b = FloatBtn::create()) { OverlayManager::get()->addChild(b, 9999); added = true; }
        }
        return true;
    }
};

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
            if (key == KEY_Period && PlayLayer::get()) { stepOnce(); return true; }
        }
        return CCKeyboardDispatcher::dispatchKeyboardMSG(key, down, repeat, ts);
    }
};
#endif

$on_mod(Loaded) { loadCfg(); }

void XyzMenu::onClose(CCObject* s) {
    s_open = nullptr;
    Popup::onClose(s);
}
