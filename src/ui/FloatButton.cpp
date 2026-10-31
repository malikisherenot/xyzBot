#include "Menu.hpp"
#include "Draw.hpp"
#include "../core/State.hpp"
#include <Geode/modify/CCScheduler.hpp>
#include <Geode/ui/OverlayManager.hpp>

using namespace geode::prelude;
using namespace xyz::draw;

class XyzFloatButton : public CCLayer {
    static constexpr float R = 22.f;
    CCNode* m_body = nullptr;
    CCNode* m_halo = nullptr;
    CCDrawNode* m_art = nullptr;
    CCPoint m_start;
    bool m_down = false, m_moved = false;
    float m_fx = 0.06f, m_fy = 0.86f;
    float m_target = 1.f;
    int m_drawnR = -1, m_drawnG = -1, m_drawnB = -1;

public:
    static XyzFloatButton* create() {
        auto r = new XyzFloatButton();
        if (r->init()) { r->autorelease(); return r; }
        delete r; return nullptr;
    }

    bool init() override {
        if (!CCLayer::init()) return false;
        auto m = Mod::get();
        m_fx = (float)m->getSavedValue<double>("btn_fx", 0.06);
        m_fy = (float)m->getSavedValue<double>("btn_fy", 0.86);

        m_halo = CCNode::create();
        this->addChild(m_halo);
        m_body = CCNode::create();
        this->addChild(m_body);
        m_art = CCDrawNode::create();
        m_body->addChild(m_art);
        auto lbl = mkLabel("XYZ");
        sc(lbl, .34f);
        m_body->addChild(lbl, 2);

        this->redraw();
        m_halo->runAction(CCRepeatForever::create(CCSequence::create(
            CCEaseSineInOut::create(CCScaleTo::create(1.4f, 1.12f)),
            CCEaseSineInOut::create(CCScaleTo::create(1.4f, 0.96f)), nullptr)));

        this->setTouchEnabled(true);
        this->place(false);
        this->schedule(schedule_selector(XyzFloatButton::tick), 0.1f);
        return true;
    }

    void redraw() {
        auto& c = xyz::cfg();
        m_drawnR = c.accR; m_drawnG = c.accG; m_drawnB = c.accB;
        ccColor4F a = {c.accR / 255.f, c.accG / 255.f, c.accB / 255.f, 1.f};
        m_art->clear();
        auto h = static_cast<CCDrawNode*>(m_halo->getChildByTag(77));
        if (!h) { h = CCDrawNode::create(); h->setTag(77); m_halo->addChild(h); }
        h->clear();
        for (int i = 6; i >= 1; --i) disc(h, {0, 0}, R + i * 1.6f, {a.r, a.g, a.b, 0.035f * (7 - i)});
        disc(m_art, {0, 0}, R, {0.05f, 0.09f, 0.12f, 0.94f});
        disc(m_art, {0, 0}, R - 3.f, {a.r * 0.8f, a.g * 0.8f, a.b * 0.8f, 0.96f});
        disc(m_art, {0, 0}, R - 0.5f, {0, 0, 0, 0}, 1.2f, {1, 1, 1, 0.85f}, 48);
        disc(m_art, {-R * 0.28f, R * 0.38f}, R * 0.42f, {1, 1, 1, 0.12f});
    }

    void registerWithTouchDispatcher() override {
        CCTouchDispatcher::get()->addTargetedDelegate(this, -700, true);
    }

    CCPoint clampPos(CCPoint p) {
        auto s = CCDirector::get()->getWinSize();
        return {std::clamp(p.x, R + 2.f, s.width - R - 2.f), std::clamp(p.y, R + 2.f, s.height - R - 2.f)};
    }

    void place(bool animate) {
        auto s = CCDirector::get()->getWinSize();
        auto p = clampPos({m_fx * s.width, m_fy * s.height});
        if (animate) this->runAction(CCEaseBackOut::create(CCMoveTo::create(0.25f, p)));
        else this->setPosition(p);
    }

    void tick(float) {
        auto& c = xyz::cfg();
        auto pl0 = PlayLayer::get();
        bool inLevel = pl0 && !pl0->m_isPaused;
        bool on = Mod::get()->getSettingValue<bool>("floating-button") && (c.btnInLevel || !inLevel);
        this->setVisible(on);
        if (!on) return;
        if (c.accR != m_drawnR || c.accG != m_drawnG || c.accB != m_drawnB) this->redraw();
        auto pl = PlayLayer::get();
        float target = (pl && !pl->m_isPaused) ? 0.7f : 1.f;
        if (target != m_target && !m_down) {
            m_target = target;
            m_body->stopAllActions();
            m_body->runAction(CCEaseBackOut::create(CCScaleTo::create(0.25f, m_target)));
        }
        if (!m_down && !m_moved) {
            auto s = CCDirector::get()->getWinSize();
            auto p = clampPos({m_fx * s.width, m_fy * s.height});
            if (ccpDistance(p, this->getPosition()) > 1.f && !this->numberOfRunningActions()) this->setPosition(p);
        }
    }

    bool hit(CCTouch* t) {
        return this->isVisible() && ccpDistance(t->getLocation(), this->getPosition()) <= R + 8.f;
    }

    bool ccTouchBegan(CCTouch* t, CCEvent*) override {
        if (!hit(t)) return false;
        m_start = t->getLocation(); m_moved = false; m_down = true;
        this->stopAllActions();
        m_body->stopAllActions();
        m_body->runAction(CCEaseSineOut::create(CCScaleTo::create(0.08f, m_target * 0.85f)));
        return true;
    }

    void ccTouchMoved(CCTouch* t, CCEvent*) override {
        if (!m_moved && ccpDistance(t->getLocation(), m_start) > 8.f) m_moved = true;
        if (m_moved) this->setPosition(clampPos(t->getLocation()));
    }

    void release() {
        m_down = false;
        m_body->stopAllActions();
        m_body->runAction(CCEaseBackOut::create(CCScaleTo::create(0.28f, m_target)));
    }

    void ccTouchEnded(CCTouch*, CCEvent*) override {
        release();
        if (m_moved) {
            auto s = CCDirector::get()->getWinSize();
            auto p = this->getPosition();
            m_fx = p.x < s.width / 2 ? 0.035f : 0.965f;
            m_fy = p.y / s.height;
            Mod::get()->setSavedValue<double>("btn_fx", m_fx);
            Mod::get()->setSavedValue<double>("btn_fy", m_fy);
            this->place(true);
            m_moved = false;
            return;
        }
        if (auto pl = PlayLayer::get(); pl && !pl->m_isPaused) pl->pauseGame(false);
        XyzMenu::toggle();
    }

    void ccTouchCancelled(CCTouch*, CCEvent*) override { release(); m_moved = false; }
};

static Ref<XyzFloatButton> s_btn;

class $modify(XyzFloatSched, CCScheduler) {
    void update(float dt) {
        CCScheduler::update(dt);
        static int frames = 0;
        if (++frames < 5) return;
        auto scene = CCDirector::get()->getRunningScene();
        if (!scene) return;
        auto ov = OverlayManager::get();
        if (!ov) return;
        if (!s_btn) s_btn = XyzFloatButton::create();
        if (s_btn && s_btn->getParent() != ov) {
            if (s_btn->getParent()) s_btn->removeFromParent();
            ov->addChild(s_btn, 9999);
        }
    }
};
