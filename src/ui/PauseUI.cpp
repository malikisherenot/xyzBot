// pause menu extras: exit lock and Copy Pos, bottom center under the play button
#include <Geode/Geode.hpp>
#include "Draw.hpp"
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>

using namespace geode::prelude;

namespace {
bool g_exitLock = false;
}

class $modify(XyzLockPlay, PlayLayer) {
    bool init(GJGameLevel* level, bool a, bool b) {
        g_exitLock = false; // a new level always starts unlocked
        return PlayLayer::init(level, a, b);
    }
};

class $modify(XyzPause, PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();
        auto win = CCDirector::get()->getWinSize();
        auto menu = CCMenu::create();
        menu->setPosition({0.f, 0.f});
        this->addChild(menu, 10);

        auto lockSpr = [](bool locked) {
            auto s = CCSprite::createWithSpriteFrameName(locked ? "GJ_lock_001.png" : "GJ_lock_open_001.png");
            s->setScale(.75f);
            return s;
        };
        auto lock = CCMenuItemExt::createSpriteExtra(lockSpr(g_exitLock), [lockSpr](CCMenuItemSpriteExtra* self) {
            g_exitLock = !g_exitLock;
            self->setSprite(lockSpr(g_exitLock));
        });
        menu->addChild(lock);

        auto cs = xyz::draw::restyleButton(ButtonSprite::create("Copy Pos"));
        cs->setScale(.6f);
        auto copy = CCMenuItemExt::createSpriteExtra(cs, [](CCMenuItemSpriteExtra*) {
            auto pl = PlayLayer::get();
            if (!pl || !pl->m_player1) return;
            auto p = pl->m_player1->getPosition();
            geode::utils::clipboard::write(fmt::format("{:.3f}, {:.3f}", p.x, p.y));
        });
        // bottom center, under the volume sliders: lock and Copy Pos side by side
        float csW = cs->getScaledContentSize().width, lockW = 32.f, gap = 12.f;
        float left = win.width / 2.f - (lockW + gap + csW) / 2.f, y = win.height * 0.075f; // under the volume sliders
        lock->setPosition({left + lockW / 2.f, y});
        copy->setPosition({left + lockW + gap + csW / 2.f, y});
        menu->addChild(copy);
    }

    void onQuit(CCObject* sender) {
        if (g_exitLock) {
            Notification::create("Exit is locked, unlock it with the lock button", NotificationIcon::Warning)->show();
            return;
        }
        PauseLayer::onQuit(sender);
    }
};
