#pragma once
#include <Geode/Geode.hpp>

namespace xyz::glass {
// Full-screen node: blurred snapshot of the running scene, masked to a centered rounded rect with a glass finish.
// Returns nullptr when shaders or render textures fail (caller should fall back to a flat panel).
cocos2d::CCNode* create(cocos2d::CCSize panel, float radius);
void setAlpha(cocos2d::CCNode* glass, float alpha);
void setTheme(cocos2d::CCNode* glass, cocos2d::ccColor3B accent, cocos2d::ccColor3B tint);
}
