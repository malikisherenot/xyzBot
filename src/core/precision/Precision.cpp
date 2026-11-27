#include "Precision.hpp"
#include "../State.hpp"
#include <Geode/modify/PlayerObject.hpp>
#include <cmath>
#include <cstdint>
#include <tuple>
#include <vector>

using namespace geode::prelude;

namespace xyz::precision {
namespace {

// ---------------- Velocity Unrounding ----------------
// GD rounds the y velocity to the third decimal in several places; most of them are inlined, so the code that does it is
// overwritten with NOPs. The addresses are for one GD version and one platform, every other build can't be patched safely.
struct PatchGroup {
    std::vector<Patch*> applied;

    void init(std::vector<std::tuple<uintptr_t, size_t, uint8_t>> const& entries) {
        for (auto& [rva, size, value] : entries) {
            ByteVector bytes(size, value);
            auto result = Mod::get()->patch(reinterpret_cast<void*>(base::get() + rva), bytes);
            if (result.isOk()) {
                auto* patch = result.unwrap();
                patch->setAutoEnable(false);
                (void)patch->disable();
                applied.push_back(patch);
            } else {
                log::error("[unrounding] could not create the patch at 0x{:X}: {}", rva, result.unwrapErr());
            }
        }
    }
    void toggle(bool enable) {
        for (auto* patch : applied) {
            auto result = patch->toggle(enable);
            if (!result.isOk()) log::error("[unrounding] could not toggle a patch: {}", result.unwrapErr());
        }
    }
};

// the places that round the y velocity, per GD version (Windows only): {address, length, byte}
std::vector<std::tuple<uintptr_t, size_t, uint8_t>> patchesFor(std::string const& ver, std::string& note) {
#ifdef GEODE_IS_WINDOWS
    if (ver == "2.2081") {
        note = "5 places, the checkpoint load is not patched in 2.2081";
        return {
            {0x38C329, 0x24, 0x90}, // updateJump
            {0x213EA2, 0x32, 0x90}, // checkCollisions
            {0x38DAC7, 0x38, 0x90}, // postCollision
            {0x39323B, 0x40, 0x90}, // collidedWithObjectInternal
            {0x39FF18, 0x32, 0x90}, // boostPlayer
        };
    }
    if (ver == "2.2091") {
        note = "all 6 places";
        return {
            {0x4EA5BC, 0x24, 0x90}, // updateJump
            {0x31B1CE, 0x32, 0x90}, // checkCollisions
            {0x4EBDF8, 0x38, 0x90}, // postCollision
            {0x4F146A, 0x3F, 0x90}, // collidedWithObjectInternal
            {0x4FE334, 0x26, 0x90}, // boostPlayer
            {0x501726, 0x32, 0x90}, // loadFromCheckpoint
        };
    }
#endif
    (void)ver; (void)note;
    return {};
}

PatchGroup g_nops;
std::string g_velStatus = "Velocity Unrounding: off";
bool g_synced = false;

// ---------------- X Position Extension ----------------
// Positions are 32 bit floats. At a very high TPS one tick moves the player by less than the gap between two floats at
// its x, so the game's own addition loses the movement. This keeps the exact x in a double and gives the game the closest float.
struct Ext { double xd = 0.0; bool valid = false; };
Ext g_ext[2];
double g_k = 0.0;   // units per second at speed 1.0, measured on steps that are big enough to read it (about 346.2)
int g_kSamples = 0;
bool g_kLoaded = false;

double ulpOf(float x) { return (double)(std::nextafter(x, INFINITY) - x); }

} // namespace

void applyVelUnround(bool enable) {
    auto ver = Loader::get()->getGameVersion();
    std::string note;
    auto list = patchesFor(ver, note);
    if (list.empty()) {
        g_velStatus = enable ? fmt::format("Velocity Unrounding: partial, the patches exist only for Windows 2.2081 and 2.2091 (this is {})", ver)
                             : std::string("Velocity Unrounding: off");
        return;
    }
    if (g_nops.applied.empty()) g_nops.init(list);
    g_nops.toggle(enable);
    g_velStatus = enable ? fmt::format("Velocity Unrounding: on (Windows {}, {})", ver, note) : std::string("Velocity Unrounding: off");
}

std::string status() {
    std::string s = g_velStatus;
    if (cfg().xExt) s += g_kSamples >= 5 ? fmt::format("\nX Extension: ready, {:.1f} units/s at speed 1", g_k) : std::string("\nX Extension: waiting to measure the speed");
    return s;
}

class $modify(XyzPrecisionPlayer, PlayerObject) {
    // with the option on the velocity is stored as it is (the patches take out the rounding that is inlined elsewhere)
    void setYVelocity(double velocity, int type) {
        if (cfg().velUnround) {
            this->m_yVelocity = velocity;
            return;
        }
        PlayerObject::setYVelocity(velocity, type);
    }

    void update(float dt) {
        if (!g_synced) { // first physics step: bring the patches in line with the saved option
            g_synced = true;
            applyVelUnround(cfg().velUnround);
        }
        auto gl = this->m_gameLayer;
        if (!cfg().xExt || !gl || this->m_isPlatformer || (this != gl->m_player1 && this != gl->m_player2) || dt <= 0.f) {
            PlayerObject::update(dt);
            return;
        }
        if (!g_kLoaded) {
            g_kLoaded = true;
            g_k = Mod::get()->getSavedValue<double>("xext-k", 0.0);
            if (g_k > 0.0) g_kSamples = 5;
        }

        auto& e = g_ext[this == gl->m_player2 ? 1 : 0];
        float x0 = this->getPositionX();
        if (!e.valid || std::abs(e.xd - (double)x0) > 2.0 * ulpOf(x0) + 1e-6) { e.xd = x0; e.valid = true; } // moved by something else

        PlayerObject::update(dt);

        float x1 = this->getPositionX();
        double ulp = ulpOf(x1), obs = (double)x1 - (double)x0, speed = this->m_playerSpeed;
        double est = g_k * speed * (double)dt;
        if (g_kSamples >= 5 && est > 0.0 && est < 8.0 * ulp && std::abs(obs - est) <= 1.5 * ulp) {
            e.xd += est;                               // the movement the float addition could not hold
            this->setPositionX((float)e.xd);
        } else {
            e.xd = x1;
            if (speed > 0.0 && obs > 400.0 * ulp) {    // a step big enough to read the speed from
                double kObs = obs / (speed * (double)dt);
                if (kObs > 1.0 && kObs < 5000.0) {
                    g_k = g_kSamples == 0 ? kObs : g_k * 0.95 + kObs * 0.05;
                    if (++g_kSamples == 5) Mod::get()->setSavedValue("xext-k", g_k);
                }
            }
        }
    }
};

} // namespace xyz::precision
