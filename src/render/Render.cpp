// Video rendering; frames are handed to eclipse.ffmpeg-api the way Echo (GPL-3.0) does it.
#include "../core/State.hpp"
#include "../vis/Vis.hpp"
#include <chrono>
#include <Geode/modify/CCDirector.hpp>
#include <Geode/ui/OverlayManager.hpp>
#include <Geode/binding/FMODAudioEngine.hpp>
#include <Geode/binding/GameManager.hpp>
#include <Geode/binding/MusicDownloadManager.hpp>
#include <Geode/binding/LevelTools.hpp>
#include <condition_variable>
#include <deque>
#include <filesystem>
#include <mutex>
#include <thread>

#if XYZ_FFMPEG_API && __has_include(<eclipse.ffmpeg-api/include/events.hpp>)
#include <eclipse.ffmpeg-api/include/events.hpp>
#define XYZ_FFMPEG 1
#endif

using namespace geode::prelude;
namespace fs = std::filesystem;

namespace {
bool g_on = false;
int g_tail = -1;
long long g_frames = 0;
std::chrono::steady_clock::time_point g_t0;
bool g_vsyncWas = true;
int g_w = 0, g_h = 0, g_fps = 60;
fs::path g_out, g_song;
Ref<CCRenderTexture> g_rt;
#ifdef XYZ_FFMPEG
std::unique_ptr<ffmpeg::events::Recorder> g_enc;

constexpr size_t MAX_QUEUED = 4, MAX_SPARE = 2;
constexpr int RING = 4;

struct Pipe {
    std::thread th;
    std::mutex mx;
    std::condition_variable cv;
    std::deque<std::vector<unsigned char>> queue;
    std::vector<std::vector<unsigned char>> spare;
    bool closing = false, broken = false;
    std::string failure;
};
std::unique_ptr<Pipe> g_pipe;

#ifdef GEODE_IS_WINDOWS
GLuint g_pbo[RING] = {};
GLsync g_fence[RING] = {};
std::deque<int> g_inflight;
int g_next = 0;
bool g_pboOk = false;
#endif

size_t frameBytes() { return (size_t)g_w * g_h * 4; }

std::vector<unsigned char> acquireFrame() {
    std::vector<unsigned char> f;
    {
        std::lock_guard g(g_pipe->mx);
        if (!g_pipe->spare.empty()) { f = std::move(g_pipe->spare.back()); g_pipe->spare.pop_back(); }
    }
    f.resize(frameBytes());
    return f;
}

void queueFrame(std::vector<unsigned char> f) {
    std::unique_lock g(g_pipe->mx);
    g_pipe->cv.wait(g, [] { return g_pipe->queue.size() < MAX_QUEUED || g_pipe->broken; });
    if (g_pipe->broken) return;
    g_pipe->queue.push_back(std::move(f));
    g_pipe->cv.notify_all();
}

void encodeLoop(Pipe* p) {
    for (;;) {
        std::vector<unsigned char> f;
        {
            std::unique_lock g(p->mx);
            p->cv.wait(g, [p] { return p->closing || !p->queue.empty(); });
            if (p->queue.empty()) return;
            f = std::move(p->queue.front());
            p->queue.pop_front();
        }
        p->cv.notify_all();
        if (!p->broken) {
            auto res = g_enc->writeFrame(f);
            if (res.isErr()) {
                std::lock_guard g(p->mx);
                p->failure = res.unwrapErr();
                p->broken = true;
                p->cv.notify_all();
            }
        }
        std::lock_guard g(p->mx);
        if (p->spare.size() < MAX_SPARE) p->spare.push_back(std::move(f));
    }
}

void startPipe() {
    g_pipe = std::make_unique<Pipe>();
    g_pipe->th = std::thread(encodeLoop, g_pipe.get());
}

void stopPipe() {
    if (!g_pipe) return;
    {
        std::lock_guard g(g_pipe->mx);
        g_pipe->closing = true;
    }
    g_pipe->cv.notify_all();
    if (g_pipe->th.joinable()) g_pipe->th.join();
    g_pipe.reset();
}

void fail(std::string const& why) {
    if (!g_pipe) return;
    std::lock_guard g(g_pipe->mx);
    if (!g_pipe->broken) g_pipe->failure = why;
    g_pipe->broken = true;
    g_pipe->cv.notify_all();
}

#ifdef GEODE_IS_WINDOWS
bool collect(bool wait) {
    if (g_inflight.empty()) return false;
    int slot = g_inflight.front();
    GLenum r = glClientWaitSync(g_fence[slot], wait ? GL_SYNC_FLUSH_COMMANDS_BIT : 0, wait ? 5'000'000'000ull : 0);
    if (!wait && r != GL_ALREADY_SIGNALED && r != GL_CONDITION_SATISFIED) return false;
    glDeleteSync(g_fence[slot]);
    g_fence[slot] = nullptr;
    g_inflight.pop_front();
    if (r == GL_TIMEOUT_EXPIRED || r == GL_WAIT_FAILED) { fail("GPU readback timed out"); return true; }
    glBindBuffer(GL_PIXEL_PACK_BUFFER, g_pbo[slot]);
    if (auto data = static_cast<unsigned char const*>(glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY))) {
        auto f = acquireFrame();
        std::memcpy(f.data(), data, f.size());
        if (glUnmapBuffer(GL_PIXEL_PACK_BUFFER)) queueFrame(std::move(f));
        else fail("The captured frame buffer was invalid");
    } else fail("Could not read the captured frame buffer");
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    return true;
}

void openRing() {
    g_pboOk = false;
    glGenBuffers(RING, g_pbo);
    bool ok = true;
    for (auto b : g_pbo) {
        glBindBuffer(GL_PIXEL_PACK_BUFFER, b);
        glBufferData(GL_PIXEL_PACK_BUFFER, (GLsizeiptr)frameBytes(), nullptr, GL_STREAM_READ);
        GLint size = 0;
        glGetBufferParameteriv(GL_PIXEL_PACK_BUFFER, GL_BUFFER_SIZE, &size);
        ok = ok && b && (size_t)size == frameBytes();
    }
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    if (!ok) { glDeleteBuffers(RING, g_pbo); for (auto& b : g_pbo) b = 0; return; }
    for (auto& f : g_fence) f = nullptr;
    g_inflight.clear();
    g_next = 0;
    g_pboOk = true;
}

void closeRing() {
    if (g_pboOk) {
        while (collect(true)) {}
        glDeleteBuffers(RING, g_pbo);
        for (auto& b : g_pbo) b = 0;
    }
    g_pboOk = false;
}

void readIntoRing() {
    int slot = g_next;
    while (g_fence[slot] && collect(true)) {}
    glBindBuffer(GL_PIXEL_PACK_BUFFER, g_pbo[slot]);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, g_w, g_h, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    g_fence[slot] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    g_inflight.push_back(slot);
    g_next = (slot + 1) % RING;
    while (collect(false)) {}
}
#endif

void readDirect() {
    auto f = acquireFrame();
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, g_w, g_h, GL_RGBA, GL_UNSIGNED_BYTE, f.data());
    queueFrame(std::move(f));
}
#endif

