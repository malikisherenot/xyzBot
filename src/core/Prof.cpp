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
    void checkCollisions(PlayerObject* p, float dt, bool b) {
        if (!xyz::cfg().profiler || xyz::rt().simulating) { GJBaseGameLayer::checkCollisions(p, dt, b); return; }
        xyz::ProfScope s(xyz::P_COLL);
        GJBaseGameLayer::checkCollisions(p, dt, b);
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
