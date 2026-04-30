#include "Encoder.h"
#include "Logger.h"
#include <algorithm>
#include <cstring>

extern "C" {
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
}

namespace {
    constexpr const char* CODEC_H264         = "libx264";
    constexpr const char* CODEC_MPEG4        = "mpeg4";
    constexpr const char* OPT_PRESET         = "preset";
    constexpr const char* OPT_PRESET_VAL     = "medium";
    constexpr const char* OPT_CRF            = "crf";
    constexpr const char* OPT_CRF_VAL        = "23";

    constexpr int VIDEO_TIMEBASE_DEN         = 12800;
    constexpr int VIDEO_GOP_SIZE             = 12;
    constexpr int AUDIO_SWR_BUF_SAMPLES      = 8192;
    constexpr int AUDIO_FALLBACK_FRAME_SIZE  = 4096;
}

Encoder::Encoder() {
    m_packet = av_packet_alloc();
}

Encoder::~Encoder() {
    if (m_open) { close(); }
    av_packet_free(&m_packet);
}

bool Encoder::open(const std::string& outputPath, int width, int height,
                   AVRational srcVideoTimeBase, int sampleRate, int channels,
                   int videoBitRateKbps, int audioBitRateKbps, bool preferH264) {
    m_srcVideoTimeBase = srcVideoTimeBase;
    m_outputPath       = outputPath;
    m_videoBitRate     = videoBitRateKbps * 1000;
    m_audioBitRate     = audioBitRateKbps * 1000;
    m_preferH264       = preferH264;

    if (!m_muxer.open(outputPath)) {
        return false;
    }

    if (!initVideoStream(width, height)) { cleanup(); return false; }

    if (sampleRate > 0 && channels > 0) {
        if (!initAudioStream(sampleRate, channels)) { cleanup(); return false; }
    }

    if (!m_muxer.beginFile()) { cleanup(); return false; }

    m_open = true;
    Logger::instance().info("Encoder: opened " + outputPath);
    return true;
}

