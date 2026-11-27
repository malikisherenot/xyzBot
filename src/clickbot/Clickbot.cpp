#include "Clickbot.hpp"
#include "../core/State.hpp"
#include "../render/Encoder.hpp"
#include <Geode/Geode.hpp>
#include <Geode/binding/FMODAudioEngine.hpp>
#include <algorithm>
#include <array>
#include <fstream>
#include <random>
#include <unordered_map>

using namespace geode::prelude;
namespace fs = std::filesystem;

namespace xyz {
bool isRendering();
long long renderFrameCount();
float renderFps();
}

namespace xyz::clickbot {
namespace {

enum Kind { Click, Release, Soft, SoftRelease, Hard, HardRelease, Micro, MicroRelease, NKinds };

struct Pack {
    std::string name;
    std::array<std::vector<fs::path>, NKinds> files[2]; // [player][kind]
    size_t total = 0;
};

Pack g_pack;
std::string g_status = "No clickpack selected";
bool g_loaded = false;
std::mt19937 g_rng{std::random_device{}()};

// timing of the last input of each player, in seconds of game time
double g_lastPress[2] = {-1e9, -1e9}, g_lastRelease[2] = {-1e9, -1e9};
int g_lastPick[2][NKinds] = {{-1, -1, -1, -1, -1, -1, -1, -1}, {-1, -1, -1, -1, -1, -1, -1, -1}};

// rendering
struct Ev { double t; fs::path file; float vol; };
std::vector<Ev> g_events;
long long g_seenFrames = 0, g_frameTick = 0;

constexpr double MICRO = 0.075, SOFT = 0.2, HARD = 1.0; // seconds between inputs that pick the softer or harder sounds

std::string lower(std::string s) {
    std::string o;
    for (char c : s) if (std::isalpha((unsigned char)c) || std::isdigit((unsigned char)c)) o += (char)std::tolower((unsigned char)c);
    return o;
}

bool audioExt(fs::path const& p) {
    auto e = lower(p.extension().string());
    return e == "wav" || e == "mp3" || e == "ogg" || e == "flac";
}

// the kind a folder or file name stands for, -1 if it says nothing
int kindOf(std::string name) {
    auto n = lower(std::move(name));
    bool rel = n.find("release") != std::string::npos;
    bool micro = n.find("micro") != std::string::npos, soft = n.find("soft") != std::string::npos, hard = n.find("hard") != std::string::npos;
    bool click = n.find("click") != std::string::npos || n.find("press") != std::string::npos;
    if (rel) return micro ? MicroRelease : soft ? SoftRelease : hard ? HardRelease : Release;
    if (click || micro || soft || hard) return micro ? Micro : soft ? Soft : hard ? Hard : Click;
    return -1;
}

int playerOf(std::string name) { // 0 / 1 for player1 / player2 folders, -1 shared
    auto n = lower(std::move(name));
    if (n == "player1" || n == "p1") return 0;
    if (n == "player2" || n == "p2") return 1;
    return -1;
}

void scan(fs::path const& root, Pack& pk) {
    std::error_code ec;
    for (auto it = fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied, ec); it != fs::recursive_directory_iterator(); it.increment(ec)) {
        if (ec) break;
        if (!it->is_regular_file(ec) || !audioExt(it->path())) continue;
        auto rel = fs::relative(it->path(), root, ec);
        std::vector<std::string> parts;
        for (const auto& c : rel) parts.push_back(c.string());
        int player = -1, kind = -1;
        bool hold = false;
        for (size_t i = 0; i + 1 < parts.size(); ++i) {
            if (lower(parts[i]).find("hold") != std::string::npos) hold = true;
            if (int pl = playerOf(parts[i]); pl >= 0) player = pl;
        }
        if (hold) continue; // sustained hold loops are not used
        for (size_t i = parts.size() - 1; i-- > 0;) { // deepest folder first, then the file name
            kind = kindOf(parts[i]);
            if (kind >= 0) break;
        }
        if (kind < 0) kind = kindOf(fs::path(parts.back()).stem().string());
        if (kind < 0) kind = Click;
        for (int p = 0; p < 2; ++p)
            if (player < 0 || player == p) pk.files[p][kind].push_back(it->path());
        ++pk.total;
    }
}

bool safeEntry(fs::path const& e) {
    if (e.is_absolute()) return false;
    for (const auto& c : e) if (c == ".." ) return false;
    return true;
}

// extracts the audio files of a zip into the cache, once per version of the zip
std::string extractZip(fs::path const& zip, fs::path const& to) {
    std::error_code ec;
    auto stamp = to / ".stamp";
    std::string want = fmt::format("{}:{}", fs::file_size(zip, ec), fs::last_write_time(zip, ec).time_since_epoch().count());
    {
        std::ifstream f(stamp);
        std::string have;
        if (f && std::getline(f, have) && have == want) return {};
    }
    fs::remove_all(to, ec);
    fs::create_directories(to, ec);
    auto uz = utils::file::Unzip::create(zip);
    if (uz.isErr()) return "cannot open the zip";
    auto& z = uz.unwrap();
    int n = 0;
    for (auto& e : z.getEntries()) {
        if (!audioExt(e) || !safeEntry(e)) continue;
        auto data = z.extract(e);
        if (data.isErr()) continue;
        auto dest = to / e;
        fs::create_directories(dest.parent_path(), ec);
        std::ofstream o(dest, std::ios::binary);
        auto& v = data.unwrap();
        o.write((char const*)v.data(), (std::streamsize)v.size());
        ++n;
    }
    if (n == 0) return "no audio files in the zip";
    std::ofstream(stamp) << want;
    return {};
}

void load() {
    g_loaded = true;
    g_pack = Pack{};
    g_events.clear();
    auto name = current();
    if (name.empty()) { g_status = "No clickpack selected"; return; }
    auto path = dir() / name;
    std::error_code ec;
    if (!fs::exists(path, ec)) { g_status = "Pack not found: " + name; return; }
    fs::path root = path;
    if (fs::is_regular_file(path, ec)) {
        root = persistDir("clickpack-cache") / fs::path(name).stem();
        if (auto e = extractZip(path, root); !e.empty()) { g_status = name + ": " + e; return; }
    }
    g_pack.name = name;
    scan(root, g_pack);
    if (g_pack.total == 0) { g_status = name + ": no sounds found (wav, mp3, ogg, flac)"; return; }
    size_t kinds = 0;
    for (int k = 0; k < NKinds; ++k) if (!g_pack.files[0][k].empty()) ++kinds;
    g_status = fmt::format("{}: {} sounds in {} groups", name, g_pack.total, kinds);
}

void ensureLoaded() { if (!g_loaded) load(); }

// the sound for this input, falling back to softer groups when one is empty
fs::path pick(int p, bool down, double gap, int& kindOut) {
    int k;
    if (down) k = gap < MICRO ? Micro : gap < SOFT ? Soft : gap > HARD ? Hard : Click;
    else k = gap < MICRO ? MicroRelease : gap < SOFT ? SoftRelease : gap > HARD ? HardRelease : Release;
    int chain[4];
    int base = down ? Click : Release;
    int soft = down ? Soft : SoftRelease;
    int n = 0;
    chain[n++] = k;
    if (k == (down ? Micro : MicroRelease)) chain[n++] = soft;
    if (k != base) chain[n++] = base;
    for (int i = 0; i < n; ++i) {
        auto& v = g_pack.files[p][chain[i]];
        if (v.empty()) continue;
        int idx = (int)(g_rng() % v.size());
        if (v.size() > 1 && idx == g_lastPick[p][chain[i]]) idx = (idx + 1) % (int)v.size(); // not the same sound twice in a row
        g_lastPick[p][chain[i]] = idx;
        kindOut = chain[i];
        return v[idx];
    }
    return {};
}

double gameTime() { return (double)rt().tick / std::max(1.f, curTps()); }

Mod* M() { return Mod::get(); }

} // namespace

