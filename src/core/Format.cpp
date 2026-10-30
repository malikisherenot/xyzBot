// .xyz v2: delta + varint columns, linear-predicted float residuals, then LZ77. Lossless.
#include "State.hpp"
#include <cstring>
#include <fstream>

using namespace geode::prelude;

namespace xyz {
namespace {
using Buf = std::vector<uint8_t>;

void putV(Buf& b, uint64_t v) { while (v >= 0x80) { b.push_back((uint8_t)(v | 0x80)); v >>= 7; } b.push_back((uint8_t)v); }
uint64_t zz(int64_t v) { return ((uint64_t)v << 1) ^ (uint64_t)(v >> 63); }
int64_t unzz(uint64_t v) { return (int64_t)(v >> 1) ^ -(int64_t)(v & 1); }

struct Rd {
    uint8_t const* p; uint8_t const* e; bool ok = true;
    uint64_t v() {
        uint64_t r = 0; int s = 0;
        while (p < e) { uint8_t c = *p++; r |= (uint64_t)(c & 0x7f) << s; if (!(c & 0x80)) return r; s += 7; if (s > 63) break; }
        ok = false; return 0;
    }
    uint8_t b() { if (p >= e) { ok = false; return 0; } return *p++; }
};

uint32_t hash4(Buf const& in, size_t i) {
    uint32_t x = in[i] | in[i + 1] << 8 | in[i + 2] << 16 | (uint32_t)in[i + 3] << 24;
    return (x * 2654435761u) >> 16;
}

Buf lz(Buf const& in) {
    Buf out; size_t n = in.size(), i = 0, lit = 0;
    std::vector<int32_t> head(1 << 16, -1);
    auto flush = [&](size_t to, size_t ml, size_t off) {
        putV(out, to - lit); out.insert(out.end(), in.begin() + lit, in.begin() + to);
        putV(out, ml); if (ml) putV(out, off);
    };
    while (i + 4 <= n) {
        auto h = hash4(in, i); int32_t c = head[h]; head[h] = (int32_t)i;
        if (c >= 0) {
            size_t l = 0;
            while (i + l < n && in[(size_t)c + l] == in[i + l] && l < 1 << 20) ++l;
            if (l >= 4) {
                flush(i, l, i - (size_t)c);
                for (size_t k = 1; k < l && i + k + 4 <= n; ++k) head[hash4(in, i + k)] = (int32_t)(i + k);
                i += l; lit = i; continue;
            }
        }
        ++i;
    }
    flush(n, 0, 0);
    return out;
}

bool unlz(uint8_t const* p, uint8_t const* e, size_t raw, Buf& out) {
    Rd r{p, e}; out.reserve(raw);
    while (true) {
        uint64_t ll = r.v();
        if (!r.ok || ll > (uint64_t)(r.e - r.p) || out.size() + ll > raw) return false;
        out.insert(out.end(), r.p, r.p + ll); r.p += ll;
        uint64_t ml = r.v(); if (!r.ok) return false;
        if (ml == 0) break;
        uint64_t off = r.v();
        if (!r.ok || off == 0 || off > out.size() || out.size() + ml > raw) return false;
        size_t s = out.size() - off;
        for (uint64_t k = 0; k < ml; ++k) out.push_back(out[s + k]);
    }
    return out.size() == raw;
}

int32_t bitsOf(float f) { int32_t i; std::memcpy(&i, &f, 4); return i; }
float floatOf(int32_t i) { float f; std::memcpy(&f, &i, 4); return f; }

struct Hist { int32_t a[4][2] = {}; };  // [channel][prev1, prev2]
int32_t predict(Hist const& h, int c) { return (int32_t)((uint32_t)h.a[c][0] * 2u - (uint32_t)h.a[c][1]); }
void push(Hist& h, int c, int32_t v) { h.a[c][1] = h.a[c][0]; h.a[c][0] = v; }

Buf encode(Macro const& m) {
    Buf b;
    putV(b, m.tps); putV(b, m.ticks); putV(b, m.seed); putV(b, zz(m.levelId));
    putV(b, m.inputs.size()); putV(b, m.fixes.size());
    uint32_t last = 0;
    for (auto& i : m.inputs) { putV(b, i.tick - last); last = i.tick; }
    for (auto& i : m.inputs) b.push_back((uint8_t)(((i.button - 1) & 3) | (i.down << 2) | (i.p2 << 3)));
    last = 0;
    for (auto& f : m.fixes) { putV(b, ((uint64_t)(f.tick - last) << 1) | f.p2); last = f.tick; }
    for (int c = 0; c < 4; ++c) {
        Hist hc[2];
        for (auto& f : m.fixes) {
            float v[4] = {f.x, f.y, f.rot, f.yv};
            int32_t bits = bitsOf(v[c]);
            putV(b, zz((int64_t)bits - (int64_t)predict(hc[f.p2], c)));
            push(hc[f.p2], c, bits);
        }
    }
    return b;
}

bool decode(Buf const& raw, Macro& m) {
    Rd r{raw.data(), raw.data() + raw.size()};
    m = Macro{};
    m.tps = (uint32_t)r.v(); m.ticks = (uint32_t)r.v(); m.seed = r.v(); m.levelId = unzz(r.v());
    uint64_t ni = r.v(), nf = r.v();
    if (!r.ok || ni > raw.size() || nf > raw.size()) return false;
    m.inputs.resize(ni); m.fixes.resize(nf);
    uint32_t last = 0;
    for (auto& i : m.inputs) { last += (uint32_t)r.v(); i.tick = last; }
    for (auto& i : m.inputs) { uint8_t c = r.b(); i.button = (c & 3) + 1; i.down = c & 4; i.p2 = c & 8; }
    last = 0;
    for (auto& f : m.fixes) { uint64_t v = r.v(); last += (uint32_t)(v >> 1); f.tick = last; f.p2 = v & 1; }
    for (int c = 0; c < 4; ++c) {
        Hist hc[2];
        for (auto& f : m.fixes) {
            int32_t bits = (int32_t)((int64_t)predict(hc[f.p2], c) + unzz(r.v()));
            push(hc[f.p2], c, bits);
            float v = floatOf(bits);
            (c == 0 ? f.x : c == 1 ? f.y : c == 2 ? f.rot : f.yv) = v;
        }
    }
    return r.ok;
}

template <class T> bool rd(std::ifstream& f, T& v) { return (bool)f.read(reinterpret_cast<char*>(&v), sizeof(T)); }

bool loadLegacy(std::ifstream& f, Macro& m) {
    uint32_t ver = 0, n = 0;
    m = Macro{};
    if (!rd(f, ver) || ver != 1) return false;
    if (!rd(f, m.tps) || !rd(f, m.ticks) || !rd(f, m.seed) || !rd(f, m.levelId) || !rd(f, n)) return false;
    m.inputs.resize(n);
    for (auto& i : m.inputs) { uint8_t fl; if (!rd(f, i.tick) || !rd(f, i.button) || !rd(f, fl)) return false; i.down = fl & 1; i.p2 = fl & 2; }
    if (!rd(f, n)) return false;
    m.fixes.resize(n);
    for (auto& x : m.fixes) { uint8_t p; if (!rd(f, x.tick) || !rd(f, p) || !rd(f, x.x) || !rd(f, x.y) || !rd(f, x.rot) || !rd(f, x.yv)) return false; x.p2 = p; }
    return true;
}
}

bool saveMacro(Macro const& m, std::string const& name) {
    Buf raw = encode(m);
    Buf packed = lz(raw);
    Buf out = {'X', 'Y', 'Z', '2'};
    putV(out, raw.size());
    out.insert(out.end(), packed.begin(), packed.end());
    std::ofstream f(macroDir() / (name + ".xyz"), std::ios::binary);
    if (!f) return false;
    f.write(reinterpret_cast<char const*>(out.data()), (std::streamsize)out.size());
    return (bool)f;
}

bool loadMacro(Macro& m, std::string const& name) {
    std::ifstream f(macroDir() / (name + ".xyz"), std::ios::binary);
    char magic[4];
    if (!f || !f.read(magic, 4)) return false;
    if (std::memcmp(magic, "XYZM", 4) == 0) return loadLegacy(f, m);
    if (std::memcmp(magic, "XYZ2", 4) != 0) return false;
    Buf rest((std::istreambuf_iterator<char>(f)), {});
    Rd r{rest.data(), rest.data() + rest.size()};
    uint64_t rawSize = r.v();
    if (!r.ok || rawSize > (1ull << 30)) return false;
    Buf raw;
    if (!unlz(r.p, r.e, (size_t)rawSize, raw)) return false;
    return decode(raw, m);
}
}
