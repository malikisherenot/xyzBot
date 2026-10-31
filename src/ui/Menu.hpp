#pragma once
#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/ScrollLayer.hpp>

class XyzMenu : public geode::Popup {
    int m_tab = 0;
    int m_sel = -1;
    std::string m_sub;
    float m_w = 0.f, m_h = 0.f;
    geode::ScrollLayer* m_scroll = nullptr;
    cocos2d::CCNode* m_side = nullptr;
    cocos2d::CCNode* m_glass = nullptr;
    float m_fade = 0.f;
    bool init() override;
    void buildSide();
    void rebuild();
    void fadeIn(float dt);
public:
    static XyzMenu* create();
    static void toggle();
    void onClose(cocos2d::CCObject*) override;
};
