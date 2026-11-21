#include "StepBar.hpp"
#include "Draw.hpp"
#include "../core/State.hpp"
#include <Geode/ui/OverlayManager.hpp>

using namespace geode::prelude;
using namespace xyz::draw;

// the frame stepper is a tiny mini-menu now: one little panel, title on top, buttons below (^o^)/
class XyzStepBar : public CCLayer {
    static constexpr int N = 6; // 0 back, 1 -N, 2 step, 3 +N, 4 hold, 5 run (the pill in the header)
    CCDrawNode* m_art = nullptr;
    CCLabelBMFont* m_lbl[N] = {};
    CCLabelBMFont* m_title = nullptr;
    CCRect m_rect[N];
    CCRect m_panel;
    float m_headY = 0.f;
    int m_held = -1;
    int m_lastFont = -1, m_lastN = -1, m_drawnHeld = -2;
    int m_accR = -1, m_accG = -1, m_accB = -1;
    uint32_t m_lastTick = 0xFFFFFFFFu;

public:
    static XyzStepBar* create() {
        auto r = new XyzStepBar();
        if (r->init()) { r->autorelease(); return r; }
        delete r; return nullptr;
    }

    static bool shouldShow() {
        auto& c = xyz::cfg();
        if (!c.stepper || !c.stepBar) return false;
        auto gl = xyz::activeLayer();
        if (!gl) return false;
        if (auto pl = typeinfo_cast<PlayLayer*>(gl)) return !pl->m_isPaused;
        return true;
    }

    bool init() override {
        if (!CCLayer::init()) return false;
        m_art = CCDrawNode::create();
        this->addChild(m_art);
        char const* names[N] = {"Back", "-10", "Step", "+10", "Hold", "Run"};
        for (int i = 0; i < N; ++i) {
            m_lbl[i] = mkLabel(names[i]);
            this->addChild(m_lbl[i], 2);
        }
        m_title = mkLabel("FRAME");
        m_title->setAnchorPoint({0.f, 0.5f});
        this->addChild(m_title, 2);
        this->layout();
        this->setTouchEnabled(true);
        this->setVisible(false);
        this->scheduleUpdate();
        return true;
    }

    void layout() {
        auto ws = CCDirector::get()->getWinSize();
        float pad = 6.f, bw = 34.f, bh = 18.f, gap = 3.f, head = 12.f;
        float pw = pad * 2.f + 5.f * bw + 4.f * gap;
        float ph = pad + bh + 3.f + head + 4.f;
        float x0 = ws.width - pw - 10.f, y0 = 10.f;
        m_panel = CCRect(x0, y0, pw, ph);
        for (int i = 0; i < 5; ++i) {
            m_rect[i] = CCRect(x0 + pad + i * (bw + gap), y0 + pad, bw, bh);
            m_lbl[i]->setPosition({m_rect[i].getMidX(), m_rect[i].getMidY()});
        }
        m_headY = y0 + pad + bh + 3.f;
        m_rect[5] = CCRect(x0 + pw - pad - 28.f, m_headY, 28.f, head);
        m_lbl[5]->setPosition({m_rect[5].getMidX(), m_rect[5].getMidY()});
        m_title->setPosition({x0 + pad + 1.f, m_headY + head / 2.f});
        m_drawnHeld = -2;
    }

