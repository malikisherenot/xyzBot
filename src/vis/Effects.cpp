// Showcase / rendering helpers ported from Scarlet Utils (own implementation).
#include "../core/State.hpp"
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/modify/RingObject.hpp>
#include <Geode/modify/GameObject.hpp>
#include <Geode/modify/EffectGameObject.hpp>
#include <Geode/modify/CCCircleWave.hpp>
#include <Geode/modify/ParticleGameObject.hpp>
#include <Geode/modify/EndLevelLayer.hpp>
#include <Geode/modify/GJGroundLayer.hpp>
#include <Geode/modify/ShaderLayer.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/binding/FMODAudioEngine.hpp>

using namespace geode::prelude;

class $modify(XyzFxPlayer, PlayerObject) {
    void spawnPortalCircle(ccColor3B color, float startRadius) {
        if (!xyz::cfg().noEffect) PlayerObject::spawnPortalCircle(color, startRadius);
    }
    void playDeathEffect() {
        if (!xyz::cfg().noDeathEffect) PlayerObject::playDeathEffect();
    }
    void playSpiderDashEffect(CCPoint from, CCPoint to) {
        if (!xyz::cfg().noEffect) PlayerObject::playSpiderDashEffect(from, to);
    }
    void startDashing(DashRingObject* object) {
        auto& c = xyz::cfg(); auto& r = xyz::rt();
        static int lastOrb = 0;
        bool same = object && lastOrb == object->m_objectID;
        if (c.noEffect || (c.optStacked && r.optStart && same)) {
            auto og = m_playEffects;
            m_playEffects = false;
            PlayerObject::startDashing(object);
            m_playEffects = og;
        } else PlayerObject::startDashing(object);
        r.optStart = true;
        if (object) lastOrb = object->m_objectID;
    }
    void stopDashing() {
        auto& c = xyz::cfg(); auto& r = xyz::rt();
        if (c.noEffect || (c.optStacked && r.optStop)) {
            auto og = m_maybeReducedEffects;
            m_maybeReducedEffects = true;
            PlayerObject::stopDashing();
            m_maybeReducedEffects = og;
        } else PlayerObject::stopDashing();
        r.optStop = true;
    }
};

class $modify(XyzFxRing, RingObject) {
    void spawnCircle() {
        auto& c = xyz::cfg(); auto& r = xyz::rt();
        static int lastOrb = 0;
        bool same = lastOrb == m_objectID;
        if (!c.noEffect && !(c.optStacked && r.optRing && same)) RingObject::spawnCircle();
        r.optRing = true;
        lastOrb = m_objectID;
    }
};

class $modify(XyzFxObject, GameObject) {
    void playShineEffect() {
        if (!xyz::cfg().noEffect) GameObject::playShineEffect();
    }
};

class $modify(XyzFxTrigger, EffectGameObject) {
    void playTriggerEffect() {
        if (!xyz::cfg().noEffect) EffectGameObject::playTriggerEffect();
    }
};

class $modify(XyzFxWave, CCCircleWave) {
    void draw() {
        if (!xyz::cfg().noEffect) CCCircleWave::draw();
    }
};

class $modify(XyzFxParticle, ParticleGameObject) {
    void updateParticle() {
        ParticleGameObject::updateParticle();
        if (xyz::cfg().noEffect) this->setVisible(false);
    }
};

class $modify(XyzFxEnd, EndLevelLayer) {
    void showLayer(bool instant) {
        if (!xyz::cfg().hideEndscreen) EndLevelLayer::showLayer(instant);
    }
};

class $modify(XyzFxGround, GJGroundLayer) {
    static GJGroundLayer* create(int groundID, int lineType) {
        if (xyz::layoutOn()) { groundID = 1; lineType = 1; }
        return GJGroundLayer::create(groundID, lineType);
    }
};

class $modify(XyzFxShader, ShaderLayer) {
    void performCalculations() {
        if (!xyz::layoutOn()) ShaderLayer::performCalculations();
    }
};

