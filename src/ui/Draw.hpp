#pragma once
#include <Geode/Geode.hpp>
#include "../core/State.hpp"
#include <vector>
#include <algorithm>

namespace xyz::draw {
constexpr int PAPER_TAG = 0x5A7; // label in the regular font (roboto-regular in mod.json)
constexpr int BOLD_TAG = 0x5A8;  // label in the heavy font (roboto-semibold in mod.json): titles, tabs, section headers

// A missing .fnt makes cocos corrupt the heap and crash the game, so the file is checked before it is used.
inline bool fontExists(std::string const& f) {
    auto fu = cocos2d::CCFileUtils::sharedFileUtils();
    std::string p = fu->fullPathForFilename(f.c_str(), false);
    return !p.empty() && fu->isFileExist(p.c_str());
}
inline std::string const& fontFile() {
    static std::string f = [] { using namespace geode::prelude; return std::string("roboto-regular.fnt"_spr); }();
    return f;
}
inline std::string const& fontBoldFile() {
    static std::string f = [] { using namespace geode::prelude; return std::string("roboto-semibold.fnt"_spr); }();
    return f;
}
inline bool fontsBad() { static bool bad = !fontExists(fontFile()); return bad; }      // no mod font at all: game font
inline bool boldBad() { static bool bad = !fontExists(fontBoldFile()); return bad; }  // no heavy font: regular one
inline std::string fontOf(int = 0) { return fontsBad() ? std::string("bigFont.fnt") : fontFile(); }

inline cocos2d::CCLabelBMFont* mkLabelStyle(char const* text, int = 0) {
    if (!fontsBad()) {
        if (auto l = cocos2d::CCLabelBMFont::create(text, fontFile().c_str())) { l->setTag(PAPER_TAG); return l; }
    }
    return cocos2d::CCLabelBMFont::create(text, "bigFont.fnt");
}
inline cocos2d::CCLabelBMFont* mkLabel(char const* text) { return mkLabelStyle(text); }
inline cocos2d::CCLabelBMFont* mkLabelBold(char const* text) {
    if (!fontsBad() && !boldBad()) {
        if (auto l = cocos2d::CCLabelBMFont::create(text, fontBoldFile().c_str())) { l->setTag(BOLD_TAG); return l; }
    }
    return mkLabelStyle(text);
}

// Size of each font relative to the game's GD Big font, on top of the measured line-height ratio (so the size in
// mod.json does not matter). These are the numbers to change if one font looks too big or too small.
constexpr float kFontSize = 1.05f;
constexpr float kBoldSize = 1.2f;
inline float ratio(std::string const& file, float mul) {
    auto a = cocos2d::CCLabelBMFont::create("Ag", file.c_str());
    auto b = cocos2d::CCLabelBMFont::create("Ag", "bigFont.fnt");
    if (!a || !b) return 1.f;
    float ha = a->getContentSize().height, hb = b->getContentSize().height;
    return (ha > 0.f && hb > 0.f) ? std::clamp(hb / ha * mul, 0.3f, 8.f) : 1.f;
}
inline float fontK() { static float k = fontsBad() ? 1.f : ratio(fontFile(), kFontSize); return k; }
inline float boldK() { static float k = (fontsBad() || boldBad()) ? 1.f : ratio(fontBoldFile(), kBoldSize); return k; }
inline float kOf(cocos2d::CCLabelBMFont* l) {
    if (!l) return 1.f;
    if (l->getTag() == PAPER_TAG) return fontK();
    if (l->getTag() == BOLD_TAG) return boldK();
    return 1.f;
}
inline void sc(cocos2d::CCLabelBMFont* l, float s) { l->setScale(s * kOf(l)); }
inline void fit(cocos2d::CCLabelBMFont* l, float width, float def, float minS) {
    float k = kOf(l), cw = l->getContentSize().width, s = def;
    if (cw > 0.f && cw * def * k > width) s = std::max(minS, width / (cw * k));
    l->setScale(s * k);
}
inline void setStyle(cocos2d::CCLabelBMFont* l, int = 0) {
    l->setFntFile(fontOf().c_str());
    l->setTag(fontsBad() ? 0 : PAPER_TAG);
}
// a game button (ButtonSprite) whose text is switched to the mod's font and kept inside the button
inline ButtonSprite* restyleButton(ButtonSprite* b) {
    if (!b || !b->m_label) return b;
    auto l = b->m_label;
    float old = l->getScale();
    setStyle(l);
    l->setScale(old * kOf(l));
    float maxW = b->getContentSize().width - 16.f, cw = l->getContentSize().width * l->getScale();
    if (cw > maxW && cw > 0.f) l->setScale(l->getScale() * maxW / cw);
    return b;
}

inline cocos2d::ccColor4F pm(cocos2d::ccColor4F c) { return {c.r * c.a, c.g * c.a, c.b * c.a, c.a}; }

inline void poly(cocos2d::CCDrawNode* d, std::vector<cocos2d::CCPoint> const& in, cocos2d::ccColor4F fill, float bw = 0.f, cocos2d::ccColor4F border = {0, 0, 0, 0}) {
    std::vector<cocos2d::CCPoint> q;
    for (auto& p : in)
        if (q.empty() || cocos2d::ccpDistance(q.back(), p) > 0.01f) q.push_back(p);
    while (q.size() > 1 && cocos2d::ccpDistance(q.front(), q.back()) <= 0.01f) q.pop_back();
    if (q.size() < 3) return;
    d->drawPolygon(q.data(), (unsigned)q.size(), pm(fill), bw, pm(border));
}

inline void rrect(cocos2d::CCDrawNode* d, cocos2d::CCPoint o, cocos2d::CCSize s, float r, cocos2d::ccColor4F fill, float bw = 0.f, cocos2d::ccColor4F border = {0, 0, 0, 0}) {
    r = std::min(r, std::min(s.width, s.height) / 2.f - 0.02f);
    std::vector<cocos2d::CCPoint> pts;
    auto arc = [&](cocos2d::CCPoint c, float a0) {
        for (int i = 0; i <= 6; ++i) {
            float a = (a0 + i * 15.f) * 3.14159265f / 180.f;
            pts.push_back({c.x + std::cos(a) * r, c.y + std::sin(a) * r});
        }
    };
    arc({o.x + s.width - r, o.y + s.height - r}, 0);
    arc({o.x + r, o.y + s.height - r}, 90);
    arc({o.x + r, o.y + r}, 180);
    arc({o.x + s.width - r, o.y + r}, 270);
    poly(d, pts, fill, bw, border);
}

inline void disc(cocos2d::CCDrawNode* d, cocos2d::CCPoint c, float r, cocos2d::ccColor4F fill, float bw = 0.f, cocos2d::ccColor4F border = {0, 0, 0, 0}, int seg = 28) {
    std::vector<cocos2d::CCPoint> pts;
    for (int i = 0; i < seg; ++i) {
        float a = i * 6.2831853f / seg;
        pts.push_back({c.x + std::cos(a) * r, c.y + std::sin(a) * r});
    }
    poly(d, pts, fill, bw, border);
}

inline cocos2d::ccColor4F mix(cocos2d::ccColor4F a, cocos2d::ccColor4F b, float t) {
    return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t, a.a + (b.a - a.a) * t};
}
}
