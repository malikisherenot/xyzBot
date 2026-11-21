#if XYZ_LIBAV
#include "Encoder.hpp"
#include <Geode/Geode.hpp>
#include <deque>
#include <mutex>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/audio_fifo.h>
#include <libavutil/channel_layout.h>
#include <libavutil/log.h>
#include <libavutil/opt.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
#if XYZ_MEDIACODEC
#include <libavcodec/jni.h>
#endif
}
#if XYZ_MEDIACODEC
#include <Geode/cocos/platform/android/jni/JniHelper.h>
#endif

namespace fs = std::filesystem;

namespace xyz::enc {
namespace {

std::mutex g_logMx;
std::deque<std::string> g_tail; // newest FFmpeg warnings and errors

void logCb(void* avcl, int level, char const* fmt, va_list vl) {
    if (level > AV_LOG_WARNING) return;
    char line[1024];
    int prefix = 1;
    av_log_format_line(avcl, level, fmt, vl, line, sizeof line, &prefix);
    std::string s = line;
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
    if (s.empty()) return;
    geode::log::warn("[ffmpeg] {}", s);
    std::lock_guard g(g_logMx);
    g_tail.push_back(std::move(s));
    if (g_tail.size() > 40) g_tail.pop_front();
}

void setup() {
    static std::once_flag once;
    std::call_once(once, [] {
        av_log_set_callback(logCb);
#if XYZ_MEDIACODEC
        av_jni_set_java_vm(cocos2d::JniHelper::getJavaVM(), nullptr);
#endif
    });
}

std::string lastLine() {
    std::lock_guard g(g_logMx);
    return g_tail.empty() ? std::string() : " (" + g_tail.back() + ")";
}

std::string avs(int e) {
    char b[AV_ERROR_MAX_STRING_SIZE] = {};
    av_strerror(e, b, sizeof b);
    return b;
}

std::string path8(fs::path const& p) { return geode::utils::string::pathToString(p); }

#define AVCHK(expr, what) do { int r_ = (expr); if (r_ < 0) return std::string(what) + ": " + avs(r_) + lastLine(); } while (0)

} // namespace

struct Recorder::Impl {
    AVFormatContext* fc = nullptr;
    AVCodecContext* cc = nullptr;
    AVStream* st = nullptr;
    SwsContext* sws = nullptr;
    AVFrame* frame = nullptr;
    AVPacket* pkt = nullptr;
    int w = 0, h = 0;
    int64_t pts = 0;
    bool headerWritten = false, finished = false;

    ~Impl() { release(); }

    void release() {
        if (sws) { sws_freeContext(sws); sws = nullptr; }
        av_frame_free(&frame);
        av_packet_free(&pkt);
        avcodec_free_context(&cc);
        if (fc) {
            if (fc->pb && !(fc->oformat->flags & AVFMT_NOFILE)) avio_closep(&fc->pb);
            avformat_free_context(fc);
            fc = nullptr;
        }
    }

    std::string receive() {
        for (;;) {
            int r = avcodec_receive_packet(cc, pkt);
            if (r == AVERROR(EAGAIN) || r == AVERROR_EOF) return {};
            AVCHK(r, "receive packet");
            av_packet_rescale_ts(pkt, cc->time_base, st->time_base);
            pkt->stream_index = st->index;
            AVCHK(av_interleaved_write_frame(fc, pkt), "write packet");
        }
    }