class $modify(XyzFxBGL, GJBaseGameLayer) {
    void playExitDualEffect(PlayerObject* player) {
        auto& c = xyz::cfg();
        if (!((m_playerDied && c.noDeathEffect) || c.noEffect)) GJBaseGameLayer::playExitDualEffect(player);
    }

    void updateColor(ccColor3B& color, float fadeTime, int colorID, bool blending, float opacity, ccHSVValue& copyHSV,
                     int colorIDToCopy, bool copyOpacity, EffectGameObject* callerObject, int unk1, int unk2) {
        if (xyz::layoutOn()) {
            auto& c = xyz::cfg();
            if (colorID == 1000) color = {(GLubyte)c.layoutBg[0], (GLubyte)c.layoutBg[1], (GLubyte)c.layoutBg[2]};
            else if (colorID == 1001) color = {(GLubyte)c.layoutGr[0], (GLubyte)c.layoutGr[1], (GLubyte)c.layoutGr[2]};
            else color = {255, 255, 255};
        }
        GJBaseGameLayer::updateColor(color, fadeTime, colorID, blending, opacity, copyHSV, colorIDToCopy, copyOpacity, callerObject, unk1, unk2);
    }

    void createBackground(int background) {
        if (xyz::layoutOn()) background = 13;
        GJBaseGameLayer::createBackground(background);
    }

    void createMiddleground(int middleground) {
        if (xyz::layoutOn()) middleground = 0;
        GJBaseGameLayer::createMiddleground(middleground);
    }
};

class $modify(XyzFxPL, PlayLayer) {
    struct Fields { CCLayerColor* start = nullptr; CCLayerColor* end = nullptr; };

    void addObject(GameObject* object) {
        if (xyz::cfg().noEffect && object->m_objectType != GameObjectType::InverseMirrorPortal && object->m_objectType != GameObjectType::NormalMirrorPortal)
            object->m_hasNoEffects = true;
        PlayLayer::addObject(object);
    }

    void playGravityEffect(bool flip) {
        if (!xyz::cfg().noEffect) PlayLayer::playGravityEffect(flip);
    }

    void showNewBest(bool newReward, int orbs, int diamonds, bool demonKey, bool noRetry, bool noTitle) {
        if (!xyz::cfg().hideNewBest) PlayLayer::showNewBest(newReward, orbs, diamonds, demonKey, noRetry, noTitle);
    }

    void onEnterTransitionDidFinish() {
        if (xyz::layoutOn()) this->toggleGlitter(false);
        PlayLayer::onEnterTransitionDidFinish();
    }

    void startGame() {
        PlayLayer::startGame();
        auto& c = xyz::cfg();
        if (c.fadeLevel && c.fadeLevelIn > 0.f && !(m_fields->start && m_fields->start->getParent())) {
            auto ws = CCDirector::get()->getWinSize();
            m_fields->start = CCLayerColor::create(ccc4(0, 0, 0, 255), ws.width, ws.height);
            this->addChild(m_fields->start, 100);
            m_fields->start->runAction(CCSequence::create(CCFadeOut::create(c.fadeLevelIn), CCRemoveSelf::create(), nullptr));
            if (c.fadeAudio && c.fadeAudioIn > 0.f) FMODAudioEngine::sharedEngine()->fadeMusic(c.fadeAudioIn, 0, 0.f, 1.f);
        }
        if (c.restartFirst) queueInMainThread([] { if (auto p = PlayLayer::get()) p->resetLevel(); });
    }

    void showEndLayer() {
        PlayLayer::showEndLayer();
        auto& c = xyz::cfg();
        if (c.fadeLevel && c.fadeLevelOut > 0.f && !(m_fields->end && m_fields->end->getParent())) {
            auto ws = CCDirector::get()->getWinSize();
            m_fields->end = CCLayerColor::create(ccc4(0, 0, 0, 255), ws.width, ws.height);
            m_fields->end->setOpacity(0);
            this->addChild(m_fields->end, 1000);
            m_fields->end->runAction(CCFadeIn::create(c.fadeLevelOut));
            if (c.fadeAudio && c.fadeAudioOut > 0.f) FMODAudioEngine::sharedEngine()->fadeMusic(c.fadeAudioOut, 0, 1.f, 0.f);
        }
    }
};
