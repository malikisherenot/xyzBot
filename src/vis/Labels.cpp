#include "Vis.hpp"
#include "../core/State.hpp"
#include <Geode/binding/RingObject.hpp>
#include <Geode/binding/GameToolbox.hpp>

using namespace geode::prelude;

namespace xyz::labels {
namespace {
struct Def {
    std::string id, name;
    std::function<std::string(CCLabelBMFont*)> fn;
    Cfg c;
    CCLabelBMFont* node = nullptr;
};
CCNode* g_ui = nullptr;
std::vector<Def>& defs();
bool g_refresh = true;

std::string pv(PlayLayer* pl, char const* n, std::function<double(PlayerObject*)> f, int prec) {
    auto a = pl->m_player1; auto b = pl->m_player2;
    if (pl->m_gameState.m_isDualMode && b) return fmt::format("{}: {:.{}f} / {:.{}f}", n, f(a), prec, f(b), prec);
    return fmt::format("{}: {:.{}f}", n, f(a), prec);
}

std::string_view orb(RingObject* r) {
    switch (r->m_objectID) {
        case 36: return "Yellow"; case 84: return "Blue"; case 141: return "Pink"; case 1022: return "Green";
        case 1330: return "Black"; case 1333: return "Red"; case 1594: return "Toggle"; case 1704: return "Green Dash";
        case 1751: return "Pink Dash"; case 3004: return "Spider"; case 3027: return "Teleport";
        default: return "Unknown";
    }
}

std::vector<Def>& defs() {
    static std::vector<Def> d;
    if (!d.empty()) return d;
    auto add = [&](char const* id, char const* nm, std::function<std::string(CCLabelBMFont*)> f) {
        Def x; x.id = id; x.name = nm; x.fn = f;
        auto m = Mod::get(); std::string k = std::string("lbl_") + id;
        x.c.on = m->getSavedValue<bool>(k + "_on", false);
        x.c.anchor = m->getSavedValue<int>(k + "_anchor", 0);
        x.c.font = m->getSavedValue<int>(k + "_font", 1);
        x.c.opacity = (float)m->getSavedValue<double>(k + "_op", 1.0);
        x.c.scale = (float)m->getSavedValue<double>(k + "_sc", 0.7);
        d.push_back(std::move(x));
    };
    auto PL = [] { return PlayLayer::get(); };
    add("tick", "Tick", [](auto) { return fmt::format("Tick: {}", rt().tick); });
    add("game_tick", "Internal Game Tick", [=](auto) { return fmt::format("Game tick: {}", PL()->m_gameState.m_currentProgress); });
    add("tps", "TPS", [](auto) { return fmt::format("TPS: {}", (int)curTps()); });
    add("player_x", "Player X", [=](auto) { return pv(PL(), "X", [](PlayerObject* p) { return (double)p->getPositionX(); }, 6); });
    add("player_y", "Player Y", [=](auto) { return pv(PL(), "Y", [](PlayerObject* p) { return (double)p->getPositionY(); }, 6); });
    add("player_xvel", "Player X Velocity", [=](auto) { return pv(PL(), "X Velocity", [](PlayerObject* p) { return (double)p->m_platformerXVelocity; }, 6); });
    add("player_yvel", "Player Y Velocity", [=](auto) { return pv(PL(), "Y Velocity", [](PlayerObject* p) { return (double)p->m_yVelocity; }, 3); });
    add("player_rot", "Player Rotation", [=](auto) { return pv(PL(), "Rotation", [](PlayerObject* p) { return (double)p->getRotation(); }, 3); });
    add("player_speed", "Player Speed", [=](auto) { return pv(PL(), "Speed", [](PlayerObject* p) { return (double)p->m_playerSpeed; }, 2); });
    add("player_grav", "Player Gravity", [=](auto) {
        auto pl = PL(); auto n = [](PlayerObject* p) { return p->m_isUpsideDown ? "Flipped" : "Normal"; };
        if (pl->m_gameState.m_isDualMode && pl->m_player2) return fmt::format("Gravity: {} / {}", n(pl->m_player1), n(pl->m_player2));
        return fmt::format("Gravity: {}", n(pl->m_player1));
    });
    add("player_alive", "Player Dead/Alive", [=](auto) {
        auto pl = PL(); auto n = [](PlayerObject* p) { return p->m_isDead ? "Dead" : "Alive"; };
        if (pl->m_gameState.m_isDualMode && pl->m_player2) return fmt::format("{} / {}", n(pl->m_player1), n(pl->m_player2));
        return fmt::format("{}", n(pl->m_player1));
    });
    add("checkpoints", "Checkpoint #", [](auto) { return fmt::format("Checkpoints: {}", rt().cpCount); });
    add("level_time", "Level Time", [=](auto) { return fmt::format("Level Time: {:.6f}s", PL()->m_gameState.m_levelTime); });
    add("time_warp", "Time Warp", [=](auto) { return fmt::format("Time Warp: {:.2f}x", PL()->m_gameState.m_timeWarp); });
    add("bot_state", "Bot State", [](auto) {
        auto m = rt().mode;
        return fmt::format("Bot State: {}", m == Mode::Play ? "Playing" : m == Mode::Record ? "Recording" : "Idle");
    });
    add("random_state", "Random Seed State", [](auto) { return fmt::format("Random State: {}", GameToolbox::getfast_srand()); });
    add("action_index", "Action Index", [](auto) {
        auto& r = rt();
        return fmt::format("Action Index: {}/{}", r.nextIn, r.mode == Mode::Record ? r.rec.inputs.size() : r.play.inputs.size());
    });
    add("prevent_death", "Prevent Death", [](auto) {
        auto& r = rt();
        return fmt::format("Prevent Death: {}", !xyz::cfg().preventDeath ? "Off" : r.frozen ? "Death predicted, frozen" : "Armed");
    });
    add("touching_orbs", "Touching Orbs", [=](auto) {
        std::string t = "Touching Orbs: ";
        auto one = [&](PlayerObject* p) {
            bool any = false;
            if (p->m_touchingRings)
                for (auto ring : CCArrayExt<RingObject*>(p->m_touchingRings)) {
                    if (ring->hasBeenActivatedByPlayer(p)) continue;
                    any = true; t += orb(ring);
                    if (ring->m_objectID == 1594) t += fmt::format("({})", ring->m_targetGroupID);
                    if (ring->m_isMultiActivate) t += "*";
                    t += " ";
                }
            if (!any) t += "None";
        };
        auto pl = PL();
        one(pl->m_player1);
        if (pl->m_gameState.m_isDualMode && pl->m_player2) { t += " / "; one(pl->m_player2); }
        return t;
    });
    add("cps", "CPS", [=](CCLabelBMFont* l) {
        int a = cpsNow(0), b = cpsNow(1);
        l->setColor((a > 16 || b > 16) ? ccColor3B{255, 128, 128} : ccColor3B{255, 255, 255});
        if (PL()->m_gameState.m_isDualMode && PL()->m_levelSettings->m_twoPlayerMode) return fmt::format("CPS: {} / {}", a, b);
        return fmt::format("CPS: {}", a);
    });
    add("max_cps", "Max CPS", [=](CCLabelBMFont* l) {
        auto& r = rt();
        l->setColor((r.maxCps[0] > 16 || r.maxCps[1] > 16) ? ccColor3B{255, 128, 128} : ccColor3B{255, 255, 255});
        if (PL()->m_levelSettings->m_twoPlayerMode) return fmt::format("Max CPS: {} / {}", r.maxCps[0], r.maxCps[1]);
        return fmt::format("Max CPS: {}", r.maxCps[0]);
    });
    add("last_input", "Ticks Since Last Input", [](auto) {
        auto& r = rt();
        auto& in = r.mode == Mode::Record ? r.rec.inputs : r.play.inputs;
        if (in.empty()) return std::string("No Inputs In Replay");
        if (r.mode == Mode::Play) {
            if (r.nextIn == 0) return std::string("Waiting For First Input");
            return fmt::format("Ticks Since Last Input: {}", r.tick - in[std::min(r.nextIn, in.size()) - 1].tick);
        }
        return fmt::format("Ticks Since Last Input: {}", r.tick - in.back().tick);
    });
    add("ssb_factor", "Scroll Speed Bug Factor", [](auto) { return fmt::format("SSB Factor: {:.6f}", ssbFactor()); });
    add("ssb_event", "Scroll Speed Bug Event", [](CCLabelBMFont* l) {
        auto& r = rt();
        l->setColor(r.ssbActive ? ccColor3B{255, 160, 80} : ccColor3B{255, 255, 255});
        if (r.ssbActive) return fmt::format("SSB Event: ACTIVE ({:.6f}) | Count: {}", r.ssbLast, r.ssbEvents);
        return fmt::format("SSB Event: None | Count: {}", r.ssbEvents);
    });
    add("frame_budget", "Frame Budget", [](auto) { return fmt::format("Budget: {:.0f} ms", xyz::cfg().budgetMs); });
    return d;
}
}

size_t count() { return defs().size(); }
std::string const& id(size_t i) { return defs()[i].id; }
std::string const& name(size_t i) { return defs()[i].name; }
Cfg& cfg(size_t i) { return defs()[i].c; }
void invalidate() { g_refresh = true; }
void save(size_t i) {
    auto& d = defs()[i]; auto m = Mod::get(); std::string k = "lbl_" + d.id;
    m->setSavedValue<bool>(k + "_on", d.c.on); m->setSavedValue<int>(k + "_anchor", d.c.anchor);
    m->setSavedValue<int>(k + "_font", d.c.font); m->setSavedValue<double>(k + "_op", d.c.opacity);
    m->setSavedValue<double>(k + "_sc", d.c.scale);
    g_refresh = true;
}
}