bool Encoder::initVideoStream(int width, int height) {
    // When H264 is preferred, try libx264 first — avoids h264_mf which rejects
    // YUV420P on some Windows systems. Fall through to MPEG4 if unavailable.
    const AVCodec* codec = nullptr;
    if (m_preferH264) { codec = avcodec_find_encoder_by_name(CODEC_H264); }
    if (!codec) { codec = avcodec_find_encoder_by_name(CODEC_MPEG4); }
    if (!codec) { codec = avcodec_find_encoder(AV_CODEC_ID_MPEG4); }
    if (!codec) {
        Logger::instance().error("Encoder: no suitable video codec found");
        return false;
    }

    m_videoCtx = avcodec_alloc_context3(codec);
    m_videoCtx->codec_id  = codec->id;
    m_videoCtx->bit_rate  = m_videoBitRate;
    m_videoCtx->width     = width;
    m_videoCtx->height    = height;
    m_videoCtx->time_base = {1, VIDEO_TIMEBASE_DEN};
    m_videoCtx->framerate = {0, 1};
    m_videoCtx->gop_size  = VIDEO_GOP_SIZE;
    m_videoCtx->pix_fmt   = AV_PIX_FMT_YUV420P;
    // MPEG4 warns when given consecutive B-frames; disable them explicitly.
    if (codec->id == AV_CODEC_ID_MPEG4) {
        m_videoCtx->max_b_frames = 0;
    }

    if (m_muxer.needsGlobalHeader()) {
        m_videoCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    AVDictionary* opts = nullptr;
    if (codec->id == AV_CODEC_ID_H264) {
        av_dict_set(&opts, OPT_PRESET, OPT_PRESET_VAL, 0);
        av_dict_set(&opts, OPT_CRF,    OPT_CRF_VAL,    0);
    }
    if (avcodec_open2(m_videoCtx, codec, &opts) < 0) {
        av_dict_free(&opts);
        Logger::instance().error("Encoder: failed to open video codec");
        return false;
    }
    av_dict_free(&opts);

    m_videoStream = m_muxer.addStream(m_videoCtx);
    if (!m_videoStream) {
        return false;
    }

    Logger::instance().info(
        "Encoder: video — " + std::string(codec->name) +
        " " + std::to_string(width) + "x" + std::to_string(height) +
        " @ " + std::to_string(m_videoBitRate / 1000) + " kbps");
    return true;
}

bool Encoder::initAudioStream(int sampleRate, int channels) {
    const AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_AAC);
    if (!codec) {
        Logger::instance().error("Encoder: AAC codec not found");
        return false;
    }

    m_audioCtx = avcodec_alloc_context3(codec);
    m_audioCtx->sample_fmt  = AV_SAMPLE_FMT_FLTP;
    m_audioCtx->bit_rate    = m_audioBitRate;
    m_audioCtx->sample_rate = sampleRate;
    m_audioCtx->time_base   = {1, sampleRate};

    if (channels >= 2) {
        AVChannelLayout stereo = AV_CHANNEL_LAYOUT_STEREO;
        av_channel_layout_copy(&m_audioCtx->ch_layout, &stereo);
    } else {
        AVChannelLayout mono = AV_CHANNEL_LAYOUT_MONO;
        av_channel_layout_copy(&m_audioCtx->ch_layout, &mono);
    }

    if (m_muxer.needsGlobalHeader()) {
        m_audioCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    if (avcodec_open2(m_audioCtx, codec, nullptr) < 0) {
        Logger::instance().error("Encoder: failed to open audio codec");
        return false;
    }

    m_audioStream = m_muxer.addStream(m_audioCtx);
    if (!m_audioStream) {
        return false;
    }

    Logger::instance().info(
        "Encoder: audio — " + std::string(codec->name) +
        " " + std::to_string(sampleRate) + " Hz" +
        " " + std::to_string(channels) + " ch" +
        " @ " + std::to_string(m_audioBitRate / 1000) + " kbps");

    AVChannelLayout srcLayout = {};
    AVChannelLayout dstLayout = {};
    if (channels >= 2) {
        AVChannelLayout stereo = AV_CHANNEL_LAYOUT_STEREO;
        av_channel_layout_copy(&srcLayout, &stereo);
        av_channel_layout_copy(&dstLayout, &stereo);
    } else {
        AVChannelLayout mono = AV_CHANNEL_LAYOUT_MONO;
        av_channel_layout_copy(&srcLayout, &mono);
        av_channel_layout_copy(&dstLayout, &mono);
    }

    swr_alloc_set_opts2(&m_swrCtx,
        &dstLayout, AV_SAMPLE_FMT_FLTP, sampleRate,
        &srcLayout, AV_SAMPLE_FMT_S16,  sampleRate,
        0, nullptr);
    av_channel_layout_uninit(&srcLayout);
    av_channel_layout_uninit(&dstLayout);

    if (swr_init(m_swrCtx) < 0) {
        Logger::instance().error("Encoder: failed to initialize audio resampler");
        return false;
    }

    int frameSize = m_audioCtx->frame_size > 0 ? m_audioCtx->frame_size : AUDIO_FALLBACK_FRAME_SIZE;
    m_audioFifo = av_audio_fifo_alloc(AV_SAMPLE_FMT_FLTP, channels, frameSize * 2);
    if (!m_audioFifo) { return false; }

    m_swrFrame = av_frame_alloc();
    m_swrFrame->format      = AV_SAMPLE_FMT_FLTP;
    m_swrFrame->sample_rate = sampleRate;
    m_swrFrame->nb_samples  = AUDIO_SWR_BUF_SAMPLES;
    av_channel_layout_copy(&m_swrFrame->ch_layout, &m_audioCtx->ch_layout);
    av_frame_get_buffer(m_swrFrame, 0);

    m_encFrame = av_frame_alloc();
    m_encFrame->format      = AV_SAMPLE_FMT_FLTP;
    m_encFrame->sample_rate = sampleRate;
    m_encFrame->nb_samples  = frameSize;
    av_channel_layout_copy(&m_encFrame->ch_layout, &m_audioCtx->ch_layout);
    av_frame_get_buffer(m_encFrame, 0);

    return true;
}

bool Encoder::writeVideoFrame(AVFrame* frame) {
    if (!m_videoCtx) { return false; }
    if (!frame) { return encodeAndWrite(m_videoCtx, nullptr, m_videoStream); }

    AVFrame* copy = av_frame_clone(frame);
    copy->pts       = av_rescale_q(frame->pts, m_srcVideoTimeBase, m_videoCtx->time_base);
    copy->pict_type = AV_PICTURE_TYPE_NONE;
    bool ok = encodeAndWrite(m_videoCtx, copy, m_videoStream);
    av_frame_free(&copy);
    return ok;
}

bool Encoder::writeAudioFrame(AVFrame* frame) {
    if (!m_audioCtx) { return true; }
    if (!frame) { return drainAudioFifo(true); }

    int outSamples = av_rescale_rnd(
        swr_get_delay(m_swrCtx, m_audioCtx->sample_rate) + frame->nb_samples,
        m_audioCtx->sample_rate, m_audioCtx->sample_rate, AV_ROUND_UP);

    int converted = swr_convert(m_swrCtx,
        m_swrFrame->data, std::min(outSamples, AUDIO_SWR_BUF_SAMPLES),
        (const uint8_t**)frame->data, frame->nb_samples);
    if (converted > 0) {
        av_audio_fifo_write(m_audioFifo, (void**)m_swrFrame->data, converted);
    }
    return drainAudioFifo(false);
}

bool Encoder::drainAudioFifo(bool flush) {
    if (!m_audioCtx || !m_audioFifo) { return true; }
    int frameSize = m_audioCtx->frame_size > 0 ? m_audioCtx->frame_size : AUDIO_FALLBACK_FRAME_SIZE;

    while (av_audio_fifo_size(m_audioFifo) >= (flush ? 1 : frameSize)) {
        int toRead = std::min(av_audio_fifo_size(m_audioFifo), frameSize);
        av_frame_make_writable(m_encFrame);
        // Zero the frame so any partial flush frame is padded with silence.
        for (int ch = 0; ch < m_encFrame->ch_layout.nb_channels; ++ch) {
            memset(m_encFrame->data[ch], 0, frameSize * sizeof(float));
        }
        av_audio_fifo_read(m_audioFifo, (void**)m_encFrame->data, toRead);
        m_encFrame->nb_samples = frameSize;
        m_encFrame->pts        = m_audioSampleCount;
        m_audioSampleCount    += toRead;

        if (!encodeAndWrite(m_audioCtx, m_encFrame, m_audioStream)) { return false; }
    }
    return true;
}

bool Encoder::encodeAndWrite(AVCodecContext* ctx, AVFrame* frame, AVStream* stream) {
    if (avcodec_send_frame(ctx, frame) < 0) {
        return frame == nullptr; // nullptr is a flush signal; EAGAIN during flush is normal
    }
    while (avcodec_receive_packet(ctx, m_packet) >= 0) {
        av_packet_rescale_ts(m_packet, ctx->time_base, stream->time_base);
        m_packet->stream_index = stream->index;
        m_muxer.writePacket(m_packet);
        av_packet_unref(m_packet);
    }
    return true;
}

bool Encoder::close() {
    if (!m_open) { return false; }

    if (m_videoCtx) { encodeAndWrite(m_videoCtx, nullptr, m_videoStream); }
    if (m_audioCtx) {
        drainAudioFifo(true);
        encodeAndWrite(m_audioCtx, nullptr, m_audioStream);
    }

    m_muxer.endFile();
    Logger::instance().info("Encoder: finalized " + m_outputPath);

    cleanup();
    m_open = false;
    return true;
}

void Encoder::cleanup() {
    if (m_audioFifo) { av_audio_fifo_free(m_audioFifo); m_audioFifo = nullptr; }
    if (m_swrFrame)  { av_frame_free(&m_swrFrame); }
    if (m_encFrame)  { av_frame_free(&m_encFrame); }
    if (m_swrCtx)    { swr_free(&m_swrCtx); }
    if (m_videoCtx)  { avcodec_free_context(&m_videoCtx); }
    if (m_audioCtx)  { avcodec_free_context(&m_audioCtx); }
    m_muxer.close();
    m_videoStream = nullptr;
    m_audioStream = nullptr;
}
