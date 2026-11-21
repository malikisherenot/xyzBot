#include "Logger.hpp"
#include "State.hpp"
#include <algorithm>
#include <chrono>
#include <ctime>
#include <fstream>
#include <vector>

using namespace geode::prelude;

namespace xyz::logger {
namespace {
constexpr size_t kMaxFiles = 20;
constexpr uintmax_t kMaxBytes = 64ull * 1024 * 1024;

std::ofstream g_file;
bool g_failed = false;
uintmax_t g_bytes = 0;
std::chrono::steady_clock::time_point g_start, g_lastFlush;

std::string stamp(char const* format) {
    auto t = std::time(nullptr);
    std::tm tm{};
#ifdef GEODE_IS_WINDOWS
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char buf[64];
    std::strftime(buf, sizeof buf, format, &tm);
    return buf;
}

void trimOld(std::filesystem::path const& dir) {
    std::error_code ec;
    std::vector<std::filesystem::path> files;
    for (auto& e : std::filesystem::directory_iterator(dir, ec))
        if (e.is_regular_file(ec) && e.path().extension() == ".log") files.push_back(e.path());
    if (files.size() < kMaxFiles) return;
    std::sort(files.begin(), files.end()); // names start with the timestamp
    for (size_t i = 0; i < files.size() - (kMaxFiles - 1); ++i) std::filesystem::remove(files[i], ec);
}

void put(std::string const& text, bool flush);

bool open() {
    if (g_file.is_open()) return true;
    if (g_failed) return false;
    auto dir = persistDir("logs");
    trimOld(dir);
    g_file.open(dir / (stamp("%Y-%m-%d_%H-%M-%S") + ".log"), std::ios::out | std::ios::trunc);
    if (!g_file) { g_failed = true; return false; }
    g_start = g_lastFlush = std::chrono::steady_clock::now();
    put(fmt::format("xyzBot {} | GD {} | {} | {}", Mod::get()->getVersion().toVString(), Loader::get()->getGameVersion(), GEODE_PLATFORM_NAME, stamp("%Y-%m-%d %H:%M:%S")), true);
    return true;
}

void put(std::string const& text, bool flush) {
    if (!g_file.is_open() && !open()) return;
    if (g_bytes > kMaxBytes) return;
    auto now = std::chrono::steady_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - g_start).count();
    auto line = fmt::format("{:>9} {}\n", ms, text);
    g_file << line;
    g_bytes += line.size();
    if (g_bytes > kMaxBytes) { g_file << "size limit reached, logging stopped\n"; flush = true; }
    if (flush || now - g_lastFlush > std::chrono::seconds(1)) { g_file.flush(); g_lastFlush = now; }
}

void playerLine(std::string& out, char const* tag, PlayerObject* p) {
    out += fmt::format(" {} x={:.9g} y={:.9g} r={:.9g} v={:.9g}", tag, p->getPositionX(), p->getPositionY(), p->getRotation(), p->m_yVelocity);
}
}

void event(std::string const& text) {
    if (!cfg().logging) return;
    put("E " + text, true);
}

void input(uint32_t tick, int button, bool down, bool p2, char const* source) {
    if (!cfg().logging) return;
    put(fmt::format("I t={} b={} {} p{} {}", tick, button, down ? "down" : "up", p2 ? 2 : 1, source), true);
}

void position(uint32_t tick) {
    auto& c = cfg();
    if (!c.logging || !c.logPositions) return;
    if (tick % (uint32_t)std::max(1, c.logPosEvery) != 0) return;
    auto gl = activeLayer();
    if (!gl || !gl->m_player1) return;
    std::string line = fmt::format("P t={}", tick);
    playerLine(line, "p1", gl->m_player1);
    if (gl->m_gameState.m_isDualMode && gl->m_player2) playerLine(line, "p2", gl->m_player2);
    put(line, false);
}
}