fs::path renderDir() {
    auto p = Mod::get()->getSaveDir() / "renders";
    std::error_code ec; fs::create_directories(p, ec);
    return p;
}

void setVsync(bool on) {
#ifdef GEODE_IS_WINDOWS
    CCApplication::sharedApplication()->toggleVerticalSync(on);
#else
    (void)on;
#endif
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
RenderStats renderStats() {
    RenderStats s;
    if (!g_on) return s;
    s.on = true; s.frames = g_frames; s.fps = g_fps;
    s.realSec = std::chrono::duration<double>(std::chrono::steady_clock::now() - g_t0).count();
    s.videoSec = (double)g_frames / std::max(1, g_fps);
    return s;
}

std::string startRender() {
#ifndef XYZ_FFMPEG
    return "This build has no FFmpeg API support";
#else
    if (g_on) return "Already rendering";
    if (!renderAvailable()) return "Install the FFmpeg API mod (eclipse.ffmpeg-api)";
    auto pl = PlayLayer::get();
    if (!pl) return "Open a level first";
    auto& c = cfg();

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
    auto want = [&](std::string n) { if (std::find(order.begin(), order.end(), n) == order.end()) order.push_back(std::move(n)); };
    if (c.renderCodec > 0 && c.renderCodec < 5) want(CODECS[c.renderCodec]);
#ifdef GEODE_IS_ANDROID
    want("h264_mediacodec");
#endif
    for (auto n : {"h264_nvenc", "h264_amf", "h264_qsv", "h264_videotoolbox", "libx264", "h264_mediacodec", "libopenh264", "mpeg4"}) want(n);

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
    startPipe();
#ifdef GEODE_IS_WINDOWS
    openRing();
#endif

    g_song.clear();
    if (c.renderAudio && pl->m_level) {
        if (pl->m_level->m_songID > 0) g_song = std::string(MusicDownloadManager::sharedState()->pathForSong(pl->m_level->m_songID));
        else g_song = std::string(CCFileUtils::get()->fullPathForFilename(LevelTools::getAudioFileName(pl->m_level->m_audioTrack).c_str(), false));
    }

    g_frames = 0; g_tail = -1;
    g_t0 = std::chrono::steady_clock::now();
    g_on = true;
    g_vsyncWas = GameManager::get()->getGameVariable("0030");
    setVsync(false);
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
    setVsync(g_vsyncWas);
    setMusicVolume(1.f);
#ifdef GEODE_IS_WINDOWS
    closeRing();
#endif
    std::string failure;
    if (g_pipe) { std::lock_guard g(g_pipe->mx); failure = g_pipe->failure; }
    stopPipe();
    if (g_enc) { g_enc->stop(); g_enc.reset(); }
    g_rt = nullptr;
    if (!failure.empty()) {
        Notification::create("Render failed: " + failure, NotificationIcon::Error)->show();
        return;
    }
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
    if (!g_on || !g_enc || !g_rt || !g_pipe) return;
    bool broken;
    { std::lock_guard g(g_pipe->mx); broken = g_pipe->broken; }
    if (broken) { renderStop(false); return; }
    auto pl = PlayLayer::get();
    if (!pl || pl->m_isPaused) return;
    auto scene = CCDirector::get()->getRunningScene();
    if (!scene) return;
    setMusicVolume(0.f);

    auto ov = OverlayManager::get();
    bool ovVisible = ov && ov->isVisible();
    if (ov) ov->setVisible(false);
    labels::captureHide(true);
    g_rt->beginWithClear(0, 0, 0, 1);
    scene->visit();
#ifdef GEODE_IS_WINDOWS
    if (g_pboOk) readIntoRing(); else readDirect();
#else
    readDirect();
#endif
    g_rt->end();
    labels::captureHide(false);
    if (ov) ov->setVisible(ovVisible);
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
