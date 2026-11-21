// Own FFmpeg (libav*) video encoder, replaces the eclipse.ffmpeg-api mod. No FFmpeg headers here, so
// the rest of the mod never needs them. Every function returns an error text, empty means success.
#pragma once
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace xyz::enc {

struct Settings {
    std::string codec;
    int width = 0, height = 0, fps = 60;
    long long bitrate = 0;
    std::filesystem::path out;
};

class Recorder {
public:
    Recorder();
    ~Recorder();
    std::string init(Settings const& s);
    // bottom-up RGBA, width * height * 4 bytes (the way glReadPixels gives it)
    std::string writeFrame(std::vector<unsigned char> const& rgba);
    std::string stop();
    struct Impl;
private:
    std::unique_ptr<Impl> m;
};

// H.264 / MPEG-4 encoders that this FFmpeg build really contains
std::vector<std::string> availableCodecs();
// copies the video and adds the audio file as AAC (cut to the video length)
std::string mixVideoAudio(std::filesystem::path const& video, std::filesystem::path const& audio, std::filesystem::path const& out);

}
