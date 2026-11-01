#include "StepBar.hpp"
#include "Draw.hpp"
#include "../core/State.hpp"
#include <Geode/ui/OverlayManager.hpp>

using namespace geode::prelude;
using namespace xyz::draw;

// On-screen frame stepper controls for touch devices: Step, +N, Hold (runs slowly while held), Run (turns the stepper off).
class XyzStepBar : public CCLayer {
    static constexpr int N = 4;
    CCDrawNode* m_art = nullptr;
    CCLabelBMFont* m_lbl[N] = {};
    CCRect m_rect[N];
    int m_held = -1;
    int m_lastFont = -1, m_lastN = -1, m_drawnHeld = -2;
    int m_accR = -1, m_accG = -1, m_accB = -1;

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
        char const* names[N] = {"Step", "+10", "Hold", "Run"};
        for (int i = 0; i < N; ++i) {
            m_lbl[i] = mkLabel(names[i]);
            this->addChild(m_lbl[i], 2);
        }
        this->layout();
        this->setTouchEnabled(true);
        this->setVisible(false);
        this->scheduleUpdate();
        return true;
    }

    void layout() {
        auto ws = CCDirector::get()->getWinSize();
        float bw = 54.f, bh = 28.f, gap = 6.f;
        float total = N * bw + (N - 1) * gap;
        float x0 = (ws.width - total) / 2.f, y = 12.f;
        for (int i = 0; i < N; ++i) {
            m_rect[i] = CCRect(x0 + i * (bw + gap), y, bw, bh);
            m_lbl[i]->setPosition({m_rect[i].getMidX(), m_rect[i].getMidY()});
        }
        m_drawnHeld = -2;
    }

    void redraw() {
        auto& c = xyz::cfg();
        m_drawnHeld = m_held;
        m_accR = c.accR; m_accG = c.accG; m_accB = c.accB;
        ccColor4F a = {c.accR / 255.f, c.accG / 255.f, c.accB / 255.f, 1.f};
        m_art->clear();
        for (int i = 0; i < N; ++i) {
            auto& r = m_rect[i];
            bool down = m_held == i;
            bool accent = i == 3;
            ccColor4F fill = accent ? ccColor4F{a.r * 0.75f, a.g * 0.75f, a.b * 0.75f, down ? 0.95f : 0.7f}
                                    : ccColor4F{0.04f, 0.07f, 0.1f, down ? 0.95f : 0.62f};
            ccColor4F border = down ? ccColor4F{a.r, a.g, a.b, 0.95f} : ccColor4F{1, 1, 1, 0.35f};
            rrect(m_art, {r.origin.x, r.origin.y}, r.size, 10, fill, 1.2f, border);
            m_lbl[i]->setScale(0.f);
            sc(m_lbl[i], down ? .3f : .34f);
        }
    }

    void update(float) override {
        auto& c = xyz::cfg();
        bool show = shouldShow();
        this->setVisible(show);
        if (!show) { m_held = -1; return; }
        if (c.menuFont != m_lastFont) {
            m_lastFont = c.menuFont;
            for (auto l : m_lbl) setStyle(l, c.menuFont);
            m_drawnHeld = -2;
        }
        if (c.stepN != m_lastN) {
            m_lastN = c.stepN;
            m_lbl[1]->setString(fmt::format("+{}", c.stepN).c_str());
            m_drawnHeld = -2;
        }
        if (m_drawnHeld != m_held || m_accR != c.accR || m_accG != c.accG || m_accB != c.accB) this->redraw();
        if (m_held == 2) for (int i = 0; i < std::clamp(c.holdRate, 1, 16); ++i) xyz::stepOnce();
    }

    void registerWithTouchDispatcher() override {
        CCTouchDispatcher::get()->addTargetedDelegate(this, -650, true);
    }

    bool ccTouchBegan(CCTouch* t, CCEvent*) override {
        if (!this->isVisible()) return false;
        auto p = t->getLocation();
        for (int i = 0; i < N; ++i) {
            if (!m_rect[i].containsPoint(p)) continue;
            m_held = i;
            auto& c = xyz::cfg();
            if (i == 0) xyz::stepOnce();
            else if (i == 1) for (int k = 0; k < std::clamp(c.stepN, 1, 240); ++k) xyz::stepOnce();
            else if (i == 3) { c.stepper = false; m_held = -1; }
            return true;
        }
        return false;
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
