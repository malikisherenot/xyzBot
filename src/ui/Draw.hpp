#pragma once
#include <Geode/Geode.hpp>
#include "../core/State.hpp"
#include <vector>
#include <algorithm>

namespace xyz::draw {
constexpr int ARIAL_TAG = 0x5A7, CHAT_TAG = 0x5A8;
enum class RobotoFace : int { ExtraLight, SemiBold, Bold, ThinItalic };
constexpr int ROBOTO_TAG_BASE = 0x5A9;
inline bool& fontBad() { static bool b = false; return b; }
inline std::string const& fontFile() {
    static std::string f = [] { using namespace geode::prelude; return std::string("arial-italic.fnt"_spr); }();
    return f;
}
inline std::string const& robotoFile(RobotoFace face) {
    static std::string const extraLight = [] { using namespace geode::prelude; return std::string("roboto-extralight.fnt"_spr); }();
    static std::string const semiBold = [] { using namespace geode::prelude; return std::string("roboto-semibold.fnt"_spr); }();
    static std::string const bold = [] { using namespace geode::prelude; return std::string("roboto-bold.fnt"_spr); }();
    static std::string const thinItalic = [] { using namespace geode::prelude; return std::string("roboto-thinitalic.fnt"_spr); }();
    switch (face) {
        case RobotoFace::SemiBold: return semiBold;
        case RobotoFace::Bold: return bold;
        case RobotoFace::ThinItalic: return thinItalic;
        default: return extraLight;
    }
}
inline int robotoTag(RobotoFace face) { return ROBOTO_TAG_BASE + static_cast<int>(face); }
inline cocos2d::CCLabelBMFont* mkRobotoLabel(char const* text, RobotoFace face) {
    if (auto l = cocos2d::CCLabelBMFont::create(text, robotoFile(face).c_str())) { l->setTag(robotoTag(face)); return l; }
    return cocos2d::CCLabelBMFont::create(text, "bigFont.fnt");
}
inline int resolveFontStyle(int style) { return style < 0 ? xyz::menuFontNow() : style; }
inline std::string fontOf(int style) {
    style = resolveFontStyle(style);
    if (style == 2) return "chatFont.fnt";
    if (style == 3) return robotoFile(RobotoFace::ExtraLight);
    return (style != 1 || fontBad()) ? std::string("bigFont.fnt") : fontFile();
}
inline cocos2d::CCLabelBMFont* mkLabelStyle(char const* text, int style) {
    style = resolveFontStyle(style);
    if (style == 3) return mkRobotoLabel(text, RobotoFace::ExtraLight);
    if (style == 2) {
        if (auto l = cocos2d::CCLabelBMFont::create(text, "chatFont.fnt")) { l->setTag(CHAT_TAG); return l; }
    } else if (style == 1 && !fontBad()) {
        if (auto l = cocos2d::CCLabelBMFont::create(text, fontFile().c_str())) { l->setTag(ARIAL_TAG); return l; }
        fontBad() = true;
    }
    return cocos2d::CCLabelBMFont::create(text, "bigFont.fnt");
}
inline cocos2d::CCLabelBMFont* mkLabel(char const* text) { return mkLabelStyle(text, xyz::menuFontNow()); }

inline float fontK() {
    static float k = 0.f;
    if (k > 0.f) return k;
    if (fontBad()) return 1.f;
    auto a = cocos2d::CCLabelBMFont::create("Ag", fontFile().c_str());
    auto b = cocos2d::CCLabelBMFont::create("Ag", "bigFont.fnt");
    if (!a || !b) { if (!a) fontBad() = true; return 1.f; }
    float ha = a->getContentSize().height, hb = b->getContentSize().height;
    k = (ha > 0.f && hb > 0.f) ? std::clamp(hb / ha * 1.25f, 0.5f, 8.f) : 1.f;
    return k;
}
inline float fontKChat() {
    static float k = 0.f;
    if (k > 0.f) return k;
    auto a = cocos2d::CCLabelBMFont::create("Ag", "chatFont.fnt");
    auto b = cocos2d::CCLabelBMFont::create("Ag", "bigFont.fnt");
    if (!a || !b) return 1.f;
    float ha = a->getContentSize().height, hb = b->getContentSize().height;
    k = (ha > 0.f && hb > 0.f) ? std::clamp(hb / ha * 0.8f, 0.3f, 8.f) : 1.f;
    return k;
}
inline float fontKRoboto(RobotoFace face) {
    static float scales[4] = {};
    auto& k = scales[static_cast<int>(face)];
    if (k > 0.f) return k;
    auto a = cocos2d::CCLabelBMFont::create("Ag", robotoFile(face).c_str());
    auto b = cocos2d::CCLabelBMFont::create("Ag", "bigFont.fnt");
    if (!a || !b) { k = 1.f; return k; }
    float ha = a->getContentSize().height, hb = b->getContentSize().height;
    k = (ha > 0.f && hb > 0.f) ? std::clamp(hb / ha, 0.3f, 8.f) : 1.f;
    return k;
}
inline float kOf(cocos2d::CCLabelBMFont* l) {
    if (!l) return 1.f;
    if (l->getTag() == ARIAL_TAG) return fontK();
    if (l->getTag() == CHAT_TAG) return fontKChat();
    int tag = l->getTag();
    if (tag >= ROBOTO_TAG_BASE && tag < ROBOTO_TAG_BASE + 4)
        return fontKRoboto(static_cast<RobotoFace>(tag - ROBOTO_TAG_BASE));
    return 1.f;
}
inline void sc(cocos2d::CCLabelBMFont* l, float s) { l->setScale(s * kOf(l)); }
inline void fit(cocos2d::CCLabelBMFont* l, float width, float def, float minS) {
    float k = kOf(l), cw = l->getContentSize().width, s = def;
    if (cw > 0.f && cw * def * k > width) s = std::max(minS, width / (cw * k));
    l->setScale(s * k);
}
inline void setStyle(cocos2d::CCLabelBMFont* l, int style) {
    style = resolveFontStyle(style);
    bool arial = style == 1 && !fontBad();
    l->setFntFile(fontOf(style).c_str());
    l->setTag(style == 2 ? CHAT_TAG : style == 3 ? robotoTag(RobotoFace::ExtraLight) : (arial ? ARIAL_TAG : 0));
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