    void redraw() {
        auto& c = xyz::cfg();
        m_drawnHeld = m_held;
        m_accR = c.accR; m_accG = c.accG; m_accB = c.accB;
        ccColor4F a = {c.accR / 255.f, c.accG / 255.f, c.accB / 255.f, 1.f};
        m_art->clear();
        // the panel itself + a thin line under the title
        rrect(m_art, {m_panel.origin.x, m_panel.origin.y}, m_panel.size, 8, {0.04f, 0.07f, 0.1f, 0.8f}, 1.2f, {1, 1, 1, 0.28f});
        m_art->drawSegment({m_panel.origin.x + 6.f, m_headY - 1.5f}, {m_panel.origin.x + m_panel.size.width - 6.f, m_headY - 1.5f}, 0.4f, {1, 1, 1, 0.14f});
        m_title->setColor({(GLubyte)c.accR, (GLubyte)c.accG, (GLubyte)c.accB});
        m_title->setScale(0.f); sc(m_title, .26f);
        for (int i = 0; i < N; ++i) {
            auto& r = m_rect[i];
            bool down = m_held == i;
            bool accent = i == 5;
            ccColor4F fill = accent ? ccColor4F{a.r * 0.75f, a.g * 0.75f, a.b * 0.75f, down ? 0.95f : 0.7f}
                                    : ccColor4F{1, 1, 1, down ? 0.22f : 0.08f};
            ccColor4F border = down ? ccColor4F{a.r, a.g, a.b, 0.95f} : ccColor4F{1, 1, 1, accent ? 0.f : 0.22f};
            rrect(m_art, {r.origin.x, r.origin.y}, r.size, accent ? 5.f : 6.f, fill, 1.f, border);
            m_lbl[i]->setScale(0.f);
            sc(m_lbl[i], accent ? .22f : (down ? .25f : .28f));
        }
    }

    void update(float) override {
        auto& c = xyz::cfg();
        bool show = shouldShow();
        this->setVisible(show);
        if (!show) { m_held = -1; return; }
        if (xyz::menuFontNow() != m_lastFont) {
            m_lastFont = xyz::menuFontNow();
            for (auto l : m_lbl) setStyle(l, xyz::menuFontNow());
            setStyle(m_title, xyz::menuFontNow());
            m_drawnHeld = -2;
        }
        if (c.stepN != m_lastN) {
            m_lastN = c.stepN;
            m_lbl[1]->setString(fmt::format("-{}", c.stepN).c_str());
            m_lbl[3]->setString(fmt::format("+{}", c.stepN).c_str());
            m_drawnHeld = -2;
        }
        uint32_t t = xyz::rt().tick;
        if (t != m_lastTick) {
            m_lastTick = t;
            m_title->setString(fmt::format("FRAME  {}", t).c_str());
            m_title->setScale(0.f); sc(m_title, .26f);
        }
        if (m_drawnHeld != m_held || m_accR != c.accR || m_accG != c.accG || m_accB != c.accB) this->redraw();
        if (m_held == 4) for (int i = 0; i < std::clamp(c.holdRate, 1, 16); ++i) xyz::stepOnce();
    }

    void registerWithTouchDispatcher() override {
        CCTouchDispatcher::get()->addTargetedDelegate(this, -650, true);
    }

    bool ccTouchBegan(CCTouch* t, CCEvent*) override {
        if (!this->isVisible()) return false;
        auto p = t->getLocation();
        if (!m_panel.containsPoint(p)) return false;
        for (int i = 0; i < N; ++i) {
            if (!m_rect[i].containsPoint(p)) continue;
            m_held = i;
            auto& c = xyz::cfg();
            if (i == 0) xyz::stepBack(1);
            else if (i == 1) xyz::stepBack(std::clamp(c.stepN, 1, 240));
            else if (i == 2) xyz::stepOnce();
            else if (i == 3) for (int k = 0; k < std::clamp(c.stepN, 1, 240); ++k) xyz::stepOnce();
            else if (i == 5) { c.stepper = false; m_held = -1; }
            return true;
        }
        return true; // taps on the panel itself should not click the game behind it
    }

    void ccTouchEnded(CCTouch*, CCEvent*) override { m_held = -1; }
    void ccTouchCancelled(CCTouch*, CCEvent*) override { m_held = -1; }
};

namespace xyz {
static Ref<XyzStepBar> s_bar;

void ensureStepBar() {
    auto ov = OverlayManager::get();
    if (!ov) return;
    if (!s_bar) s_bar = XyzStepBar::create();
    if (s_bar && s_bar->getParent() != ov) {
        if (s_bar->getParent()) s_bar->removeFromParent();
        ov->addChild(s_bar, 9998);
    }
}
}
