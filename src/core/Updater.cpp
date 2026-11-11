#include "Updater.hpp"
#include "State.hpp"
#include <Geode/Geode.hpp>
#include <Geode/utils/web.hpp>
#include <Geode/utils/async.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <array>
#include <cctype>
#include <ctime>
#include <fstream>
#if __has_include(<Geode/utils/game.hpp>)
#include <Geode/utils/game.hpp>
#define XYZ_HAS_RESTART 1
#endif

using namespace geode::prelude;

namespace xyz::updater {
namespace {
constexpr char const* API = "https://api.github.com/repos/malikisherenot/xyzBot/releases/latest";

State g_state = State::Idle;
bool g_auto = false;
std::string g_msg, g_latest, g_assetUrl, g_sumUrl, g_digest;
std::vector<std::string> g_notes;
async::TaskHolder<web::WebResponse> g_check, g_dl, g_sum;
std::vector<uint8_t> g_bytes;

web::WebRequest req(int seconds) {
    auto r = web::WebRequest();
    r.timeout(std::chrono::seconds(seconds));
    r.userAgent(fmt::format("xyzBot/{}", Mod::get()->getVersion().toVString()));
    return r;
}

void fail(std::string why) {
    g_state = State::Failed;
    g_msg = std::move(why);
}

std::array<int, 3> parseVersion(std::string s) {
    std::array<int, 3> v{0, 0, 0};
    if (!s.empty() && (s[0] == 'v' || s[0] == 'V')) s.erase(0, 1);
    size_t at = 0;
    for (int i = 0; i < 3 && at < s.size(); ++i) {
        int n = 0;
        while (at < s.size() && s[at] >= '0' && s[at] <= '9') n = n * 10 + (s[at++] - '0');
        v[i] = n;
        if (at < s.size() && s[at] == '.') ++at; else break;
    }
    return v;
}

std::string sha256Hex(std::vector<uint8_t> const& data) {
    static const uint32_t K[64] = {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
        0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
        0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
    uint32_t h[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    std::vector<uint8_t> m = data;
    uint64_t bits = (uint64_t)data.size() * 8;
    m.push_back(0x80);
    while (m.size() % 64 != 56) m.push_back(0);
    for (int i = 7; i >= 0; --i) m.push_back((uint8_t)(bits >> (i * 8)));
    auto rotr = [](uint32_t x, int n) { return (x >> n) | (x << (32 - n)); };
    for (size_t off = 0; off < m.size(); off += 64) {
        uint32_t w[64];
        for (int i = 0; i < 16; ++i) w[i] = (uint32_t)m[off + i * 4] << 24 | (uint32_t)m[off + i * 4 + 1] << 16 | (uint32_t)m[off + i * 4 + 2] << 8 | m[off + i * 4 + 3];
        for (int i = 16; i < 64; ++i) {
            uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
            uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }
        uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
        for (int i = 0; i < 64; ++i) {
            uint32_t S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25), ch = (e & f) ^ (~e & g);
            uint32_t t1 = hh + S1 + ch + K[i] + w[i];
            uint32_t S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22), mj = (a & b) ^ (a & c) ^ (b & c);
            uint32_t t2 = S0 + mj;
            hh = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
    }
    return fmt::format("{:08x}{:08x}{:08x}{:08x}{:08x}{:08x}{:08x}{:08x}", h[0], h[1], h[2], h[3], h[4], h[5], h[6], h[7]);
}

std::vector<std::string> readNotes(std::string const& body) {
    std::vector<std::string> out;
    size_t at = 0;
    while (at <= body.size() && out.size() < 80) {
        size_t end = body.find('\n', at);
        if (end == std::string::npos) end = body.size();
        std::string line = body.substr(at, end - at);
        at = end + 1;
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
        while (!line.empty() && line.front() == '#') line.erase(0, 1);
        while (!line.empty() && line.front() == ' ') line.erase(0, 1);
        if (line.empty()) continue;
        while (line.size() > 64) {
            size_t cut = line.rfind(' ', 64);
            if (cut == std::string::npos || cut < 20) cut = 64;
            out.push_back(line.substr(0, cut));
            line.erase(0, cut);
            while (!line.empty() && line.front() == ' ') line.erase(0, 1);
        }
        out.push_back(line);
        if (end >= body.size()) break;
    }
    return out;
}

void install() {
    auto pkg = Mod::get()->getPackagePath();
    auto fresh = pkg; fresh += ".update";
    auto backup = pkg; backup += ".bak";
    {
        std::ofstream f(fresh, std::ios::binary);
        if (!f || !f.write(reinterpret_cast<char const*>(g_bytes.data()), (std::streamsize)g_bytes.size())) { fail("Could not write the update next to the mod file"); return; }
    }
    std::error_code ec;
    std::filesystem::remove(backup, ec);
    std::filesystem::rename(pkg, backup, ec);
    if (ec) { fail("Could not replace the mod file (" + ec.message() + "). The download is saved as " + geode::utils::string::pathToString(fresh.filename())); return; }
    std::filesystem::rename(fresh, pkg, ec);
    if (ec) {
        std::error_code undo;
        std::filesystem::rename(backup, pkg, undo);
        fail("Could not place the new mod file (" + ec.message() + ")");
        return;
    }
    g_bytes.clear(); g_bytes.shrink_to_fit();
    g_state = State::Done;
    g_msg = "Updated to " + g_latest + ". Restart the game to use it.";
}

bool isHex64(std::string const& s) {
    if (s.size() != 64) return false;
    for (char c : s) if (!std::isxdigit((unsigned char)c)) return false;
    return true;
}

void verifyAndInstall() {
    if (!g_digest.empty()) {
        if (sha256Hex(g_bytes) != g_digest) { fail("The download does not match its checksum, nothing was installed"); return; }
        install();
        return;
    }
    if (g_sumUrl.empty()) { install(); return; }
    g_sum.spawn(req(20).get(g_sumUrl), [](web::WebResponse r) {
        if (!r.ok()) { fail("Could not download the checksum file"); return; }
        auto text = std::move(r).string().unwrapOrDefault();
        std::string want;
        for (char c : text) {
            if (std::isxdigit((unsigned char)c)) want += (char)std::tolower((unsigned char)c);
            else break;
        }
        if (want.size() != 64) { fail("The checksum file is not valid"); return; }
        if (sha256Hex(g_bytes) != want) { fail("The download does not match its checksum, nothing was installed"); return; }
        install();
    });
}
}

State state() { return g_state; }
std::string const& message() { return g_msg; }
std::string const& latest() { return g_latest; }
std::vector<std::string> const& notes() { return g_notes; }

void check(bool manual) {
    if (g_state == State::Checking || g_state == State::Downloading) return;
    g_state = State::Checking;
    g_msg = "Checking for updates...";
    g_check.spawn(req(15).header("Accept", "application/vnd.github+json").get(API), [manual](web::WebResponse r) {
        if (!r.ok()) { fail(r.code() <= 0 ? "Could not reach GitHub" : fmt::format("GitHub answered with error {}", r.code())); return; }
        auto json = r.json().unwrapOrDefault();
        auto tag = json["tag_name"].asString().unwrapOrDefault();
        if (tag.empty()) { fail("GitHub sent an unexpected answer"); return; }
        g_latest = tag;
        g_notes = readNotes(json["body"].asString().unwrapOrDefault());
        g_assetUrl.clear(); g_sumUrl.clear(); g_digest.clear();
        std::string asset;
        for (auto const& a : json["assets"]) {
            auto name = a["name"].asString().unwrapOrDefault();
            auto url = a["browser_download_url"].asString().unwrapOrDefault();
            if (name.size() > 6 && name.compare(name.size() - 6, 6, ".geode") == 0) {
                g_assetUrl = url; asset = name;
                auto dg = a["digest"].asString().unwrapOrDefault();
                if (dg.rfind("sha256:", 0) == 0) dg.erase(0, 7);
                for (auto& ch : dg) ch = (char)std::tolower((unsigned char)ch);
                g_digest = isHex64(dg) ? dg : std::string();
            }
        }
        for (auto const& a : json["assets"]) {
            auto name = a["name"].asString().unwrapOrDefault();
            if (!asset.empty() && name == asset + ".sha256") g_sumUrl = a["browser_download_url"].asString().unwrapOrDefault();
        }
        auto mine = parseVersion(Mod::get()->getVersion().toVString()), theirs = parseVersion(tag);
        if (!(mine < theirs)) { g_state = State::UpToDate; g_msg = "You are up to date (" + Mod::get()->getVersion().toVString() + ")."; return; }
        g_state = State::Available;
        g_msg = "xyzBot " + tag + " is available.";
        if (!manual && g_auto && cfg().updateNotify) Notification::create("New release " + tag + "!", NotificationIcon::Info, 4.f)->show();
    });
}

void download() {
    if (g_state != State::Available) return;
    if (g_assetUrl.empty()) { fail("This release has no .geode file. Use Open Releases."); return; }
    g_state = State::Downloading;
    g_msg = "Downloading " + g_latest + "...";
    g_dl.spawn(req(180).get(g_assetUrl), [](web::WebResponse r) {
        if (!r.ok()) { fail(r.code() <= 0 ? "The download failed" : fmt::format("The download failed (error {})", r.code())); return; }
        g_bytes = std::move(r).data();
        if (g_bytes.size() < 4096 || g_bytes[0] != 'P' || g_bytes[1] != 'K') { fail("The downloaded file is not a valid mod"); return; }
        g_msg = "Verifying...";
        verifyAndInstall();
    });
}

void autoCheck() {
    if (!cfg().autoUpdate) return;
    g_auto = true;
    check(false);
}

void restart() {
#ifdef XYZ_HAS_RESTART
    geode::utils::game::restart();
#else
    Notification::create("Please restart the game manually", NotificationIcon::Info)->show();
#endif
}
}

class $modify(XyzUpdateMenu, MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) return false;
        static bool done = false;
        if (!done) { done = true; xyz::updater::autoCheck(); }
        return true;
    }
};
