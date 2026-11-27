#include "Lua.hpp"
#include "../State.hpp"
#include <algorithm>
#include <cmath>
#include <deque>
#include <fstream>
#include <functional>
#include <sstream>

extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

using namespace geode::prelude;

namespace xyz::lua {
namespace {

constexpr size_t MEM_LIMIT = 64u * 1024u * 1024u;  // per script VM
constexpr int INSTR_LIMIT = 2'000'000;            // VM instructions one callback may run
constexpr size_t MAX_FILE = 1u << 20;

struct Script {
    std::string name;
    int env = LUA_NOREF;
    bool failed = false; // an error stops the events of this script until Reload
};

lua_State* L = nullptr;
size_t g_mem = 0;
std::vector<Script> g_scripts;
std::deque<std::string> g_out;
GJBaseGameLayer* g_gl = nullptr;
int g_attempt = 0;
bool g_ready = false, g_inject = false;
std::string g_status = "No scripts loaded";

struct Click { bool down; int button; bool p1; };
std::vector<Click> g_clicks;

void out(std::string s) {
    log::info("[lua] {}", s);
    g_out.push_back(std::move(s));
    while (g_out.size() > 40) g_out.pop_front();
}

void* alloc(void*, void* p, size_t old, size_t n) {
    if (!p) old = 0;
    if (n == 0) { g_mem -= old; free(p); return nullptr; }
    if (g_mem - old + n > MEM_LIMIT) return nullptr;
    void* r = realloc(p, n);
    if (r) g_mem = g_mem - old + n;
    return r;
}

void hook(lua_State* s, lua_Debug*) { luaL_error(s, "script ran too long (endless loop?)"); }

// ---- the xyz table
int l_log(lua_State* s) {
    int n = lua_gettop(s);
    std::string line;
    for (int i = 1; i <= n; ++i) {
        size_t len = 0;
        char const* t = luaL_tolstring(s, i, &len);
        if (i > 1) line += '\t';
        line.append(t, len);
        lua_pop(s, 1);
    }
    out(line);
    return 0;
}
int l_tick(lua_State* s) { lua_pushinteger(s, rt().tick); return 1; }
int l_tps(lua_State* s) { lua_pushnumber(s, curTps()); return 1; }
int l_attempt(lua_State* s) { lua_pushinteger(s, g_attempt); return 1; }
int l_state(lua_State* s) {
    auto m = rt().mode;
    lua_pushstring(s, m == Mode::Record ? "recording" : m == Mode::Play ? "playing" : "idle");
    return 1;
}
int l_notify(lua_State* s) {
    Notification::create(luaL_checkstring(s, 1), NotificationIcon::Info)->show();
    return 0;
}
int l_level(lua_State* s) {
    auto pl = PlayLayer::get();
    if (!pl || !pl->m_level) { lua_pushnil(s); return 1; }
    lua_pushstring(s, pl->m_level->m_levelName.c_str());
    lua_pushinteger(s, pl->m_level->m_levelID.value());
    return 2;
}
int l_click(lua_State* s) {
    luaL_checkany(s, 1);
    bool down = lua_toboolean(s, 1);
    int button = (int)luaL_optinteger(s, 2, 1);
    int player = (int)luaL_optinteger(s, 3, 1);
    if (button < 1 || button > 3) return luaL_error(s, "button must be 1 (jump), 2 (left) or 3 (right)");
    if (g_clicks.size() < 64) g_clicks.push_back({down, button, player != 2});
    return 0;
}
int l_player(lua_State* s) {
    int n = (int)luaL_optinteger(s, 1, 1);
    PlayerObject* p = g_gl ? (n == 2 ? g_gl->m_player2 : g_gl->m_player1) : nullptr;
    if (!p) { lua_pushnil(s); return 1; }
    lua_newtable(s);
    auto num = [&](char const* k, double v) { lua_pushnumber(s, v); lua_setfield(s, -2, k); };
    auto flag = [&](char const* k, bool v) { lua_pushboolean(s, v); lua_setfield(s, -2, k); };
    num("x", p->getPositionX());
    num("y", p->getPositionY());
    num("yvel", p->m_yVelocity);
    num("rotation", p->getRotation());
    num("speed", p->m_playerSpeed);
    num("gravity", p->m_gravityMod);
    num("scale", p->getScale());
    flag("on_ground", p->m_isOnGround);
    flag("dead", p->m_isDead);
    flag("upside_down", p->m_isUpsideDown);
    char const* mode = p->m_isShip ? "ship" : p->m_isBall ? "ball" : p->m_isBird ? "ufo" : p->m_isDart ? "wave"
        : p->m_isRobot ? "robot" : p->m_isSpider ? "spider" : p->m_isSwing ? "swing" : "cube";
    lua_pushstring(s, mode);
    lua_setfield(s, -2, "mode");
    return 1;
}

// xyz.set_player(player, field, value): writes a value of the live player. Experimental, a macro played without the same
// edits will desync. Fields: x, y, rotation, yvel, speed, gravity, scale.
int l_set_player(lua_State* s) {
    int n = (int)luaL_checkinteger(s, 1);
    std::string f = luaL_checkstring(s, 2);
    double v = luaL_checknumber(s, 3);
    PlayerObject* p = g_gl ? (n == 2 ? g_gl->m_player2 : g_gl->m_player1) : nullptr;
    if (!p) return luaL_error(s, "there is no player %d right now", n);
    if (!std::isfinite(v)) return luaL_error(s, "the value must be a finite number");
    if (f == "x") p->setPositionX((float)v);
    else if (f == "y") p->setPositionY((float)v);
    else if (f == "rotation") p->setRotation((float)v);
    else if (f == "yvel") p->m_yVelocity = v;
    else if (f == "speed") p->m_playerSpeed = std::clamp(v, 0.0, 10.0);
    else if (f == "gravity") p->m_gravityMod = std::clamp(v, -20.0, 20.0);
    else if (f == "scale") p->setScale((float)std::clamp(v, 0.05, 20.0));
    else return luaL_error(s, "unknown field '%s' (x, y, rotation, yvel, speed, gravity, scale)", f.c_str());
    return 0;
}

void pushXyz(lua_State* s) {
    static luaL_Reg fns[] = {
        {"log", l_log}, {"tick", l_tick}, {"tps", l_tps}, {"attempt", l_attempt}, {"state", l_state},
        {"notify", l_notify}, {"level", l_level}, {"click", l_click}, {"player", l_player}, {"set_player", l_set_player}, {nullptr, nullptr}};
    luaL_newlib(s, fns);
}

// ---- the VM: only a safe set of the standard library is visible to scripts
void closeVm() {
    for (auto& sc : g_scripts) if (L && sc.env != LUA_NOREF) luaL_unref(L, LUA_REGISTRYINDEX, sc.env);
    g_scripts.clear();
    if (L) { lua_close(L); L = nullptr; }
    g_mem = 0;
}

int safeIndex = LUA_NOREF;

void openVm() {
    L = lua_newstate(alloc, nullptr);
    if (!L) { g_status = "Could not start Lua (out of memory)"; return; }
    luaL_requiref(L, "_G", luaopen_base, 1);          lua_pop(L, 1);
    luaL_requiref(L, LUA_TABLIBNAME, luaopen_table, 1);   lua_pop(L, 1);
    luaL_requiref(L, LUA_STRLIBNAME, luaopen_string, 1);  lua_pop(L, 1);
    luaL_requiref(L, LUA_MATHLIBNAME, luaopen_math, 1);   lua_pop(L, 1);
    luaL_requiref(L, LUA_UTF8LIBNAME, luaopen_utf8, 1);   lua_pop(L, 1);
    luaL_requiref(L, LUA_COLIBNAME, luaopen_coroutine, 1); lua_pop(L, 1);

    lua_newtable(L); // the safe globals every script sees
    int safe = lua_gettop(L);
    for (char const* n : {"assert", "error", "ipairs", "next", "pairs", "pcall", "select", "tonumber", "tostring", "type",
                          "xpcall", "rawget", "rawset", "rawequal", "rawlen", "setmetatable", "getmetatable",
                          LUA_TABLIBNAME, LUA_STRLIBNAME, LUA_MATHLIBNAME, LUA_UTF8LIBNAME, LUA_COLIBNAME}) {
        lua_getglobal(L, n);
        lua_setfield(L, safe, n);
    }
    pushXyz(L);
    lua_setfield(L, safe, "xyz");
    lua_pushcfunction(L, l_log);
    lua_setfield(L, safe, "print");
    safeIndex = luaL_ref(L, LUA_REGISTRYINDEX); // pops the safe table
}

bool loadScript(std::string const& name) {
    auto path = dir() / name;
    std::ifstream f(path, std::ios::binary);
    if (!f) { out(name + ": cannot read the file"); return false; }
    std::stringstream ss; ss << f.rdbuf();
    std::string code = ss.str();
    if (code.size() > MAX_FILE) { out(name + ": file is bigger than 1 MB"); return false; }

    lua_newtable(L); // this script's own globals, reads fall through to the safe ones
    lua_newtable(L);
    lua_rawgeti(L, LUA_REGISTRYINDEX, safeIndex);
    lua_setfield(L, -2, "__index");
    lua_setmetatable(L, -2);
    int envTop = lua_gettop(L);

    if (luaL_loadbufferx(L, code.data(), code.size(), ("@" + name).c_str(), "t") != LUA_OK) {
        out(lua_tostring(L, -1));
        lua_settop(L, envTop - 1);
        return false;
    }
    lua_pushvalue(L, envTop);
    lua_setupvalue(L, -2, 1); // _ENV
    lua_sethook(L, hook, LUA_MASKCOUNT, INSTR_LIMIT);
    int r = lua_pcall(L, 0, 0, 0);
    lua_sethook(L, nullptr, 0, 0);
    if (r != LUA_OK) {
        out(lua_tostring(L, -1) ? lua_tostring(L, -1) : "script error");
        lua_settop(L, envTop - 1);
        return false;
    }
    lua_pushvalue(L, envTop);
    int ref = luaL_ref(L, LUA_REGISTRYINDEX);
    lua_settop(L, envTop - 1);
    g_scripts.push_back({name, ref, false});
    return true;
}

void ensureReady() {
    if (g_ready) return;
    g_ready = true;
    std::error_code ec;
    std::filesystem::create_directories(dir(), ec);
    if (files().empty()) {
        std::ofstream f(dir() / "example.lua");
        f << R"(-- Example xyzBot script. Turn it on in the Scripts tab, then press Reload.
-- Events (define the ones you need):
--   function on_tick(tick) end                  every physics tick
--   function on_input(button, down, player) end  your own presses (button 1 = jump, 2 = left, 3 = right)
--   function on_death() end
--   function on_reset(attempt) end              a new attempt began
-- Calls: xyz.tick() xyz.tps() xyz.attempt() xyz.state() xyz.level() xyz.player(1 or 2)
--        xyz.click(down, button, player) xyz.log(...) xyz.notify(text)
-- xyz.player(n) gives a table: x, y, yvel, rotation, speed, gravity, scale, on_ground, dead, upside_down, mode
-- xyz.set_player(n, field, value) changes the live player. Fields: x, y, rotation, yvel, speed, gravity, scale.
--   Experimental: a macro only plays the same way if the same edits are made again at the same ticks,
--   so use it from a script that runs every time, for example:
--     function on_tick(tick) if tick == 600 then xyz.set_player(1, "speed", 1.3) end end

function on_reset(attempt)
    xyz.log("attempt", attempt)
end

function on_tick(tick)
    local p = xyz.player(1)
    if p and tick % 240 == 0 then
        xyz.log(string.format("tick %d  x=%.1f  y=%.1f  %s", tick, p.x, p.y, p.mode))
    end
end
)";
    }
    openVm();
    if (!L) return;
    int loaded = 0, bad = 0;
    for (auto& n : files()) {
        if (!enabled(n)) continue;
        if (loadScript(n)) ++loaded; else ++bad;
    }
    g_status = fmt::format("{} script(s) running{}", loaded, bad ? fmt::format(", {} with errors", bad) : std::string());
}

// calls one event of every script, pushArgs puts the arguments on the stack and returns how many
void fire(char const* ev, std::function<int(lua_State*)> const& pushArgs) {
    if (!L || g_scripts.empty()) return;
    for (auto& sc : g_scripts) {
        if (sc.failed) continue;
        int top = lua_gettop(L);
        lua_rawgeti(L, LUA_REGISTRYINDEX, sc.env);
        lua_getfield(L, -1, ev);
        if (!lua_isfunction(L, -1)) { lua_settop(L, top); continue; }
        int n = pushArgs(L);
        lua_sethook(L, hook, LUA_MASKCOUNT, INSTR_LIMIT);
        int r = lua_pcall(L, n, 0, 0);
        lua_sethook(L, nullptr, 0, 0);
        if (r != LUA_OK) {
            out(sc.name + ": " + (lua_tostring(L, -1) ? lua_tostring(L, -1) : "error") + " (script stopped)");
            sc.failed = true;
            g_status = "A script stopped with an error, see the output below";
        }
        lua_settop(L, top);
    }
}

} // namespace

