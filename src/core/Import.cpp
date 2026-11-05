// Macro importers. Reads many formats into the native Macro:
//  - .gdr / .gdr.json / .json  GDR1 (msgpack or json), including xdBot macros with their frame fixes
//  - .xd                       legacy xdBot text macros (inputs + position fixes), logic from xdBot (GPL-3.0)
//  - .slc                      Silicate v1 (layout from siliconv, GPL-3.0); v2/v3 need slc_oxide, not included
//  - .txt / .csv               "tick button down [p2]" lines, optional "tps=240" line
// Imported frame fixes carry only what the source stored: missing rotation / y velocity are NaN and are not applied.
#include "State.hpp"
#include "../third_party/json.hpp"
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>
#include <sstream>

using namespace geode::prelude;

namespace xyz {
namespace {
using json = nlohmann::json;
std::string g_err;
constexpr float NOVAL = std::numeric_limits<float>::quiet_NaN();

double numOr(json const& j, char const* k, double d) {
    if (!j.is_object()) return d;
    auto it = j.find(k);
    return (it != j.end() && it->is_number()) ? it->get<double>() : d;
}
bool boolOr(json const& j, char const* k, bool d) {
    if (!j.is_object()) return d;
    auto it = j.find(k);
    return (it != j.end() && it->is_boolean()) ? it->get<bool>() : d;
}
std::string strOr(json const& j, char const* k) {
    if (!j.is_object()) return "";
    auto it = j.find(k);
    return (it != j.end() && it->is_string()) ? it->get<std::string>() : "";
}
std::vector<std::string> splitBy(std::string const& s, char sep) {
    std::vector<std::string> out; std::string cur;
    for (char ch : s) { if (ch == sep) { out.push_back(cur); cur.clear(); } else cur += ch; }
    out.push_back(cur);
    return out;
}
float toF(std::string const& s, float d = 0.f) { char* e = nullptr; float v = std::strtof(s.c_str(), &e); return e == s.c_str() ? d : v; }

// xdBot before 2.3.6 stored frames one lower than now
int legacyOffset(std::string const& bot, std::string ver) {
    if (bot != "xdBot") return 0;
    if (!ver.empty() && ver.front() == 'v') ver.erase(0, 1);
    auto parts = splitBy(ver, '.');
    if (parts.size() != 3) return 1;
    auto n = [](std::string const& s) { int v = 0; if (s.empty()) return 0; for (char c : s) { if (c < '0' || c > '9') return 0; v = v * 10 + (c - '0'); } return v; };
    int a = n(parts[0]), b = n(parts[1]), c = n(parts[2]);
    return (a > 2 || (a == 2 && (b > 3 || (b == 3 && c >= 6)))) ? 0 : 1;
}
bool usesRotation(std::string const& bot, std::string const& ver) {
    if (bot != "xdBot") return false;
    if (ver == "v2.0.0") return true;
    return ver.find("beta.") == std::string::npos && ver.find("alpha.") == std::string::npos;
}

// an xdBot fix at frame f is the state right before frame f runs: that is our fix at tick f-1
void addFix(Macro& m, int frame, bool p2, float x, float y, float rot) {
    if (frame < 1 || (x == 0.f && y == 0.f)) return;
    m.fixes.push_back({(uint32_t)(frame - 1), p2, x, y, rot, NOVAL});
}

void finish(Macro& m) {
    std::stable_sort(m.inputs.begin(), m.inputs.end(), [](auto& a, auto& b) { return a.tick < b.tick; });
    std::stable_sort(m.fixes.begin(), m.fixes.end(), [](auto& a, auto& b) { return a.tick < b.tick; });
    m.ticks = 0;
    for (auto& i : m.inputs) m.ticks = std::max(m.ticks, i.tick);
}

bool importGdr(Macro& m, std::vector<uint8_t> const& data) {
    json j = json::from_msgpack(data, true, false);
    if (j.is_discarded()) j = json::parse(data, nullptr, false);
    if (j.is_discarded() || !j.is_object() || !j.contains("inputs") || !j["inputs"].is_array()) { g_err = "Not a GDR macro"; return false; }
    json const& cj = j;
    std::string bot, ver;
    if (auto it = cj.find("bot"); it != cj.end()) { bot = strOr(*it, "name"); ver = strOr(*it, "version"); }
    int offset = legacyOffset(bot, ver);
    bool rot = usesRotation(bot, ver);

    m = Macro{};
    m.tps = (uint32_t)std::max(1L, std::lround(numOr(cj, "framerate", 240.0)));
    for (auto& i : cj["inputs"]) {
        if (!i.is_object() || !i.contains("frame") || !i["frame"].is_number()) continue;
        int btn = (int)numOr(i, "btn", 1);
        if (btn < 1 || btn > 3) continue;
        int64_t t = std::max<int64_t>(0, (int64_t)i["frame"].get<double>() + offset);
        m.inputs.push_back({(uint32_t)t, (uint8_t)btn, boolOr(i, "down", false), boolOr(i, "2p", false)});
    }
    if (auto it = cj.find("frameFixes"); it != cj.end() && it->is_array()) {
        for (auto& f : *it) {
            if (!f.is_object() || !f.contains("frame") || !f["frame"].is_number()) continue;
            int frame = (int)f["frame"].get<double>() + offset;
            if (f.contains("player1") && f["player1"].is_string()) {
                auto a = splitBy(f["player1"].get<std::string>(), ',');
                if (a.size() > 2) addFix(m, frame, false, toF(a[1]), toF(a[2]), NOVAL);
                if (f.contains("player2") && f["player2"].is_string()) {
                    auto b = splitBy(f["player2"].get<std::string>(), ',');
                    if (b.size() > 2) addFix(m, frame, true, toF(b[1]), toF(b[2]), NOVAL);
                }
            } else if (f.contains("player1X")) {
                addFix(m, frame, false, (float)numOr(f, "player1X", 0), (float)numOr(f, "player1Y", 0), NOVAL);
                addFix(m, frame, true, (float)numOr(f, "player2X", 0), (float)numOr(f, "player2Y", 0), NOVAL);
            } else if (f.contains("p1") && f["p1"].is_object()) {
                auto r1 = rot ? (float)numOr(f["p1"], "r", 0) : 0.f;
                addFix(m, frame, false, (float)numOr(f["p1"], "x", 0), (float)numOr(f["p1"], "y", 0), r1 != 0.f ? r1 : NOVAL);
                if (f.contains("p2") && f["p2"].is_object()) {
                    auto r2 = rot ? (float)numOr(f["p2"], "r", 0) : 0.f;
                    addFix(m, frame, true, (float)numOr(f["p2"], "x", 0), (float)numOr(f["p2"], "y", 0), r2 != 0.f ? r2 : NOVAL);
                }
            }
        }
    }
    if (auto it = cj.find("seed"); it != cj.end()) {
        if (it->is_number_unsigned()) m.seed = it->get<uint64_t>();
        else if (it->is_number_integer()) m.seed = (uint64_t)it->get<int64_t>();
    }
    if (auto it = cj.find("level"); it != cj.end() && it->is_object()) m.levelId = (int64_t)numOr(*it, "id", 0);
    finish(m);
    if (m.inputs.empty()) { g_err = "Macro has no inputs"; return false; }
    return true;
}

bool importXD(Macro& m, std::string const& s) {
    m = Macro{};
    m.tps = 240;   // frames are converted to 240 ticks per second
    float mult = 1.f;
    std::istringstream in(s); std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        auto a = splitBy(line, '|');
        if (a.size() < 4) {
            float fps = toF(a[0], 240.f);
            mult = a[0] == "android" ? 4.f : (fps > 0.f ? 240.f / fps : 1.f);
            continue;
        }
        int frame = (int)std::lround(toF(a[0]) * mult);
        int btn = (int)toF(a[2]);
        bool posOnly = a.size() > 4 && a[4] == "1";
        if (!posOnly) {
            if (btn >= 1 && btn <= 3 && frame >= 0) m.inputs.push_back({(uint32_t)frame, (uint8_t)btn, a[1] == "1", a[3] == "1"});
            continue;
        }
        if (a.size() > 12) {
            addFix(m, frame, false, toF(a[5]), toF(a[6]), NOVAL);
            addFix(m, frame, true, toF(a[11]), toF(a[12]), NOVAL);
        }
    }
    finish(m);
    if (m.inputs.empty()) { g_err = "Macro has no inputs"; return false; }
    return true;
}

bool importSlc(Macro& m, std::vector<uint8_t> const& d) {
    if ((d.size() >= 8 && std::memcmp(d.data(), "SLC3RPLY", 8) == 0) || (d.size() >= 4 && std::memcmp(d.data(), "SILL", 4) == 0)) {
        g_err = "Silicate v2/v3 .slc is not supported yet (only v1)";
        return false;
    }
    if (d.size() < 12) { g_err = "Not a Silicate macro"; return false; }
    double tps; uint32_t len;
    std::memcpy(&tps, d.data(), 8); std::memcpy(&len, d.data() + 8, 4);
    if (!(tps >= 1.0 && tps < 1e7) || len > (d.size() - 12) / 4) { g_err = "Not a Silicate v1 macro"; return false; }
    m = Macro{};
    m.tps = (uint32_t)std::lround(tps);
    for (uint32_t k = 0; k < len; ++k) {
        uint32_t st; std::memcpy(&st, d.data() + 12 + 4 * k, 4);
        int btn = (int)((st & 6u) >> 1);
        if (btn < 1 || btn > 3) continue;
        m.inputs.push_back({st >> 4, (uint8_t)btn, (st & 1u) != 0, (st & 8u) != 0});
    }
    size_t at = 12 + 4 * (size_t)len;
    if (d.size() >= at + 8) std::memcpy(&m.seed, d.data() + at, 8);
    finish(m);
    if (m.inputs.empty()) { g_err = "Macro has no inputs"; return false; }
    return true;
}

bool importText(Macro& m, std::string const& s) {
    m = Macro{};
    std::istringstream in(s); std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        if (line.rfind("tps=", 0) == 0) { m.tps = (uint32_t)std::max(1UL, std::stoul(line.substr(4))); continue; }
        std::replace(line.begin(), line.end(), ',', ' ');
        std::istringstream ls(line); uint32_t t; int b, d, p = 0;
        if (!(ls >> t >> b >> d)) continue;
        ls >> p;
        if (b < 1 || b > 3) continue;
        m.inputs.push_back({t, (uint8_t)b, d != 0, p != 0});
    }
    finish(m);
    if (m.inputs.empty()) { g_err = "Macro has no inputs"; return false; }
    return true;
}

