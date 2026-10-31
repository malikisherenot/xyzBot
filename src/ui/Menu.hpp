#pragma once
#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/ScrollLayer.hpp>
#include <functional>

class XyzMenu : public geode::Popup {
    int m_tab = 0;
    int m_sel = -1;
    std::string m_sub;
    float m_w = 0.f, m_h = 0.f;
    geode::ScrollLayer* m_scroll = nullptr;
    cocos2d::CCNode* m_side = nullptr;
    cocos2d::CCNode* m_glass = nullptr;
    cocos2d::CCNode* m_hl = nullptr;
    std::vector<cocos2d::CCLabelBMFont*> m_tabLabels;
    float m_fade = 0.f;
    bool m_closing = false;
    bool init() override;
    void buildSide();
    void moveHl(bool animate);
    void rebuild(bool animate = false);
    void later(bool animate);
    void defer(std::function<void()> f);
    void themeLive();
    void themeFull();
    void fadeStep(float dt);
    void doClose(float dt);
public:
    static XyzMenu* create();
    static void toggle();
    void onClose(cocos2d::CCObject*) override;
};
