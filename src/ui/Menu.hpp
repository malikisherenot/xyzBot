#pragma once
#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>

class XyzMenu : public geode::Popup {
    int m_tab = 0;
    int m_page = 0;
    int m_sel = -1;
    cocos2d::CCNode* m_content = nullptr;
    bool init() override;
    void build();
    void onClose(cocos2d::CCObject*) override;
    void row(float y, std::string label, bool on, std::function<void(bool)> cb);
    void num(float y, std::string label, std::function<std::string()> get, std::function<void(int)> step);
    void btn(float x, float y, char const* text, std::function<void()> cb);
public:
    static XyzMenu* create();
    static void toggle();
};