    std::string send(AVFrame* f) {
        AVCHK(avcodec_send_frame(cc, f), "send frame");
        return receive();
    }
};

Recorder::Recorder() : m(std::make_unique<Impl>()) {}
Recorder::~Recorder() = default;

std::string Recorder::init(Settings const& s) {
    setup();
    auto& d = *m;
    auto codec = avcodec_find_encoder_by_name(s.codec.c_str());
    if (!codec) return "Encoder not built in: " + s.codec;
    d.w = s.width; d.h = s.height;
    auto file = path8(s.out);
    AVCHK(avformat_alloc_output_context2(&d.fc, nullptr, "mp4", file.c_str()), "output");
    d.st = avformat_new_stream(d.fc, nullptr);
    d.cc = avcodec_alloc_context3(codec);
    d.pkt = av_packet_alloc();
    if (!d.st || !d.cc || !d.pkt) return "Out of memory";

    auto& c = *d.cc;
    c.width = s.width; c.height = s.height;
    c.time_base = {1, s.fps}; c.framerate = {s.fps, 1};
    c.pix_fmt = AV_PIX_FMT_YUV420P;
    c.bit_rate = s.bitrate;
    c.gop_size = std::max(1, s.fps);
    c.max_b_frames = 0;
    c.colorspace = AVCOL_SPC_BT709;
    c.color_range = AVCOL_RANGE_MPEG;
    if (d.fc->oformat->flags & AVFMT_GLOBALHEADER) c.flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    AVDictionary* opts = nullptr;
    if (s.codec == "libx264") av_dict_set(&opts, "preset", "veryfast", 0);
    int r = avcodec_open2(d.cc, codec, &opts);
    av_dict_free(&opts);
    AVCHK(r, "open " + s.codec);
    AVCHK(avcodec_parameters_from_context(d.st->codecpar, d.cc), "codec parameters");
    d.st->time_base = c.time_base;
    AVCHK(avio_open(&d.fc->pb, file.c_str(), AVIO_FLAG_WRITE), "create " + s.out.filename().string());
    AVCHK(avformat_write_header(d.fc, nullptr), "write header");
    d.headerWritten = true;

    d.frame = av_frame_alloc();
    d.frame->format = AV_PIX_FMT_YUV420P;
    d.frame->width = s.width; d.frame->height = s.height;
    AVCHK(av_frame_get_buffer(d.frame, 0), "frame buffer");

    d.sws = sws_getContext(s.width, s.height, AV_PIX_FMT_RGBA, s.width, s.height, AV_PIX_FMT_YUV420P, SWS_BILINEAR, nullptr, nullptr, nullptr);
    if (!d.sws) return "Could not create the color converter";
    // RGB (full range) to BT.709 limited range
    sws_setColorspaceDetails(d.sws, sws_getCoefficients(SWS_CS_DEFAULT), 1, sws_getCoefficients(SWS_CS_ITU709), 0, 0, 1 << 16, 1 << 16);
    return {};
}

std::string Recorder::writeFrame(std::vector<unsigned char> const& rgba) {
    auto& d = *m;
    if (!d.fc || d.finished) return "Encoder is not running";
    if (rgba.size() < (size_t)d.w * d.h * 4) return "Frame has the wrong size";
    AVCHK(av_frame_make_writable(d.frame), "frame buffer");
    // the frame is bottom-up: start at the last row and walk upwards
    uint8_t const* src[4] = {rgba.data() + (size_t)(d.h - 1) * d.w * 4, nullptr, nullptr, nullptr};
    int stride[4] = {-d.w * 4, 0, 0, 0};
    sws_scale(d.sws, src, stride, 0, d.h, d.frame->data, d.frame->linesize);
    d.frame->pts = d.pts++;
    return d.send(d.frame);
}

std::string Recorder::stop() {
    auto& d = *m;
    if (!d.fc || d.finished) return {};
    d.finished = true;
    std::string err = d.cc ? d.send(nullptr) : std::string();
    if (d.headerWritten) {
        int r = av_write_trailer(d.fc);
        if (r < 0 && err.empty()) err = "write trailer: " + avs(r) + lastLine();
    }
    d.release();
    return err;
}

std::vector<std::string> availableCodecs() {
    setup();
    std::vector<std::string> v;
    for (auto n : {"h264_nvenc", "h264_amf", "h264_qsv", "h264_videotoolbox", "h264_mediacodec", "libx264", "libopenh264", "mpeg4"})
        if (avcodec_find_encoder_by_name(n)) v.push_back(n);
    return v;
}

// ---- audio: decode the song, resample to 44.1 kHz stereo, encode AAC, merge with the copied video packets

namespace {
struct Mixer {
    AVFormatContext *vf = nullptr, *af = nullptr, *of = nullptr;
    AVCodecContext *ad = nullptr, *ae = nullptr;
    SwrContext* swr = nullptr;
    AVAudioFifo* fifo = nullptr;
    AVFrame *fr = nullptr, *ef = nullptr;
    AVPacket* pk = nullptr;
    std::vector<AVPacket*> apk;
    ~Mixer() {
        for (auto p : apk) av_packet_free(&p);
        av_packet_free(&pk);
        av_frame_free(&fr); av_frame_free(&ef);
        if (fifo) av_audio_fifo_free(fifo);
        swr_free(&swr);
        avcodec_free_context(&ad); avcodec_free_context(&ae);
        avformat_close_input(&vf); avformat_close_input(&af);
        if (of) {
            if (of->pb && !(of->oformat->flags & AVFMT_NOFILE)) avio_closep(&of->pb);
            avformat_free_context(of);
        }
    }
};
constexpr int RATE = 44100;
}

std::string mixVideoAudio(fs::path const& video, fs::path const& audio, fs::path const& out) {
    setup();
    Mixer x;
    auto vfile = path8(video), afile = path8(audio), ofile = path8(out);
    AVCHK(avformat_open_input(&x.vf, vfile.c_str(), nullptr, nullptr), "open video");
    AVCHK(avformat_find_stream_info(x.vf, nullptr), "read video");
    AVCHK(avformat_open_input(&x.af, afile.c_str(), nullptr, nullptr), "open audio");
    AVCHK(avformat_find_stream_info(x.af, nullptr), "read audio");
    int vi = av_find_best_stream(x.vf, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    int ai = av_find_best_stream(x.af, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if (vi < 0) return "The video has no video stream";
    if (ai < 0) return "The song has no audio stream";
    AVStream* vin = x.vf->streams[vi];
    AVStream* ain = x.af->streams[ai];

    auto dec = avcodec_find_decoder(ain->codecpar->codec_id);
    if (!dec) return "Song format is not built in";
    x.ad = avcodec_alloc_context3(dec);
    AVCHK(avcodec_parameters_to_context(x.ad, ain->codecpar), "audio parameters");
    AVCHK(avcodec_open2(x.ad, dec, nullptr), "open audio decoder");

    auto aenc = avcodec_find_encoder(AV_CODEC_ID_AAC);
    if (!aenc) return "AAC encoder is not built in";
    x.ae = avcodec_alloc_context3(aenc);
    x.ae->sample_rate = RATE;
    av_channel_layout_default(&x.ae->ch_layout, 2);
    x.ae->sample_fmt = AV_SAMPLE_FMT_FLTP;
    x.ae->bit_rate = 192000;
    x.ae->time_base = {1, RATE};

    AVCHK(avformat_alloc_output_context2(&x.of, nullptr, "mp4", ofile.c_str()), "output");
    if (x.of->oformat->flags & AVFMT_GLOBALHEADER) x.ae->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    AVCHK(avcodec_open2(x.ae, aenc, nullptr), "open AAC");

    AVStream* vs = avformat_new_stream(x.of, nullptr);
    AVStream* as = avformat_new_stream(x.of, nullptr);
    if (!vs || !as) return "Out of memory";
    AVCHK(avcodec_parameters_copy(vs->codecpar, vin->codecpar), "video parameters");
    vs->codecpar->codec_tag = 0;
    vs->time_base = vin->time_base;
    vs->avg_frame_rate = vin->avg_frame_rate;
    AVCHK(avcodec_parameters_from_context(as->codecpar, x.ae), "AAC parameters");
    as->time_base = x.ae->time_base;

    AVChannelLayout inLayout = x.ad->ch_layout;
    if (inLayout.nb_channels == 0) av_channel_layout_default(&inLayout, 2);
    AVCHK(swr_alloc_set_opts2(&x.swr, &x.ae->ch_layout, AV_SAMPLE_FMT_FLTP, RATE, &inLayout, x.ad->sample_fmt, x.ad->sample_rate, 0, nullptr), "resampler");
    AVCHK(swr_init(x.swr), "resampler");
    x.fifo = av_audio_fifo_alloc(AV_SAMPLE_FMT_FLTP, 2, 1);
    x.fr = av_frame_alloc(); x.ef = av_frame_alloc(); x.pk = av_packet_alloc();
    if (!x.fifo || !x.fr || !x.ef || !x.pk) return "Out of memory";

    double vdur = vin->duration != AV_NOPTS_VALUE ? (double)vin->duration * av_q2d(vin->time_base)
                : x.vf->duration != AV_NOPTS_VALUE ? (double)x.vf->duration / AV_TIME_BASE : 1e9;
    int64_t limit = (int64_t)(vdur * RATE) + 1, fed = 0, apts = 0;

    // ---- encode the audio into memory (a few MB at most)
    auto encodeFifo = [&](bool flush) -> std::string {
        int fsz = x.ae->frame_size > 0 ? x.ae->frame_size : 1024;
        while (av_audio_fifo_size(x.fifo) >= fsz || (flush && av_audio_fifo_size(x.fifo) > 0)) {
            int n = std::min(fsz, av_audio_fifo_size(x.fifo));
            av_frame_unref(x.ef);
            x.ef->nb_samples = n;
            x.ef->format = AV_SAMPLE_FMT_FLTP;
            x.ef->sample_rate = RATE;
            av_channel_layout_copy(&x.ef->ch_layout, &x.ae->ch_layout);
            AVCHK(av_frame_get_buffer(x.ef, 0), "audio buffer");
            if (av_audio_fifo_read(x.fifo, (void**)x.ef->data, n) < n) return "Could not read the audio buffer";
            x.ef->pts = apts; apts += n;
            AVCHK(avcodec_send_frame(x.ae, x.ef), "encode audio");
            for (;;) {
                int r = avcodec_receive_packet(x.ae, x.pk);
                if (r == AVERROR(EAGAIN) || r == AVERROR_EOF) break;
                AVCHK(r, "encode audio");
                x.apk.push_back(av_packet_clone(x.pk));
                av_packet_unref(x.pk);
            }
        }
        return {};
    };
    auto resample = [&](AVFrame* f) -> std::string {
        int inN = f ? f->nb_samples : 0;
        int maxOut = swr_get_out_samples(x.swr, inN);
        if (maxOut <= 0) return {};
        uint8_t** buf = nullptr; int ls = 0;
        AVCHK(av_samples_alloc_array_and_samples(&buf, &ls, 2, maxOut, AV_SAMPLE_FMT_FLTP, 0), "resample");
        int n = swr_convert(x.swr, buf, maxOut, f ? (uint8_t const**)f->extended_data : nullptr, inN);
        if (n > 0) {
            n = (int)std::min<int64_t>(n, limit - fed);
            if (n > 0) { av_audio_fifo_write(x.fifo, (void**)buf, n); fed += n; }
        }
        av_freep(&buf[0]); av_freep(&buf);
        return n < 0 ? "resample failed" : std::string();
    };
    auto drainDecoder = [&]() -> std::string {
        for (;;) {
            int r = avcodec_receive_frame(x.ad, x.fr);
            if (r == AVERROR(EAGAIN) || r == AVERROR_EOF) return {};
            AVCHK(r, "decode audio");
            auto e = resample(x.fr);
            av_frame_unref(x.fr);
            if (!e.empty()) return e;
            if (auto e2 = encodeFifo(false); !e2.empty()) return e2;
        }
    };
    while (fed < limit && av_read_frame(x.af, x.pk) >= 0) {
        if (x.pk->stream_index == ai) {
            int r = avcodec_send_packet(x.ad, x.pk);
            av_packet_unref(x.pk);
            if (r < 0 && r != AVERROR(EAGAIN)) return "decode audio: " + avs(r) + lastLine();
            if (auto e = drainDecoder(); !e.empty()) return e;
        } else av_packet_unref(x.pk);
    }
    avcodec_send_packet(x.ad, nullptr);
    if (auto e = drainDecoder(); !e.empty()) return e;
    if (auto e = resample(nullptr); !e.empty()) return e;
    if (auto e = encodeFifo(true); !e.empty()) return e;
    avcodec_send_frame(x.ae, nullptr);
    for (;;) {
        int r = avcodec_receive_packet(x.ae, x.pk);
        if (r < 0) break;
        x.apk.push_back(av_packet_clone(x.pk));
        av_packet_unref(x.pk);
    }

    // ---- write: video packets are copied, audio packets go in at the right time
    AVCHK(avio_open(&x.of->pb, ofile.c_str(), AVIO_FLAG_WRITE), "create output");
    AVCHK(avformat_write_header(x.of, nullptr), "write header");
    size_t next = 0;
    auto writeAudio = [&](double until) -> std::string {
        while (next < x.apk.size()) {
            auto p = x.apk[next];
            if ((double)p->dts * av_q2d(x.ae->time_base) > until) break;
            av_packet_rescale_ts(p, x.ae->time_base, as->time_base);
            p->stream_index = as->index;
            AVCHK(av_interleaved_write_frame(x.of, p), "write audio");
            ++next;
        }
        return {};
    };
    while (av_read_frame(x.vf, x.pk) >= 0) {
        if (x.pk->stream_index != vi) { av_packet_unref(x.pk); continue; }
        int64_t ts = x.pk->dts != AV_NOPTS_VALUE ? x.pk->dts : x.pk->pts;
        if (auto e = writeAudio((double)ts * av_q2d(vin->time_base)); !e.empty()) return e;
        av_packet_rescale_ts(x.pk, vin->time_base, vs->time_base);
        x.pk->stream_index = vs->index;
        AVCHK(av_interleaved_write_frame(x.of, x.pk), "write video");
    }
    if (auto e = writeAudio(1e18); !e.empty()) return e;
    AVCHK(av_write_trailer(x.of), "write trailer");
    return {};
}

} // namespace xyz::enc
#endif
