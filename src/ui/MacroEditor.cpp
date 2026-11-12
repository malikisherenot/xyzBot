#include "MacroEditor.hpp"
#include "../core/State.hpp"
#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/TextInput.hpp>
#include <algorithm>
#include <cctype>
#include <functional>
#include <map>

using namespace geode::prelude;

namespace {
constexpr float W = 420.f, H = 280.f;
constexpr int ROWS = 6;
constexpr size_t MAX_UNDO = 10;

class MEInputPopup : public geode::Popup {
    TextInput* m_input = nullptr;
    std::function<void(std::string)> m_cb;
public:
    static void open(std::string const& title, std::string const& hint, std::string const& cur, bool numeric, std::function<void(std::string)> cb) {
        auto p = new MEInputPopup();
        if (p->setup(title, hint, cur, numeric, std::move(cb))) { p->autorelease(); p->show(); }
        else delete p;
    }
    bool setup(std::string const& title, std::string const& hint, std::string const& cur, bool numeric, std::function<void(std::string)> cb) {
        m_cb = std::move(cb);
        if (!Popup::init(250.f, 135.f)) return false;
        this->setTitle(title);
        m_input = TextInput::create(180.f, hint, "bigFont.fnt");
        if (numeric) m_input->setFilter("-0123456789");
        m_input->setString(cur);
        m_input->setPosition({125.f, 72.f});
        m_mainLayer->addChild(m_input);
        auto menu = CCMenu::create();
        menu->setPosition({0, 0});
        m_mainLayer->addChild(menu);
        auto ok = CCMenuItemExt::createSpriteExtra(ButtonSprite::create("OK"), [this](auto) {
            auto s = m_input->getString();
            this->onClose(nullptr);
            m_cb(s);
        });
        ok->setPosition({125.f, 28.f});
        menu->addChild(ok);
        return true;
    }
};

struct Snap {
    std::vector<xyz::Input> in;
    std::vector<xyz::Fix> fx;
    uint32_t ticks = 0;
    bool hasFx = false;
};

class MacroEditor : public geode::Popup {
    std::string m_name;
    xyz::Macro m_m;
    std::vector<Snap> m_undo;
    int m_page = 0;
    int m_step = 1;
    bool m_dirty = false;
    CCNode* m_body = nullptr;

    static char const* btnName(int b) { return b == 2 ? "Left" : b == 3 ? "Right" : "Jump"; }

    int pages() const { return std::max(1, (int)((m_m.inputs.size() + ROWS - 1) / ROWS)); }

    void push(bool withFx) {
        Snap s;
        s.in = m_m.inputs; s.ticks = m_m.ticks;
        if (withFx) { s.fx = m_m.fixes; s.hasFx = true; }
        m_undo.push_back(std::move(s));
        if (m_undo.size() > MAX_UNDO) m_undo.erase(m_undo.begin());
        m_dirty = true;
    }

    void sortInputs() {
        std::stable_sort(m_m.inputs.begin(), m_m.inputs.end(), [](xyz::Input const& a, xyz::Input const& b) { return a.tick < b.tick; });
    }

    void note(std::string const& t, NotificationIcon i = NotificationIcon::Info) { Notification::create(t, i)->show(); }

    // releases anything still held at the end and removes duplicate states
    int cleanInputs() {
        sortInputs();
        std::map<int, bool> held;
        std::vector<xyz::Input> out;
        int removed = 0;
        for (auto const& in : m_m.inputs) {
            int key = in.button * 2 + (in.p2 ? 1 : 0);
            bool cur = held.count(key) ? held[key] : false;
            if (in.down == cur) { ++removed; continue; }
            held[key] = in.down;
            out.push_back(in);
        }
        uint32_t last = out.empty() ? 0 : out.back().tick;
        for (auto& [key, h] : held) {
            if (!h) continue;
            out.push_back({last + 1, (uint8_t)(key / 2), false, (bool)(key % 2)});
        }
        m_m.inputs = std::move(out);
        return removed;
    }

