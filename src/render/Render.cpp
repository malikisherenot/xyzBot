// Video rendering. The game is run one video frame per update (fixed frame time), every frame is drawn into a render
// texture, read back and handed to the eclipse.ffmpeg-api mod for encoding. The way frames are handed over follows Echo (GPL-3.0).
#include "../core/State.hpp"
#include <Geode/modify/CCDirector.hpp>
#include <Geode/ui/OverlayManager.hpp>
#include <Geode/binding/FMODAudioEngine.hpp>
#include <Geode/binding/MusicDownloadManager.hpp>
#include <Geode/binding/LevelTools.hpp>
#include <filesystem>

#if __has_include(<eclipse.ffmpeg-api/include/events.hpp>)
#include <eclipse.ffmpeg-api/include/events.hpp>
#define XYZ_FFMPEG 1
#endif

using namespace geode::prelude;
namespace fs = std::filesystem;

namespace {
bool g_on = false;
int g_tail = -1;
long long g_frames = 0;
int g_w = 0, g_h = 0, g_fps = 60;
fs::path g_out, g_song;
Ref<CCRenderTexture> g_rt;
std::vector<unsigned char> g_buf;
#ifdef XYZ_FFMPEG
std::unique_ptr<ffmpeg::events::Recorder> g_enc;
#endif

fs::path renderDir() {
    auto p = Mod::get()->getSaveDir() / "renders";
    std::error_code ec; fs::create_directories(p, ec);
    return p;
}

void setMusicVolume(float v) {
    if (auto e = FMODAudioEngine::sharedEngine(); e && e->m_backgroundMusicChannel) e->m_backgroundMusicChannel->setVolume(v);
}
}

