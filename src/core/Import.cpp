// Macro importers, based on xdBot, siliconv, slc and Echo's converters (GPL-3.0).
#include "State.hpp"
#include "../third_party/json.hpp"
#include <algorithm>
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

bool importEchoJson(Macro& m, json const& j);

bool importGdr2(Macro& m, std::vector<uint8_t> const& d);

bool importGdr(Macro& m, std::vector<uint8_t> const& data) {
    if (data.size() > 3 && std::memcmp(data.data(), "GDR", 3) == 0) return importGdr2(m, data);
    json j = json::from_msgpack(data, true, false);
    if (j.is_discarded()) j = json::parse(data, nullptr, false);
    if (j.is_discarded() || !j.is_object() || !j.contains("inputs") || !j["inputs"].is_array()) { g_err = "Not a GDR macro"; return false; }
    json const& cj = j;
    {
        auto first = cj["inputs"].empty() ? json() : cj["inputs"][0];
        if (strOr(cj, "format") == "echo" || (first.is_object() && (first.contains("holding") || first.contains("player_2")))) return importEchoJson(m, cj);
    }
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
    m.tps = 240;
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

struct Bytes {
    std::vector<uint8_t> const& d; size_t at = 0; bool fail = false;
    size_t left() const { return fail || at >= d.size() ? 0 : d.size() - at; }
    uint64_t u(size_t w) {
        if (left() < w) { fail = true; return 0; }
        uint64_t v = 0;
        for (size_t i = 0; i < w; ++i) v |= (uint64_t)d[at + i] << (8 * i);
        at += w; return v;
    }
    template <class T> T pod() {
        T v{};
        if (left() < sizeof(T)) { fail = true; return v; }
        std::memcpy(&v, &d[at], sizeof(T)); at += sizeof(T); return v;
    }
    uint64_t leb() {
        uint64_t v = 0;
        for (int sh = 0; sh < 64; sh += 7) { uint64_t b = u(1); if (fail) return 0; v |= (b & 127) << sh; if (!(b & 128)) return v; }
        fail = true; return 0;
    }
};

struct SlcWalk {
    Macro& m;
    uint64_t frame = 0, attempt = 0;
    double tps = 240.0, startTps = 240.0;
    uint64_t seed = 0;
    void press(int btn, bool down, bool p2) {
        uint64_t local = frame - attempt;
        if (local > 0x7fffffffULL) return;
        m.inputs.push_back({(uint32_t)local, (uint8_t)btn, down, p2});
    }
    void restart(uint64_t newSeed, bool hasSeed) {
        m.inputs.clear(); m.fixes.clear(); startTps = tps; attempt = frame + 1;
        if (hasSeed) seed = newSeed;
    }
    void setTps(double v) { if (std::isfinite(v) && v >= 1.0) { tps = v; if (frame <= attempt) startTps = v; } }
    void state3(uint64_t v) {
        frame += v >> 4;
        int btn = (int)((v >> 2) & 3); bool p2 = (v >> 1) & 1;
        if (btn == 0) { press(1, true, p2); press(1, false, p2); }
        else press(btn, v & 1, p2);
    }
};

bool importSlc3(Macro& m, std::vector<uint8_t> const& d) {
    Bytes in{d};
    in.at = 8;
    size_t metaSize = (size_t)in.u(2);
    if (metaSize < 16 || in.left() < metaSize + 1) { g_err = "Corrupt Silicate v3 header"; return false; }
    size_t metaStart = in.at;
    m = Macro{};
    SlcWalk w{m};
    double tps = in.pod<double>();
    w.tps = w.startTps = (std::isfinite(tps) && tps >= 1.0) ? tps : 240.0;
    w.seed = in.u(8);
    in.at = metaStart + metaSize;
    size_t end = d.back() == 0xCC ? d.size() - 1 : d.size();
    std::vector<uint64_t> cluster;
    while (!in.fail && end - in.at >= 12) {
        uint32_t id = (uint32_t)in.u(4);
        uint64_t size = in.u(8) & ((1ULL << 56) - 1);
        if (size > end - in.at) { g_err = "Corrupt Silicate v3 atom"; return false; }
        size_t atomEnd = size == 0 ? end : in.at + (size_t)size;
        if (id == 1) {
            if (atomEnd - in.at < 8) { g_err = "Corrupt Silicate v3 actions"; return false; }
            in.u(8);
            while (!in.fail && atomEnd - in.at >= 2) {
                uint16_t h = (uint16_t)in.u(2);
                switch (h >> 14) {
                case 0: case 1: {
                    size_t width = (size_t)1 << ((h >> 12) & 3), len = (size_t)1 << ((h >> 8) & 15);
                    size_t reps = (h >> 14) == 1 ? (size_t)1 << ((h >> 3) & 31) : 1;
                    if (atomEnd - in.at < width * len) { g_err = "Corrupt Silicate v3 section"; return false; }
                    cluster.assign(len, 0);
                    for (auto& v : cluster) v = in.u(width);
                    if (reps > 1 && len * reps > 50000000ULL) { g_err = "Silicate v3 repeat too large"; return false; }
                    for (size_t r = 0; r < reps; ++r) for (auto v : cluster) w.state3(v);
                    break;
                }
                case 2: {
                    size_t width = (size_t)1 << ((h >> 8) & 3);
                    if (atomEnd - in.at < width) { g_err = "Corrupt Silicate v3 special"; return false; }
                    w.frame += in.u(width);
                    switch ((h >> 10) & 15) {
                    case 3: w.setTps(in.pod<double>()); break;
                    case 0: case 1: case 2: w.restart(in.u(8), true); break;
                    case 4: break;
                    default: g_err = "Unknown Silicate v3 section"; return false;
                    }
                    break;
                }
                default: g_err = "Unknown Silicate v3 section"; return false;
                }
            }
        }
        in.at = atomEnd;
    }
    if (in.fail) { g_err = "Truncated Silicate v3 macro"; return false; }
    m.tps = (uint32_t)std::lround(w.startTps);
    m.seed = w.seed;
    finish(m);
    if (m.inputs.empty()) { g_err = "Macro has no inputs"; return false; }
    return true;
}

bool importSlc2(Macro& m, std::vector<uint8_t> const& d) {
    Bytes in{d};
    in.at = 4;
    m = Macro{};
    SlcWalk w{m};
    double tps = in.pod<double>();
    w.tps = w.startTps = (std::isfinite(tps) && tps >= 1.0) ? tps : 240.0;
    uint64_t metaSize = in.u(8);
    if (metaSize > in.left()) { g_err = "Corrupt Silicate v2 header"; return false; }
    if (metaSize >= 8) { size_t keep = in.at; w.seed = in.u(8); in.at = keep; }
    in.at += (size_t)metaSize;
    uint64_t length = in.u(8), blobCount = in.u(8);
    if (in.fail || blobCount > in.left() / 24 || length > 200000000ULL) { g_err = "Corrupt Silicate v2 header"; return false; }
    struct Blob { uint64_t size, start, len; };
    std::vector<Blob> blobs(blobCount);
    for (auto& b : blobs) { b.size = in.u(8); b.start = in.u(8); b.len = in.u(8); }
    for (auto& b : blobs) {
        if (b.size < 1 || b.size > 8) { g_err = "Corrupt Silicate v2 blob"; return false; }
        for (uint64_t i = 0; i < b.len && !in.fail; ++i) {
            uint64_t st = in.u((size_t)b.size);
            w.frame += st >> 5;
            int type = (int)((st & 0b11100) >> 2); bool p2 = (st >> 1) & 1, hold = st & 1;
            if (type == 7) w.setTps(in.pod<double>());
            else if (type >= 1 && type <= 3) w.press(type, hold, p2);
            else if (type == 4 || type == 5) w.restart(0, false);
        }
    }
    if (in.fail) { g_err = "Truncated Silicate v2 macro"; return false; }
    m.tps = (uint32_t)std::lround(w.startTps);
    m.seed = w.seed;
    finish(m);
    if (m.inputs.empty()) { g_err = "Macro has no inputs"; return false; }
    return true;
}

bool importSlc1(Macro& m, std::vector<uint8_t> const& d) {
    if (d.size() < 12) { g_err = "Not a Silicate macro"; return false; }
    double tps; uint32_t len;
    std::memcpy(&tps, d.data(), 8); std::memcpy(&len, d.data() + 8, 4);
    if (!(tps >= 1.0 && tps < 1e7) || len > (d.size() - 12) / 4) { g_err = "Not a Silicate macro"; return false; }
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

bool importSlc(Macro& m, std::vector<uint8_t> const& d) {
    if (d.size() >= 9 && std::memcmp(d.data(), "SLC3RPLY", 8) == 0) return importSlc3(m, d);
    if (d.size() >= 4 && std::memcmp(d.data(), "SILL", 4) == 0) return importSlc2(m, d);
    return importSlc1(m, d);
}

bool importTcbot(Macro& m, std::vector<uint8_t> const& d) {
    static const uint8_t header[16] = {0x9f, 0x88, 0x89, 0x84, 0x9f, 0x3b, 0x1d, 0xd8, 0xcc, 0xa1, 0x86, 0x8a, 0x88, 0x99, 0x84, 0x00};
    if (d.size() < 16 + 64 || std::memcmp(d.data(), header, 16) != 0) return false;
    uint8_t version = d[16], flags = d[18];
    float rate; std::memcpy(&rate, d.data() + 20, 4);
    float tps = rate;
    if (version == 2 && !(flags & 2) && rate > 0.f) {
        tps = 1.f / rate;
        float whole = std::round(tps);
        if (whole > 0.f && (float)(1.0 / whole) == rate) tps = whole;
    }
    Bytes in{d}; in.at = 16 + 64;
    m = Macro{};
    float startTps = tps;
    auto push = [&](uint64_t frame, int btn, bool down, bool p2) {
        if (frame > 0x7fffffffULL) return;
        m.inputs.push_back({(uint32_t)(frame >= 2 ? frame - 2 : 0), (uint8_t)btn, down, p2});
    };
    bool ok = true;
    if (version == 1) {
        uint64_t amt = in.leb();
        for (uint64_t i = 0; i < amt && !in.fail; ++i) {
            uint64_t frame = 1 + in.leb();
            uint8_t data = (uint8_t)in.u(1);
            if (in.fail) { ok = false; break; }
            int input = data & 7;
            if (input < 3) push(frame, input + 1, data & 0x80, data & 0x40);
            else if (input <= 5) m.inputs.clear();
            else { ok = false; break; }
        }
        ok = ok && !in.fail;
    } else if (version == 2) {
        uint64_t frame = 1 + in.leb(), lastDelta = 0;
        if (in.fail) ok = false;
        uint8_t info = 0; int next = 0;
        while (ok) {
            if (next == 0) {
                if (in.left() == 0) break;
                uint8_t byte = (uint8_t)in.u(1);
                info = byte >> 5; int btn = byte & 3; bool extra = byte & 0x10;
                if (btn > 0) {
                    bool hold = byte & 4, p2 = byte & 8;
                    push(frame, btn, hold, p2);
                    if (extra) push(frame, btn, !hold, p2);
                    next = 1;
                } else {
                    int custom = (byte >> 2) & 3;
                    if (custom == 3) next = extra ? 1 : 2;
                    else { m.inputs.clear(); frame = 0; next = extra ? 3 : 1; }
                }
            } else if (next == 1) {
                bool magic = info & 1; int blob = info >> 1;
                uint64_t base = magic ? lastDelta : 0, res = base;
                if (blob != 0) { res = base + in.u(blob == 3 ? 4 : (size_t)blob); if (res != 0) lastDelta = res; }
                if (in.fail) break;
                frame += res; next = 0;
            } else if (next == 2) {
                float v = in.pod<float>();
                if (in.fail) break;
                if (std::isfinite(v) && v >= 1.f && frame <= 2) startTps = v;
                next = 1;
            } else { in.u(8); if (in.fail) break; next = 1; }
        }
    } else ok = false;
    if (!ok) { g_err = "Unsupported or corrupt TCBot macro"; return false; }
    m.tps = (uint32_t)std::max(1L, std::lround(startTps));
    finish(m);
    if (m.inputs.empty()) { g_err = "Macro has no inputs"; return false; }
    return true;
}

bool parseLegacyEcho(Macro& m, std::vector<uint8_t> const& d, int strBytes, bool withMeta) {
    if (d.size() < (size_t)strBytes + 8) return false;
    uint64_t len, cap;
    if (strBytes == 32) { std::memcpy(&len, d.data() + 16, 8); std::memcpy(&cap, d.data() + 24, 8); }
    else { uint32_t a, b; std::memcpy(&a, d.data() + 16, 4); std::memcpy(&b, d.data() + 20, 4); len = a; cap = b; }
    if (len < 4 || len > 7 || cap < len || cap > 15) return false;
    std::string name((char const*)d.data(), (size_t)len);
    if (name != "SIMPLE" && name != "DEBUG" && name != "META" && name != "METADBG") return false;
    Bytes in{d}; in.at = (size_t)strBytes;
    float fps = in.pod<float>(); float endX = in.pod<float>(); (void)endX;
    if (withMeta) { in.pod<double>(); in.pod<int32_t>(); int32_t att = in.pod<int32_t>(); if (att < 0 || att > 100000000) return false; }
    if (in.fail || !std::isfinite(fps) || fps < 1.f || fps > 100000.f) return false;
    bool debug = name == "DEBUG" || name == "METADBG";
    Macro out;
    out.tps = (uint32_t)std::lround(fps);
    int32_t prev = -1;
    while (in.at < d.size()) {
        if (in.left() < 6) return false;
        int32_t number = in.pod<int32_t>(); uint8_t hold = in.pod<uint8_t>(), p2 = in.pod<uint8_t>();
        if (hold > 1 || p2 > 1 || number < 0 || number > 100000000 || prev > number) return false;
        prev = number;
        out.inputs.push_back({(uint32_t)number, 1, hold != 0, p2 != 0});
        if (debug) {
            float x = in.pod<float>(); double yv = in.pod<double>(); double xv = in.pod<double>(); float y = in.pod<float>(); float rot = in.pod<float>();
            (void)xv;
            if (in.fail || !std::isfinite(x) || !std::isfinite(y) || !std::isfinite(rot) || !std::isfinite(yv)) return false;
            if (number >= 1 && (x != 0.f || y != 0.f)) out.fixes.push_back({(uint32_t)(number - 1), p2 != 0, x, y, rot, (float)yv});
        }
    }
    if (in.fail) return false;
    m = std::move(out);
    return true;
}
bool importLegacyEcho(Macro& m, std::vector<uint8_t> const& d) {
    for (int w : {32, 24}) for (bool meta : {true, false}) {
        if (parseLegacyEcho(m, d, w, meta)) { finish(m); if (!m.inputs.empty()) return true; }
    }
    g_err = "Unsupported .echo: only the old SIMPLE/DEBUG/META .echo is readable, export new ones from Echo as .echo.json, .gdr or .slc";
    return false;
}

bool importEchoJson(Macro& m, json const& j) {
    bool cur = strOr(j, "format") == "echo";
    m = Macro{};
    m.tps = (uint32_t)std::max(1L, std::lround(numOr(j, "fps", 240.0)));
    bool debug = boolOr(j, "debug", false);
    for (auto& i : j["inputs"]) {
        if (!i.is_object()) continue;
        int btn = 1;
        if (auto it = i.find("button"); it != i.end()) {
            if (it->is_string()) { auto t = it->get<std::string>(); btn = t == "left" ? 2 : t == "right" ? 3 : 1; }
            else if (it->is_number()) btn = std::clamp((int)it->get<double>(), 1, 3);
        }
        int64_t frame = std::max<int64_t>(0, (int64_t)numOr(i, "frame", 0));
        bool p2, down;
        if (cur) { p2 = numOr(i, "player", 1) == 2; down = boolOr(i, "down", false); }
        else { p2 = boolOr(i, "player_2", false); down = boolOr(i, "holding", false); }
        m.inputs.push_back({(uint32_t)frame, (uint8_t)btn, down, p2});
        char const* kx = cur ? "x" : "x_position"; char const* ky = cur ? "y" : "y_position"; char const* kv = cur ? "yVelocity" : "y_vel";
        if ((debug || i.contains(kx)) && frame >= 1) {
            float x = (float)numOr(i, kx, 0), y = (float)numOr(i, ky, 0);
            if (x != 0.f || y != 0.f) m.fixes.push_back({(uint32_t)(frame - 1), p2, x, y, (float)numOr(i, "rotation", 0), (float)numOr(i, kv, 0)});
        }
    }
    if (auto it = j.find("seed"); it != j.end()) {
        if (it->is_string()) m.seed = std::strtoull(it->get<std::string>().c_str(), nullptr, 10);
        else if (it->is_number_unsigned()) m.seed = it->get<uint64_t>();
    }
    finish(m);
    if (m.inputs.empty()) { g_err = "Macro has no inputs"; return false; }
    return true;
}

// GDR2 (binary, maxnut's GDReplayFormat v2). Numbers are LEB128, floats and doubles are big endian.
bool importGdr2(Macro& m, std::vector<uint8_t> const& d) {
    Bytes in{d};
    in.at = 3;
    auto str = [&]() {
        uint64_t n = in.leb();
        if (in.fail || n > 0xFFFF || in.left() < n) { in.fail = true; return std::string(); }
        std::string s((char const*)&d[in.at], (size_t)n);
        in.at += (size_t)n;
        return s;
    };
    auto f32 = [&]() { uint32_t v = (uint32_t)in.u(4); v = __builtin_bswap32(v); float f; std::memcpy(&f, &v, 4); return f; };
    auto f64 = [&]() { uint64_t v = in.u(8); v = __builtin_bswap64(v); double f; std::memcpy(&f, &v, 8); return f; };
    in.leb();
    std::string inputTag = str();
    str(); str();
    f32();
    in.leb();
    double fps = f64();
    uint64_t seed = in.leb();
    in.leb();
    in.leb();
    bool platformer = in.leb() != 0;
    std::string bot = str();
    in.leb();
    uint64_t levelId = in.leb();
    str();
    uint64_t extSize = in.leb();
    if (in.fail || extSize > in.left()) { g_err = "Corrupt GDR2 header"; return false; }
    size_t extEnd = in.at + (size_t)extSize;

    m = Macro{};
    m.tps = (uint32_t)std::max(1L, std::lround(std::isfinite(fps) ? fps : 240.0));
    m.seed = seed;
    m.levelId = (int64_t)levelId;

    if (bot == "xdBot") {
        Bytes ex{d}; ex.at = in.at;
        auto rd = [&]() { uint32_t v = (uint32_t)ex.u(4); v = __builtin_bswap32(v); float f; std::memcpy(&f, &v, 4); return f; };
        uint64_t count = ex.leb();
        for (uint64_t i = 0; i < count && !ex.fail && ex.at <= extEnd; ++i) {
            uint64_t frame = ex.leb();
            float x1 = rd(), y1 = rd(), r1 = rd(); bool rot1 = ex.leb() != 0;
            float x2 = rd(), y2 = rd(), r2 = rd(); bool rot2 = ex.leb() != 0;
            if (ex.fail || frame > 0x7fffffffULL) break;
            addFix(m, (int)frame, false, x1, y1, rot1 && r1 != 0.f ? r1 : NOVAL);
            addFix(m, (int)frame, true, x2, y2, rot2 && r2 != 0.f ? r2 : NOVAL);
        }
    }
    in.at = extEnd;

    uint64_t deaths = in.leb();
    for (uint64_t i = 0; i < deaths && !in.fail; ++i) in.leb();
    in.leb();
    uint64_t p1 = in.leb();
    if (in.fail) { g_err = "Corrupt GDR2 header"; return false; }

    bool hasExt = !inputTag.empty();
    uint64_t prev = 0;
    bool p2 = p1 == 0;
    while (in.left() > 0 && !in.fail) {
        uint64_t packed = in.leb();
        uint64_t frame; int btn; bool down = packed & 1;
        if (platformer) { frame = (packed >> 3) + prev; btn = (int)((packed >> 1) & 3); }
        else { frame = (packed >> 1) + prev; btn = 1; }
        if (hasExt) {
            uint64_t sz = in.leb();
            if (in.fail || sz > in.left()) { g_err = "Corrupt GDR2 input"; return false; }
            if (inputTag == "Phys" && sz >= 28) {
                Bytes ex{d}; ex.at = in.at;
                auto rd = [&]() { uint32_t v = (uint32_t)ex.u(4); v = __builtin_bswap32(v); float f; std::memcpy(&f, &v, 4); return f; };
                auto rdd = [&]() { uint64_t v = ex.u(8); v = __builtin_bswap64(v); double f; std::memcpy(&f, &v, 8); return f; };
                float x = rd(), y = rd(), rot = rd(); rdd(); double yv = rdd();
                if (frame >= 1 && frame < 0x7fffffffULL && (x != 0.f || y != 0.f)) m.fixes.push_back({(uint32_t)(frame - 1), p2, x, y, rot, (float)yv});
            }
            in.at += (size_t)sz;
        }
        if (btn >= 1 && btn <= 3 && frame < 0x7fffffffULL) m.inputs.push_back({(uint32_t)frame, (uint8_t)btn, down, p2});
        prev = frame;
        if (p1 > 0 && --p1 == 0) { prev = 0; p2 = true; }
    }
    if (in.fail) { g_err = "Truncated GDR2 macro"; return false; }
    finish(m);
    if (m.inputs.empty()) { g_err = "Macro has no inputs"; return false; }
    return true;
}

bool importEchoPlain(Macro& m, std::string const& s) {
    std::istringstream in(s);
    float fps = 0.f;
    if (!(in >> fps) || fps < 1.f) return false;
    m = Macro{};
    m.tps = (uint32_t)std::lround(fps);
    long f; int h, p;
    while (in >> f >> h >> p) if (f >= 0) m.inputs.push_back({(uint32_t)f, 1, h != 0, p != 0});
    finish(m);
    return !m.inputs.empty();
}

bool importText(Macro& m, std::string const& s) {
    {
        std::istringstream first(s); std::string line;
        while (std::getline(first, line)) {
            if (line.empty() || line == "\r") continue;
            char* e = nullptr; std::strtod(line.c_str(), &e);
            bool lone = e != line.c_str() && std::string(e).find_first_not_of(" \t\r") == std::string::npos;
            if (lone && importEchoPlain(m, s)) return true;
            break;
        }
    }
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

// .OMEGA v1 (Obot): "OMEGA", version, u16 TPS, varint count, then per input a varint frame delta and a flag byte.
bool importOmega(Macro& m, std::vector<uint8_t> const& d) {
    if (d.size() < 8 || std::memcmp(d.data(), "OMEGA", 5) != 0) { g_err = "Not an OMEGA macro"; return false; }
    if (d[5] != 1) { g_err = "Unsupported OMEGA version"; return false; }
    auto parse = [&](bool interleaved, Macro& out) {
        Bytes in{d}; in.at = 6;
        uint32_t tps = (uint32_t)in.u(2);
        uint64_t count = in.leb();
        if (in.fail || count > in.left() / 2) return false;
        std::vector<uint64_t> deltas;
        std::vector<uint8_t> flags;
        deltas.reserve((size_t)count); flags.reserve((size_t)count);
        if (interleaved) {
            for (uint64_t i = 0; i < count; ++i) { deltas.push_back(in.leb()); flags.push_back((uint8_t)in.u(1)); }
        } else {
            for (uint64_t i = 0; i < count; ++i) deltas.push_back(in.leb());
            for (uint64_t i = 0; i < count; ++i) flags.push_back((uint8_t)in.u(1));
        }
        if (in.fail || in.at != d.size()) return false;
        out = Macro{};
        out.tps = tps ? tps : 240;
        uint64_t frame = 0;
        for (size_t i = 0; i < deltas.size(); ++i) {
            frame += deltas[i];
            if (frame > 0x7fffffffULL) return false;
            uint8_t fl = flags[i];
            int btn = (fl >> 3) & 3;
            if (btn == 0) btn = 1;
            out.inputs.push_back({(uint32_t)frame, (uint8_t)btn, (fl & 1) != 0, (fl & 2) != 0});
        }
        return true;
    };
    Macro out;
    if (!parse(true, out) && !parse(false, out)) { g_err = "Corrupt OMEGA macro"; return false; }
    m = std::move(out);
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
    for (char const* e : {".gdr", ".gdr.json", ".gdr2", ".json", ".xd", ".slc", ".tcm", ".echo", ".omega", ".txt", ".csv"}) if (endsWith(n, e)) return true;
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
        if (endsWith(n, ".tcm")) return importTcbot(m, data) || (g_err = g_err.empty() ? "Not a TCBot macro" : g_err, false);
        if (endsWith(n, ".echo") && !endsWith(n, ".echo.json")) return importLegacyEcho(m, data);
        if (endsWith(n, ".omega")) return importOmega(m, data);
        if (endsWith(n, ".gdr2")) return importGdr2(m, data);
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