std::filesystem::path dir() {
    auto d = persistDir("clickpacks");
    std::error_code ec;
    fs::create_directories(d, ec);
    auto readme = d / "README.txt";
    if (!fs::exists(readme, ec)) {
        std::ofstream(readme) << "Put clickpacks here, as folders or .zip files, then pick one in the Clickbot tab.\n"
            "Inside a pack: clicks, releases, softclicks, softreleases, hardclicks, hardreleases, microclicks, microreleases\n"
            "(wav, mp3, ogg or flac). player1 and player2 folders with the same folders inside are optional.\n";
    }
    return d;
}

std::vector<std::string> packs() {
    std::vector<std::string> v;
    std::error_code ec;
    for (auto& e : fs::directory_iterator(dir(), ec)) {
        auto n = e.path().filename().string();
        if (e.is_directory(ec) || (e.is_regular_file(ec) && lower(e.path().extension().string()) == "zip")) v.push_back(n);
    }
    std::sort(v.begin(), v.end());
    return v;
}

std::string current() { return M()->getSavedValue<std::string>("clickbot-pack"); }
void select(std::string const& name) { M()->setSavedValue("clickbot-pack", name); load(); }
void reload() { load(); }
std::string status() { ensureLoaded(); return g_status; }

bool on() { return M()->getSavedValue<bool>("clickbot-on", false); }
void setOn(bool v) { M()->setSavedValue("clickbot-on", v); }
bool liveClicks() { return M()->getSavedValue<bool>("clickbot-live", false); }
void setLiveClicks(bool v) { M()->setSavedValue("clickbot-live", v); }
bool mixRender() { return M()->getSavedValue<bool>("clickbot-render", true); }
void setMixRender(bool v) { M()->setSavedValue("clickbot-render", v); }
int volume() { return std::clamp(M()->getSavedValue<int>("clickbot-vol", 100), 0, 300); }
void setVolume(int p) { M()->setSavedValue("clickbot-vol", std::clamp(p, 0, 300)); }