namespace xyz {
bool renderAvailable() {
#ifdef XYZ_FFMPEG
    return Loader::get()->isModLoaded("eclipse.ffmpeg-api");
#else
    return false;
#endif
}

bool isRendering() { return g_on; }
float renderFps() { return g_on ? (float)g_fps : 0.f; }

std::string startRender() {
#ifndef XYZ_FFMPEG
    return "This build has no FFmpeg API support";
#else
    if (g_on) return "Already rendering";
    if (!renderAvailable()) return "Install the FFmpeg API mod (eclipse.ffmpeg-api)";
    auto pl = PlayLayer::get();
    if (!pl) return "Open a level first";
    auto& c = cfg();

    // video size: the width you chose, height follows the screen's shape (both even)
    auto ws = CCDirector::get()->getWinSize();
    g_w = std::clamp(c.renderW, 320, 3840) & ~1;
    g_h = (int)std::lround(g_w * ws.height / ws.width) & ~1;
    g_fps = std::clamp(c.renderFps, 10, 240);

    std::string name = macroFileName();
    g_out = renderDir() / (name + ".mp4");
    for (int n = 1; fs::exists(g_out) && n < 1000; ++n) g_out = renderDir() / (name + " (" + std::to_string(n) + ").mp4");

    static char const* CODECS[] = {"", "libx264", "h264_mediacodec", "libopenh264", "mpeg4"};
    auto available = ffmpeg::events::Recorder::getAvailableCodecs();
    auto offered = [&](std::string const& n) { return available.empty() || std::find(available.begin(), available.end(), n) != available.end(); };
    std::vector<std::string> order;
    if (c.renderCodec > 0 && c.renderCodec < 5) order.push_back(CODECS[c.renderCodec]);
#ifdef GEODE_IS_ANDROID
    order.push_back("h264_mediacodec");
#endif
    for (auto n : {"libx264", "h264_videotoolbox", "h264_mediacodec", "libopenh264", "mpeg4"}) order.push_back(n);

    std::string error = "No usable video codec";
    g_enc.reset();
    for (auto& codec : order) {
        if (!offered(codec)) continue;
        ffmpeg::RenderSettings s;
        s.m_pixelFormat = ffmpeg::PixelFormat::RGBA;
        s.m_codec = codec;
        s.m_bitrate = (long long)std::clamp(c.renderBitrate, 1, 200) * 1'000'000;
        s.m_width = (unsigned)g_w; s.m_height = (unsigned)g_h;
        s.m_fps = (unsigned short)g_fps;
        s.m_outputFile = g_out;
        s.m_doVerticalFlip = true;
        auto enc = std::make_unique<ffmpeg::events::Recorder>();
        auto res = enc->init(s);
        if (res.isOk()) { g_enc = std::move(enc); break; }
        error = res.unwrapErr();
    }
    if (!g_enc) return error;

    g_rt = CCRenderTexture::create(g_w, g_h);
    if (!g_rt) { g_enc->stop(); g_enc.reset(); return "Could not create the render texture"; }
    g_buf.assign((size_t)g_w * g_h * 4, 0);

    // the song, for muxing afterwards
    g_song.clear();
    if (c.renderAudio && pl->m_level) {
        if (pl->m_level->m_songID > 0) g_song = std::string(MusicDownloadManager::sharedState()->pathForSong(pl->m_level->m_songID));
        else g_song = std::string(CCFileUtils::get()->fullPathForFilename(LevelTools::getAudioFileName(pl->m_level->m_audioTrack).c_str(), false));
    }

    g_frames = 0; g_tail = -1;
    g_on = true;
    if (!c.renderMacro.empty()) {
        if (!startPlay(c.renderMacro)) { renderStop(false); return "Cannot play that macro"; }
    } else {
        pl->resetLevelFromStart();
    }
    return {};
#endif
}

void renderNotifyEnd() {
    if (g_on && g_tail < 0) g_tail = std::max(0, cfg().renderTail) * g_fps;
}

void renderStop(bool mixAudio) {
#ifdef XYZ_FFMPEG
    if (!g_on) return;
    g_on = false;
    setMusicVolume(1.f);
    if (g_enc) { g_enc->stop(); g_enc.reset(); }
    g_rt = nullptr;
    g_buf.clear(); g_buf.shrink_to_fit();
    if (g_frames == 0) {
        std::error_code ec; fs::remove(g_out, ec);
        Notification::create("Render stopped: no frames were captured", NotificationIcon::Error)->show();
        return;
    }
    if (mixAudio && cfg().renderAudio && !g_song.empty() && fs::exists(g_song)) {
        auto tmp = g_out; tmp.replace_extension(".mux.mp4");
        std::string failed;
        try {
            auto res = ffmpeg::events::AudioMixer::mixVideoAudio(g_out, g_song, tmp);
            if (res.isErr()) failed = res.unwrapErr();
        } catch (std::exception const& e) { failed = e.what(); }
        std::error_code ec;
        if (failed.empty()) { fs::remove(g_out, ec); fs::rename(tmp, g_out, ec); }
        else { fs::remove(tmp, ec); Notification::create("Rendered without audio: " + failed, NotificationIcon::Warning)->show(); }
    }
    Notification::create("Render saved: " + g_out.filename().string(), NotificationIcon::Success)->show();
#endif
}

void renderCapture() {
#ifdef XYZ_FFMPEG
    if (!g_on || !g_enc || !g_rt) return;
    auto pl = PlayLayer::get();
    if (!pl || pl->m_isPaused) return;
    auto scene = CCDirector::get()->getRunningScene();
    if (!scene) return;
    setMusicVolume(0.f);

    auto ov = OverlayManager::get();
    bool ovVisible = ov && ov->isVisible();
    if (ov) ov->setVisible(false);          // keep the bot's own buttons out of the video
    g_rt->beginWithClear(0, 0, 0, 1);
    scene->visit();
    glReadPixels(0, 0, g_w, g_h, GL_RGBA, GL_UNSIGNED_BYTE, g_buf.data());
    g_rt->end();
    if (ov) ov->setVisible(ovVisible);

    auto res = g_enc->writeFrame(g_buf);
    if (res.isErr()) {
        Notification::create("Render failed: " + res.unwrapErr(), NotificationIcon::Error)->show();
        renderStop(false);
        return;
    }
    ++g_frames;
    if (g_tail >= 0 && --g_tail < 0) renderStop(true);
#endif
}
}

class $modify(XyzRenderDirector, CCDirector) {
    void drawScene() {
        CCDirector::drawScene();
        xyz::renderCapture();
    }
};