namespace xyz::vis {
void updateLabels(PlayLayer* pl) {
    using namespace labels;
    if (!pl || !pl->m_uiLayer) return;
    auto& d = defs();
    auto ui = pl->m_uiLayer;
    if (g_ui != ui) { g_ui = ui; for (auto& l : d) l.node = nullptr; g_refresh = true; }
    bool refresh = g_refresh;
    float h[4] = {0, 0, 0, 0};
    auto size = ui->getContentSize();
    for (auto& l : d) {
        bool show = xyz::cfg().labels && l.c.on;
        if (!l.node) {
            if (!show) continue;
            l.node = CCLabelBMFont::create("...", l.c.font == 0 ? "bigFont.fnt" : "chatFont.fnt");
            l.node->setID(fmt::format("{}/label.{}", Mod::get()->getID(), l.id));
            ui->addChild(l.node, 100000);
            refresh = true;
        }
        auto n = l.node;
        n->setVisible(show);
        if (!show) continue;
        if (refresh) {
            n->setFntFile(l.c.font == 0 ? "bigFont.fnt" : "chatFont.fnt");
            n->setOpacity((GLubyte)(l.c.opacity * 255.f));
            n->setScale(l.c.scale);
        }
        auto str = l.fn(n);
        if (std::strcmp(n->getString(), str.c_str()) != 0) n->setString(str.c_str());
        if (refresh) {
            int a = std::clamp(l.c.anchor, 0, 3);
            float sp = 8.f, inner = 4.f * l.c.scale;
            CCPoint pos, anc;
            switch (a) {
                case 0: pos = ccp(sp, size.height - sp - h[0]); anc = ccp(0, 1); break;
                case 1: pos = ccp(size.width - sp, size.height - sp - h[1]); anc = ccp(1, 1); break;
                case 2: pos = ccp(10.f, h[2] + sp); anc = ccp(0, 0); break;
                default: pos = ccp(size.width - sp, h[3] + sp); anc = ccp(1, 0); break;
            }
            n->setPosition(pos); n->setAnchorPoint(anc);
            h[a] += n->getScaledContentSize().height + inner;
        }
    }
    g_refresh = false;
}
}
