#include "Menu.hpp"
#include "../core/State.hpp"
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/ui/OverlayManager.hpp>

using namespace geode::prelude;

class XyzFloatButton : public CCLayer {
    static constexpr float R = 22.f;
    CCNode* m_body = nullptr;
    CCPoint m_start;
    bool m_down = false, m_moved = false;
    float m_fx = 0.06f, m_fy = 0.86f;

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

        m_body = CCNode::create();
        this->addChild(m_body);

        auto dn = CCDrawNode::create();
        for (int i = 6; i >= 1; --i) dn->drawDot({0, 0}, R + i * 1.6f, {0.f, 0.9f, 0.6f, 0.035f * (7 - i)});
        dn->drawDot({0, 0}, R, {0.05f, 0.09f, 0.12f, 0.92f});
        dn->drawDot({0, 0}, R - 3.f, {0.f, 0.75f, 0.5f, 0.95f});
        dn->drawCircle({0, 0}, R - 0.5f, {0, 0, 0, 0}, 1.2f, {1, 1, 1, 0.85f}, 48);
        dn->drawDot({-R * 0.28f, R * 0.38f}, R * 0.45f, {1, 1, 1, 0.10f});
        m_body->addChild(dn);

        auto lbl = CCLabelBMFont::create("XYZ", "bigFont.fnt");
        lbl->setScale(.34f);
        m_body->addChild(lbl);

        this->setTouchEnabled(true);
        this->place(false);
        this->schedule(schedule_selector(XyzFloatButton::tick), 0.1f);
        return true;
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
        if (animate) this->runAction(CCEaseExponentialOut::create(CCMoveTo::create(0.18f, p)));
        else this->setPosition(p);
    }

    void tick(float) {
        bool on = Mod::get()->getSettingValue<bool>("floating-button");
        this->setVisible(on);
        if (!on) return;
        auto pl = PlayLayer::get();
        bool ingame = pl && !pl->m_isPaused;
        float target = m_down ? 0.9f : ingame ? 0.7f : 1.f;
        m_scale += (target - m_scale) * 0.4f;
        m_body->setScale(m_scale);
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
        return true;
    }

    void ccTouchMoved(CCTouch* t, CCEvent*) override {
        if (!m_moved && ccpDistance(t->getLocation(), m_start) > 8.f) m_moved = true;
        if (m_moved) this->setPosition(clampPos(t->getLocation()));
    }

    void ccTouchEnded(CCTouch*, CCEvent*) override {
        m_down = false;
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

    void ccTouchCancelled(CCTouch*, CCEvent*) override { m_down = false; m_moved = false; }

    float m_scale = 1.f;
};

class $modify(XyzFloatMenuLayer, MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) return false;
        static bool added = false;
        if (!added) {
            if (auto b = XyzFloatButton::create()) { OverlayManager::get()->addChild(b, 9999); added = true; }
        }
        return true;
    }
};