    void refresh() {
        m_page = std::clamp(m_page, 0, pages() - 1);
        m_body->removeAllChildren();
        auto menu = CCMenu::create();
        menu->setPosition({0, 0});
        m_body->addChild(menu);

        auto label = [&](std::string const& t, float x, float y, float sc, bool left, ccColor3B col = {255, 255, 255}) {
            auto l = CCLabelBMFont::create(t.c_str(), "bigFont.fnt");
            l->setScale(sc);
            l->setColor(col);
            l->setAnchorPoint({left ? 0.f : 0.5f, 0.5f});
            l->setPosition({x, y});
            m_body->addChild(l);
            return l;
        };
        auto button = [&](std::string const& t, float x, float y, int width, std::function<void()> cb, float sc = 0.5f, char const* tex = "GJ_button_04.png") {
            auto spr = ButtonSprite::create(t.c_str(), width, true, "bigFont.fnt", tex, 24.f, sc);
            auto b = CCMenuItemExt::createSpriteExtra(spr, [cb = std::move(cb)](auto) { queueInMainThread([cb] { cb(); }); });
            b->setPosition({x, y});
            menu->addChild(b);
            return b;
        };

        label(fmt::format("{}{}  |  {} inputs  |  {} TPS  |  {} ticks", m_name, m_dirty ? " *" : "", m_m.inputs.size(), m_m.tps, m_m.ticks),
              W / 2.f, H - 42.f, 0.32f, false, {200, 200, 200});

        // header
        float hy = H - 60.f;
        label("#", 16, hy, 0.28f, true, {140, 140, 140});
        label("TICK", 56, hy, 0.28f, true, {140, 140, 140});
        label("BTN", 120, hy, 0.28f, true, {140, 140, 140});
        label("PLR", 172, hy, 0.28f, true, {140, 140, 140});
        label("STATE", 220, hy, 0.28f, true, {140, 140, 140});

        for (int r = 0; r < ROWS; ++r) {
            size_t i = (size_t)m_page * ROWS + r;
            if (i >= m_m.inputs.size()) break;
            float y = H - 82.f - r * 25.f;
            auto const in = m_m.inputs[i];
            label(fmt::format("{}", i + 1), 16, y, 0.3f, true, {160, 160, 160});
            label(fmt::format("{}", in.tick), 56, y, 0.36f, true);
            button(btnName(in.button), 137, y, 44, [this, i] {
                push(false);
                auto& b = m_m.inputs[i].button;
                b = b >= 3 ? 1 : b + 1;
                refresh();
            }, 0.36f, "GJ_button_06.png");
            button(in.p2 ? "P2" : "P1", 190, y, 26, [this, i] {
                push(false);
                m_m.inputs[i].p2 = !m_m.inputs[i].p2;
                refresh();
            }, 0.36f, "GJ_button_06.png");
            button(in.down ? "Down" : "Up", 244, y, 40, [this, i] {
                push(false);
                m_m.inputs[i].down = !m_m.inputs[i].down;
                refresh();
            }, 0.36f, in.down ? "GJ_button_01.png" : "GJ_button_04.png");
            button(fmt::format("-{}", m_step), 300, y, 30, [this, i] {
                push(false);
                auto& t = m_m.inputs[i].tick;
                t = t > (uint32_t)m_step ? t - m_step : 0;
                sortInputs();
                refresh();
            }, 0.36f);
            button(fmt::format("+{}", m_step), 336, y, 30, [this, i] {
                push(false);
                m_m.inputs[i].tick += m_step;
                sortInputs();
                refresh();
            }, 0.36f);
            button("Del", 382, y, 28, [this, i] {
                push(false);
                m_m.inputs.erase(m_m.inputs.begin() + i);
                refresh();
            }, 0.36f, "GJ_button_06.png");
        }
        if (m_m.inputs.empty()) label("No inputs. Press Add.", W / 2.f, H - 120.f, 0.4f, false, {160, 160, 160});

        // navigation row
        float ny = 68.f;
        button("<", 40, ny, 24, [this] { --m_page; refresh(); }, 0.4f);
        label(fmt::format("{}/{}", m_page + 1, pages()), 84, ny, 0.34f, false);
        button(">", 128, ny, 24, [this] { ++m_page; refresh(); }, 0.4f);
        button("Go To", 178, ny, 44, [this] {
            MEInputPopup::open("Go To Tick", "Tick", "", true, [this](std::string s) {
                if (s.empty() || s == "-") return;
                uint32_t t = (uint32_t)std::max(0, utils::numFromString<int>(s).unwrapOr(0));
                size_t idx = 0;
                while (idx < m_m.inputs.size() && m_m.inputs[idx].tick < t) ++idx;
                m_page = (int)(idx / ROWS);
                refresh();
            });
        }, 0.4f);
        button("Add", 232, ny, 34, [this] {
            MEInputPopup::open("Add Click", "Tick", "", true, [this](std::string s) {
                if (s.empty() || s == "-") return;
                uint32_t t = (uint32_t)std::max(0, utils::numFromString<int>(s).unwrapOr(0));
                push(false);
                m_m.inputs.push_back({t, 1, true, false});
                m_m.inputs.push_back({t + 1, 1, false, false});
                sortInputs();
                size_t idx = 0;
                while (idx < m_m.inputs.size() && m_m.inputs[idx].tick < t) ++idx;
                m_page = (int)(idx / ROWS);
                m_m.ticks = std::max(m_m.ticks, t + 1);
                refresh();
            });
        }, 0.4f, "GJ_button_01.png");
        button("Shift All", 300, ny, 62, [this] {
            MEInputPopup::open("Shift All Ticks", "+/- ticks", "", true, [this](std::string s) {
                int d = utils::numFromString<int>(s).unwrapOr(0);
                if (d == 0) return;
                push(true);
                auto mv = [d](uint32_t& t) { t = (uint32_t)std::max<int64_t>(0, (int64_t)t + d); };
                for (auto& in : m_m.inputs) mv(in.tick);
                for (auto& f : m_m.fixes) mv(f.tick);
                mv(m_m.ticks);
                refresh();
            });
        }, 0.4f);
        button("Trim", 362, ny, 40, [this] {
            MEInputPopup::open("Trim After Tick", "Tick", "", true, [this](std::string s) {
                if (s.empty() || s == "-") return;
                uint32_t t = (uint32_t)std::max(0, utils::numFromString<int>(s).unwrapOr(0));
                push(true);
                auto& in = m_m.inputs;
                in.erase(std::remove_if(in.begin(), in.end(), [t](xyz::Input const& i) { return i.tick > t; }), in.end());
                auto& fx = m_m.fixes;
                fx.erase(std::remove_if(fx.begin(), fx.end(), [t](xyz::Fix const& f) { return f.tick > t; }), fx.end());
                m_m.ticks = std::min(m_m.ticks, t);
                cleanInputs();
                refresh();
            });
        }, 0.4f);

        // action row
        float ay = 30.f;
        button("Save", 50, ay, 50, [this] {
            bool ok = xyz::saveMacro(m_m, m_name);
            if (ok) m_dirty = false;
            note(ok ? "Saved " + m_name : std::string("Could not save"), ok ? NotificationIcon::Success : NotificationIcon::Error);
            refresh();
        }, 0.45f, "GJ_button_01.png");
        button("Save As", 122, ay, 60, [this] {
            MEInputPopup::open("Save As", "Macro name", m_name, false, [this](std::string s) {
                std::string clean;
                for (char c : s) if (std::isalnum((unsigned char)c) || c == '_' || c == '-' || c == ' ' || c == '.') clean += c;
                if (clean.empty()) { note("Invalid name", NotificationIcon::Error); return; }
                bool ok = xyz::saveMacro(m_m, clean);
                if (ok) { m_name = clean; m_dirty = false; }
                note(ok ? "Saved " + clean : std::string("Could not save"), ok ? NotificationIcon::Success : NotificationIcon::Error);
                refresh();
            });
        }, 0.45f);
        button("Undo", 190, ay, 44, [this] {
            if (m_undo.empty()) { note("Nothing to undo"); return; }
            auto s = std::move(m_undo.back());
            m_undo.pop_back();
            m_m.inputs = std::move(s.in);
            m_m.ticks = s.ticks;
            if (s.hasFx) m_m.fixes = std::move(s.fx);
            m_dirty = true;
            refresh();
        }, 0.45f);
        button("Clean", 246, ay, 46, [this] {
            push(false);
            int n = cleanInputs();
            note(fmt::format("Cleaned, removed {} duplicate inputs", n), NotificationIcon::Success);
            refresh();
        }, 0.45f);
        button(fmt::format("Step {}", m_step), 320, ay, 62, [this] {
            m_step = m_step == 1 ? 5 : m_step == 5 ? 10 : m_step == 10 ? 50 : 1;
            refresh();
        }, 0.45f);
        button("Close", 384, ay, 46, [this] { this->onClose(nullptr); }, 0.45f);
    }

public:
    static void open(std::string const& name) {
        auto p = new MacroEditor();
        if (p->setup(name)) { p->autorelease(); p->show(); }
        else delete p;
    }

    bool setup(std::string const& name) {
        m_name = name;
        if (!xyz::loadMacro(m_m, name)) { Notification::create("Could not load macro", NotificationIcon::Error)->show(); return false; }
        if (!Popup::init(W, H)) return false;
        this->setTitle("Macro Editor");
        m_body = CCNode::create();
        m_body->setPosition({0, 0});
        m_mainLayer->addChild(m_body);
        refresh();
        return true;
    }
};
}

namespace xyz {
void openMacroEditor(std::string const& name) { MacroEditor::open(name); }
}
