#include "Discord.hpp"
#include "../core/State.hpp"
#include <Geode/Geode.hpp>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#if defined(GEODE_IS_WINDOWS)
#include <windows.h>
#elif defined(GEODE_IS_MACOS)
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#endif

#if defined(GEODE_IS_WINDOWS) || defined(GEODE_IS_MACOS)
#define XYZ_DISCORD 1
#else
#define XYZ_DISCORD 0
#endif

using namespace geode::prelude;
using Clock = std::chrono::steady_clock;

namespace xyz { bool isRendering(); }

namespace xyz::discord {
namespace {

struct Shared {
    std::mutex mx;
    std::string details, state;
    long long startEpoch = 0;
    std::string status = "Off";
    Clock::time_point lastTick{};
    bool haveTick = false;
} g;

std::atomic<bool> g_run{false};
std::atomic<bool> g_threadStarted{false};

std::string clientId() {
    std::string saved = Mod::get()->getSavedValue<std::string>("discord-app-id");
    return saved.empty() ? std::string(kDiscordClientId) : saved;
}

std::string esc(std::string const& s) {
    std::string o;
    for (unsigned char c : s) {
        switch (c) {
            case '"': o += "\\\""; break;
            case '\\': o += "\\\\"; break;
            case '\n': o += "\\n"; break;
            case '\r': o += "\\r"; break;
            case '\t': o += "\\t"; break;
            default: if (c < 0x20) o += fmt::format("\\u{:04x}", c); else o += (char)c;
        }
    }
    return o;
}

long long epochNow() { return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count(); }

// ---------------- the pipe ----------------
#if XYZ_DISCORD
struct Pipe {
#if defined(GEODE_IS_WINDOWS)
    HANDLE h = INVALID_HANDLE_VALUE;
    bool open() {
        for (int i = 0; i < 10; ++i) {
            auto name = fmt::format("\\\\.\\pipe\\discord-ipc-{}", i);
            h = CreateFileA(name.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
            if (h != INVALID_HANDLE_VALUE) return true;
        }
        return false;
    }
    bool write(void const* p, size_t n) { DWORD w = 0; return WriteFile(h, p, (DWORD)n, &w, nullptr) && w == n; }
    bool read(void* p, size_t n) { DWORD r = 0; return ReadFile(h, p, (DWORD)n, &r, nullptr) && r == n; }
    void close() { if (h != INVALID_HANDLE_VALUE) CloseHandle(h); h = INVALID_HANDLE_VALUE; }
    bool ok() const { return h != INVALID_HANDLE_VALUE; }
#else
    int fd = -1;
    bool open() {
        std::vector<std::string> bases;
        for (char const* e : {"XDG_RUNTIME_DIR", "TMPDIR", "TMP", "TEMP"}) if (auto v = getenv(e)) bases.push_back(v);
        bases.push_back("/tmp");
        for (auto& b : bases) for (int i = 0; i < 10; ++i) {
            fd = socket(AF_UNIX, SOCK_STREAM, 0);
            if (fd < 0) return false;
            sockaddr_un addr{};
            addr.sun_family = AF_UNIX;
            auto path = fmt::format("{}/discord-ipc-{}", b, i);
            if (path.size() >= sizeof(addr.sun_path)) { ::close(fd); fd = -1; continue; }
            std::strcpy(addr.sun_path, path.c_str());
            if (connect(fd, (sockaddr*)&addr, sizeof addr) == 0) return true;
            ::close(fd); fd = -1;
        }
        return false;
    }
    bool write(void const* p, size_t n) { return ::write(fd, p, n) == (ssize_t)n; }
    bool read(void* p, size_t n) { size_t got = 0; while (got < n) { auto r = ::read(fd, (char*)p + got, n - got); if (r <= 0) return false; got += (size_t)r; } return true; }
    void close() { if (fd >= 0) ::close(fd); fd = -1; }
    bool ok() const { return fd >= 0; }
#endif

    // a frame is: opcode (u32 little endian), length (u32), JSON text
    bool send(uint32_t op, std::string const& json) {
        std::string f(8, '\0');
        uint32_t len = (uint32_t)json.size();
        std::memcpy(&f[0], &op, 4); std::memcpy(&f[4], &len, 4);
        f += json;
        return write(f.data(), f.size());
    }
    bool recv(uint32_t& op, std::string& json) {
        uint32_t hdr[2];
        if (!read(hdr, 8) || hdr[1] > (1u << 20)) return false;
        op = hdr[0]; json.assign(hdr[1], '\0');
        return hdr[1] == 0 || read(&json[0], hdr[1]);
    }
};

int processId() {
#if defined(GEODE_IS_WINDOWS)
    return (int)GetCurrentProcessId();
#else
    return (int)getpid();
#endif
}

std::string activityJson(std::string const& details, std::string const& state, long long start, bool clear) {
    static int nonce = 0;
    std::string a;
    if (!clear) {
        a = fmt::format(",\"activity\":{{\"details\":\"{}\",\"state\":\"{}\",\"timestamps\":{{\"start\":{}}},"
                        "\"assets\":{{\"large_image\":\"xyzbot\",\"large_text\":\"xyzBot {}\"}}}}",
                        esc(details), esc(state), start, esc(Mod::get()->getVersion().toVString()));
    }
    return fmt::format("{{\"cmd\":\"SET_ACTIVITY\",\"args\":{{\"pid\":{}{}}},\"nonce\":\"{}\"}}", processId(), a, ++nonce);
}

void worker() {
    Pipe pipe;
    std::string sentDetails, sentState;
    bool connected = false, sentClear = true;
    auto lastSend = Clock::now() - std::chrono::seconds(60), lastTry = Clock::now() - std::chrono::seconds(60);
    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        bool run = g_run.load();
        if (!run && !connected) continue;
        auto now = Clock::now();
        if (!connected) {
            if (now - lastTry < std::chrono::seconds(15)) continue;
            lastTry = now;
            auto id = clientId();
            if (id.empty()) { std::lock_guard l(g.mx); g.status = "Set your Discord Application ID in Discord.hpp"; continue; }
            if (!pipe.open()) { std::lock_guard l(g.mx); g.status = "Discord is not running"; continue; }
            uint32_t op = 0; std::string reply;
            if (!pipe.send(0, fmt::format("{{\"v\":1,\"client_id\":\"{}\"}}", esc(id))) || !pipe.recv(op, reply)) { pipe.close(); std::lock_guard l(g.mx); g.status = "Discord did not answer"; continue; }
            connected = true; sentDetails.clear(); sentState.clear(); sentClear = true;
            { std::lock_guard l(g.mx); g.status = "Connected to Discord"; }
        }
        // what to show
        std::string details, state; long long start;
        {
            std::lock_guard l(g.mx);
            bool inLevel = g.haveTick && now - g.lastTick < std::chrono::seconds(5);
            details = inLevel ? g.details : "In the menus";
            state = inLevel ? g.state : "";
            start = g.startEpoch;
        }
        bool changed = details != sentDetails || state != sentState;
        if (!run) { // turned off: remove the presence and let go of the pipe
            if (!sentClear) pipe.send(1, activityJson("", "", 0, true));
            pipe.close(); connected = false; sentClear = true;
            std::lock_guard l(g.mx); g.status = "Off";
            continue;
        }
        if ((changed || sentClear) && now - lastSend >= std::chrono::seconds(5)) { // Discord allows few updates
            if (!pipe.send(1, activityJson(details, state, start, false))) { pipe.close(); connected = false; std::lock_guard l(g.mx); g.status = "Lost the connection to Discord"; continue; }
            uint32_t op = 0; std::string reply;
            if (!pipe.recv(op, reply)) { pipe.close(); connected = false; std::lock_guard l(g.mx); g.status = "Lost the connection to Discord"; continue; }
            sentDetails = details; sentState = state; sentClear = false; lastSend = now;
            std::lock_guard l(g.mx); g.status = "Showing your game on Discord";
        }
    }
}
#endif // XYZ_DISCORD

} // namespace

bool on() { return Mod::get()->getSavedValue<bool>("discord-on", false); }

void setOn(bool v) {
    Mod::get()->setSavedValue("discord-on", v);
#if XYZ_DISCORD
    g_run = v;
    if (v && !g_threadStarted.exchange(true)) std::thread(worker).detach();
#endif
}

std::string status() {
#if XYZ_DISCORD
    std::lock_guard l(g.mx);
    return on() ? g.status : "Off";
#else
    return "Discord Rich Presence is only for PC (Windows and macOS)";
#endif
}

void onTick() {
#if XYZ_DISCORD
    static bool started = false;
    if (!started) { started = true; if (on()) setOn(true); } // saved on state, start it on the first tick
    static int n = 0;
    if (!g_run || ++n % 60 != 0) return;
    auto pl = PlayLayer::get();
    std::string details = "In a level", state;
    if (pl && pl->m_level) {
        std::string name = pl->m_level->m_levelName;
        details = name.empty() ? "In a level" : name;
    }
    auto m = rt().mode;
    state = isRendering() ? "Rendering a video" : m == Mode::Record ? "Recording a macro" : m == Mode::Play ? "Playing a macro" : "Playing";
    std::lock_guard l(g.mx);
    if (!g.haveTick || g.details != details) g.startEpoch = epochNow();
    g.details = details; g.state = state;
    g.lastTick = Clock::now(); g.haveTick = true;
#endif
}

} // namespace xyz::discord
