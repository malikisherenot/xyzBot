#pragma once
#include <Geode/Geode.hpp>

namespace xyz::glass {
cocos2d::CCNode* create(cocos2d::CCSize panel, float radius);
void setAlpha(cocos2d::CCNode* glass, float alpha);
void setTheme(cocos2d::CCNode* glass, cocos2d::ccColor3B accent, cocos2d::ccColor3B tint);
}