void onTick() {
    if (!isRendering()) return;
    long long f = renderFrameCount();
    if (f != g_seenFrames) { g_seenFrames = f; g_frameTick = rt().tick; }
}

void renderBegin() {
    g_events.clear();
    g_seenFrames = 0;
    g_frameTick = rt().tick;
}

void onInput(int button, bool down, bool player1, bool real) {
    if (button != 1 || !on() || rt().simulating) return;
    if (real && !liveClicks()) return;
    ensureLoaded();
    if (g_pack.total == 0) return;
    int p = player1 ? 0 : 1;
    double now = gameTime();
    double gap = down ? now - g_lastRelease[p] : now - g_lastPress[p];
    (down ? g_lastPress : g_lastRelease)[p] = now;
    int kind = 0;
    auto file = pick(p, down, gap, kind);
    if (file.empty()) return;
    float vol = (float)volume() / 100.f;
    if (isRendering()) {
        if (mixRender()) {
            double t = (double)renderFrameCount() / std::max(1.f, renderFps()) + (double)((long long)rt().tick - g_frameTick) / std::max(1.f, curTps());
            g_events.push_back({std::max(0.0, t), file, vol});
        }
        return;
    }
    FMODAudioEngine::sharedEngine()->playEffect(utils::string::pathToString(file), 1.f, 0.f, vol);
}

std::vector<float> renderTrack(double videoSeconds) {
    std::vector<float> out;
#if XYZ_LIBAV
    if (g_events.empty()) return out;
    size_t frames = (size_t)(videoSeconds * 44100.0) + 2;
    out.assign(frames * 2, 0.f);
    std::unordered_map<std::string, std::vector<float>> cache;
    for (auto& e : g_events) {
        auto key = utils::string::pathToString(e.file);
        auto it = cache.find(key);
        if (it == cache.end()) {
            std::vector<float> pcm;
            if (auto err = xyz::enc::decodeAudio(e.file, pcm); !err.empty()) log::warn("[clickbot] {}: {}", key, err);
            it = cache.emplace(key, std::move(pcm)).first;
        }
        size_t at = (size_t)(e.t * 44100.0) * 2;
        auto& pcm = it->second;
        for (size_t i = 0; i < pcm.size() && at + i < out.size(); ++i) out[at + i] += pcm[i] * e.vol;
    }
#endif
    g_events.clear();
    return out;
}

} // namespace xyz::clickbot
