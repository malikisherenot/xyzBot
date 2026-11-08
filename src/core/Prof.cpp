#include "State.hpp"
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/modify/PlayLayer.hpp>

using namespace geode::prelude;

class $modify(XyzProfPlayer, PlayerObject) {
    void update(float dt) {
        if (!xyz::cfg().profiler || xyz::rt().simulating) { PlayerObject::update(dt); return; }
        xyz::ProfScope s(xyz::P_PLAYER);
        PlayerObject::update(dt);
    }
};

class $modify(XyzProfGL, GJBaseGameLayer) {
    void collisionCheckObjects(PlayerObject* player, gd::vector<GameObject*>* objects, int length, float dt) {
        if (!xyz::cfg().profiler || xyz::rt().simulating) { GJBaseGameLayer::collisionCheckObjects(player, objects, length, dt); return; }
        xyz::ProfScope s(xyz::P_COLL);
        GJBaseGameLayer::collisionCheckObjects(player, objects, length, dt);
    }
    void updateEffects(float dt) {
        if (!xyz::cfg().profiler || xyz::rt().simulating) { GJBaseGameLayer::updateEffects(dt); return; }
        xyz::ProfScope s(xyz::P_FX);
        GJBaseGameLayer::updateEffects(dt);
    }
    void updateEnterEffects(float dt) {
        if (!xyz::cfg().profiler || xyz::rt().simulating) { GJBaseGameLayer::updateEnterEffects(dt); return; }
        xyz::ProfScope s(xyz::P_ENTER);
        GJBaseGameLayer::updateEnterEffects(dt);
    }
    void updateCamera(float dt) {
        if (!xyz::cfg().profiler || xyz::rt().simulating) { GJBaseGameLayer::updateCamera(dt); return; }
        xyz::ProfScope s(xyz::P_CAM);
        GJBaseGameLayer::updateCamera(dt);
    }
};

class $modify(XyzProfPL, PlayLayer) {
    void postUpdate(float dt) {
        if (!xyz::cfg().profiler || xyz::rt().simulating) { PlayLayer::postUpdate(dt); return; }
        xyz::ProfScope s(xyz::P_POST);
        PlayLayer::postUpdate(dt);
    }
};