std::filesystem::path dir() { return persistDir("scripts"); }

std::vector<std::string> files() {
    std::vector<std::string> v;
    std::error_code ec;
    for (auto& e : std::filesystem::directory_iterator(dir(), ec))
        if (e.is_regular_file() && e.path().extension() == ".lua") v.push_back(e.path().filename().string());
    std::sort(v.begin(), v.end());
    return v;
}

bool enabled(std::string const& name) {
    auto v = Mod::get()->getSavedValue<std::vector<std::string>>("lua-enabled");
    return std::find(v.begin(), v.end(), name) != v.end();
}

void setEnabled(std::string const& name, bool on) {
    auto v = Mod::get()->getSavedValue<std::vector<std::string>>("lua-enabled");
    v.erase(std::remove(v.begin(), v.end(), name), v.end());
    if (on) v.push_back(name);
    Mod::get()->setSavedValue("lua-enabled", v);
}

void reload() {
    closeVm();
    g_clicks.clear();
    g_ready = false;
    ensureReady();
}

std::string status() { ensureReady(); return g_status; }
std::vector<std::string> output() { return {g_out.begin(), g_out.end()}; }

void onTick(GJBaseGameLayer* gl) {
    ensureReady();
    if (!L || rt().simulating) return;
    g_gl = gl;
    if (!g_scripts.empty()) fire("on_tick", [](lua_State* s) { lua_pushinteger(s, rt().tick); return 1; });
    if (!g_clicks.empty()) {
        auto cs = std::move(g_clicks);
        g_clicks.clear();
        if (rt().mode == Mode::Play) return; // a playing macro is not disturbed
        g_inject = true;
        for (auto& c : cs) gl->handleButton(c.down, c.button, c.p1);
        g_inject = false;
    }
}

void onInput(int button, bool down, bool p1) {
    if (g_inject || !g_ready || g_scripts.empty() || rt().simulating) return;
    fire("on_input", [&](lua_State* s) {
        lua_pushinteger(s, button); lua_pushboolean(s, down); lua_pushinteger(s, p1 ? 1 : 2);
        return 3;
    });
}

void onDeath() {
    if (!g_ready || g_scripts.empty() || rt().simulating) return;
    fire("on_death", [](lua_State*) { return 0; });
}

void onReset(int attempt) {
    g_attempt = attempt;
    ensureReady();
    if (g_scripts.empty()) return;
    fire("on_reset", [&](lua_State* s) { lua_pushinteger(s, attempt); return 1; });
}

} // namespace xyz::lua