bool endsWith(std::string const& s, std::string const& e) { return s.size() >= e.size() && s.compare(s.size() - e.size(), e.size(), e) == 0; }
}

std::string const& importError() { return g_err; }

bool canImport(std::filesystem::path const& file) {
    auto n = file.filename().string();
    std::transform(n.begin(), n.end(), n.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    for (char const* e : {".gdr", ".gdr.json", ".gdr2", ".json", ".xd", ".slc", ".txt", ".csv"}) if (endsWith(n, e)) return true;
    return false;
}

bool importMacro(Macro& m, std::filesystem::path const& file) {
    g_err.clear();
    std::ifstream f(file, std::ios::binary);
    if (!f) { g_err = "Cannot open file"; return false; }
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(f)), {});
    auto n = file.filename().string();
    std::transform(n.begin(), n.end(), n.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    try {
        if (endsWith(n, ".xd")) return importXD(m, std::string(data.begin(), data.end()));
        if (endsWith(n, ".slc")) return importSlc(m, data);
        if (endsWith(n, ".gdr2")) { g_err = ".gdr2 is binary and needs the GDR library: in xdBot save as .gdr or .gdr.json instead"; return false; }
        if (endsWith(n, ".gdr") || endsWith(n, ".json")) return importGdr(m, data);
        if (endsWith(n, ".txt") || endsWith(n, ".csv")) return importText(m, std::string(data.begin(), data.end()));
    } catch (std::exception const& e) {
        g_err = std::string("Import failed: ") + e.what();
        return false;
    }
    g_err = "Unknown macro format";
    return false;
}
}
