#pragma once
#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/ScrollLayer.hpp>
#include <functional>

class XyzMenu : public geode::Popup {
    int m_tab = 0;
    int m_sel = -1;
    std::string m_sub;
    std::string m_mergeBase;
    std::string m_query;
    int m_libShown = 40;
    float m_w = 0.f, m_h = 0.f, m_cw = 0.f, m_ch = 0.f;
    bool m_paper = false, m_collapsed = false;
    cocos2d::CCDrawNode* m_panel = nullptr;
    cocos2d::CCDrawNode* m_glow = nullptr;
    cocos2d::CCDrawNode* m_tabBar = nullptr;
    cocos2d::CCNode* m_arrow = nullptr;
    std::vector<float> m_tabX, m_tabW;
    std::vector<cocos2d::CCDrawNode*> m_tabBg;
    geode::ScrollLayer* m_scroll = nullptr;
    geode::ScrollLayer* m_tabScroll = nullptr;
    cocos2d::CCLabelBMFont* m_footer = nullptr;
    cocos2d::CCNode* m_side = nullptr;
    cocos2d::CCNode* m_glass = nullptr;
    cocos2d::CCNode* m_hl = nullptr;
    cocos2d::CCNode* m_head = nullptr;
    std::vector<cocos2d::CCLabelBMFont*> m_tabLabels;
    std::vector<cocos2d::CCSprite*> m_tabIcons;
    float m_fade = 0.f;
    bool m_closing = false;
    int m_drag = 0, m_edge = 0;
    cocos2d::CCPoint m_dragFrom, m_startPos;
    cocos2d::CCSize m_startSize;
    bool m_geoDirty = false;
    float m_geoClock = 0.f;
    bool init() override;
    void buildSide();
    void buildHead();
    void buildFrame();
    void applyFont();
    void moveHl(bool animate);
    void rebuild(bool animate = false);
    void later(bool animate);
    void defer(std::function<void()> f);
    void themeLive();
    void themeFull();
    void fadeStep(float dt);
    void paperStep(float dt);
    void doClose(float dt);
    void drawPanel();
    void collapse();
    void restyle();
    void relayout(bool rows);
    void scrollTabs();
    void saveRect();
    void noAnim(cocos2d::CCNode* n);
public:
    ~XyzMenu();
    static XyzMenu* create();
    static void toggle();
    void show() override;
    void onClose(cocos2d::CCObject*) override;
    bool ccTouchBegan(cocos2d::CCTouch* t, cocos2d::CCEvent* e) override;
    void ccTouchMoved(cocos2d::CCTouch* t, cocos2d::CCEvent* e) override;
    void ccTouchEnded(cocos2d::CCTouch* t, cocos2d::CCEvent* e) override;
    void ccTouchCancelled(cocos2d::CCTouch* t, cocos2d::CCEvent* e) override;
};
