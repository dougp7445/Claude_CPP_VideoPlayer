#include "VideoEncoder.h"
#include "Logger.h"

extern "C" {
#include <libavutil/opt.h>
}

namespace {
    constexpr const char* CODEC_H264         = "libx264";
    constexpr const char* CODEC_MPEG4        = "mpeg4";
    constexpr const char* OPT_PRESET         = "preset";
    constexpr const char* OPT_PRESET_VAL     = "medium";
    constexpr const char* OPT_CRF            = "crf";
    constexpr const char* OPT_CRF_VAL        = "23";
    constexpr int         VIDEO_TIMEBASE_DEN = 12800;
    constexpr int         VIDEO_GOP_SIZE     = 12;
}

VideoEncoder::VideoEncoder() {
    m_packet = av_packet_alloc();
}

VideoEncoder::~VideoEncoder() {
    if (m_open) { close(); }
    av_packet_free(&m_packet);
}

bool VideoEncoder::open(int width, int height, AVRational srcTimeBase,
                        int bitRateKbps, bool preferH264, bool needsGlobalHeader) {
    m_srcTimeBase = srcTimeBase;
    m_bitRate     = bitRateKbps * 1000;
    m_preferH264  = preferH264;

    const AVCodec* codec = nullptr;
    if (m_preferH264) { codec = avcodec_find_encoder_by_name(CODEC_H264); }
    if (!codec) { codec = avcodec_find_encoder_by_name(CODEC_MPEG4); }
    if (!codec) { codec = avcodec_find_encoder(AV_CODEC_ID_MPEG4); }
    if (!codec) {
        Logger::instance().error("VideoEncoder: no suitable codec found");
        return false;
    }

    m_ctx = avcodec_alloc_context3(codec);
    m_ctx->codec_id  = codec->id;
    m_ctx->bit_rate  = m_bitRate;
    m_ctx->width     = width;
    m_ctx->height    = height;
    m_ctx->time_base = {1, VIDEO_TIMEBASE_DEN};
    m_ctx->framerate = {0, 1};
    m_ctx->gop_size  = VIDEO_GOP_SIZE;
    m_ctx->pix_fmt   = AV_PIX_FMT_YUV420P;
    if (codec->id == AV_CODEC_ID_MPEG4) {
        m_ctx->max_b_frames = 0;
    }
    if (needsGlobalHeader) {
        m_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    AVDictionary* opts = nullptr;
    if (codec->id == AV_CODEC_ID_H264) {
        av_dict_set(&opts, OPT_PRESET, OPT_PRESET_VAL, 0);
        av_dict_set(&opts, OPT_CRF,    OPT_CRF_VAL,    0);
    }
    if (avcodec_open2(m_ctx, codec, &opts) < 0) {
        av_dict_free(&opts);
        Logger::instance().error("VideoEncoder: failed to open codec");
        cleanup();
        return false;
    }
    av_dict_free(&opts);

    Logger::instance().info(
        "VideoEncoder: " + std::string(codec->name) +
        " " + std::to_string(width) + "x" + std::to_string(height) +
        " @ " + std::to_string(m_bitRate / 1000) + " kbps");

    m_open = true;
    return true;
}

bool VideoEncoder::writeFrame(AVFrame* frame) {
    if (!m_ctx) { return false; }
    if (!frame) { return encodeAndWrite(nullptr); }

    AVFrame* copy = av_frame_clone(frame);
    copy->pts       = av_rescale_q(frame->pts, m_srcTimeBase, m_ctx->time_base);
    copy->pict_type = AV_PICTURE_TYPE_NONE;
    bool ok = encodeAndWrite(copy);
    av_frame_free(&copy);
    return ok;
}

bool VideoEncoder::startAsync(LockingQueue<AVFrame*>& frameQueue) {
    m_encodeThread = std::thread([this, &frameQueue] {
        AVFrame* frame = nullptr;
        while (frameQueue.pop(frame)) {
            writeFrame(frame);
            av_frame_free(&frame);
        }
        encodeAndWrite(nullptr);
        m_outputQueue.close();
    });
    return true;
}

bool VideoEncoder::close() {
    if (!m_open) { return false; }
    if (m_encodeThread.joinable()) {
        m_encodeThread.join();
    } else {
        encodeAndWrite(nullptr);
        m_outputQueue.close();
    }
    cleanup();
    m_open = false;
    return true;
}

bool VideoEncoder::encodeAndWrite(AVFrame* frame) {
    if (avcodec_send_frame(m_ctx, frame) < 0) {
        return frame == nullptr;
    }
    while (avcodec_receive_packet(m_ctx, m_packet) >= 0) {
        av_packet_rescale_ts(m_packet, m_ctx->time_base, m_streamTimeBase);
        m_packet->stream_index = m_streamIndex;
        AVPacket* copy = av_packet_alloc();
        av_packet_move_ref(copy, m_packet);
        m_outputQueue.push(copy);
    }
    return true;
}

void VideoEncoder::cleanup() {
    if (m_ctx) { avcodec_free_context(&m_ctx); }
    m_streamTimeBase = {1, 1};
    m_streamIndex    = -1;
}
