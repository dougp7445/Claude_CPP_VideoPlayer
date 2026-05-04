#include "AudioEncoder.h"
#include "Constants.h"
#include "Logger.h"
#include <algorithm>
#include <cstring>

extern "C" {
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
}

AudioEncoder::AudioEncoder() {
    m_packet = av_packet_alloc();
}

AudioEncoder::~AudioEncoder() {
    if (m_open) { close(); }
    av_packet_free(&m_packet);
}

bool AudioEncoder::open(int sampleRate, int channels, int bitRateKbps,
                        bool needsGlobalHeader) {
    m_bitRate = bitRateKbps * KBPS_TO_BPS;

    const AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_AAC);
    if (!codec) {
        Logger::instance().error("AudioEncoder: AAC codec not found");
        return false;
    }

    m_ctx = avcodec_alloc_context3(codec);
    m_ctx->sample_fmt  = AV_SAMPLE_FMT_FLTP;
    m_ctx->bit_rate    = m_bitRate;
    m_ctx->sample_rate = sampleRate;
    m_ctx->time_base   = {1, sampleRate};

    if (channels >= CHANNELS_STEREO) {
        AVChannelLayout stereo = AV_CHANNEL_LAYOUT_STEREO;
        av_channel_layout_copy(&m_ctx->ch_layout, &stereo);
    } else {
        AVChannelLayout mono = AV_CHANNEL_LAYOUT_MONO;
        av_channel_layout_copy(&m_ctx->ch_layout, &mono);
    }

    if (needsGlobalHeader) {
        m_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    if (avcodec_open2(m_ctx, codec, nullptr) < 0) {
        Logger::instance().error("AudioEncoder: failed to open codec");
        cleanup();
        return false;
    }

    Logger::instance().info(
        "AudioEncoder: " + std::string(codec->name) +
        " " + std::to_string(sampleRate) + " Hz" +
        " " + std::to_string(channels) + " ch" +
        " @ " + std::to_string(m_bitRate / KBPS_TO_BPS) + " kbps");

    AVChannelLayout srcLayout = {};
    AVChannelLayout dstLayout = {};
    if (channels >= CHANNELS_STEREO) {
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
        Logger::instance().error("AudioEncoder: failed to initialize resampler");
        cleanup();
        return false;
    }

    int frameSize = m_ctx->frame_size > 0 ? m_ctx->frame_size : AUDIO_FALLBACK_FRAME_SIZE;
    m_fifo = av_audio_fifo_alloc(AV_SAMPLE_FMT_FLTP, channels, frameSize * AUDIO_FIFO_HEADROOM_FRAMES);
    if (!m_fifo) { cleanup(); return false; }

    m_swrFrame = av_frame_alloc();
    m_swrFrame->format      = AV_SAMPLE_FMT_FLTP;
    m_swrFrame->sample_rate = sampleRate;
    m_swrFrame->nb_samples  = AUDIO_SWR_BUF_SAMPLES;
    av_channel_layout_copy(&m_swrFrame->ch_layout, &m_ctx->ch_layout);
    av_frame_get_buffer(m_swrFrame, 0);

    m_encFrame = av_frame_alloc();
    m_encFrame->format      = AV_SAMPLE_FMT_FLTP;
    m_encFrame->sample_rate = sampleRate;
    m_encFrame->nb_samples  = frameSize;
    av_channel_layout_copy(&m_encFrame->ch_layout, &m_ctx->ch_layout);
    av_frame_get_buffer(m_encFrame, 0);

    m_open = true;
    return true;
}

bool AudioEncoder::writeFrame(AVFrame* frame) {
    if (!m_ctx) { return true; }
    if (!frame) { return drainFifo(true); }

    int outSamples = av_rescale_rnd(
        swr_get_delay(m_swrCtx, m_ctx->sample_rate) + frame->nb_samples,
        m_ctx->sample_rate, m_ctx->sample_rate, AV_ROUND_UP);

    int converted = swr_convert(m_swrCtx,
        m_swrFrame->data, std::min(outSamples, AUDIO_SWR_BUF_SAMPLES),
        (const uint8_t**)frame->data, frame->nb_samples);
    if (converted > 0) {
        av_audio_fifo_write(m_fifo, (void**)m_swrFrame->data, converted);
    }
    return drainFifo(false);
}

bool AudioEncoder::drainFifo(bool flush) {
    if (!m_ctx || !m_fifo) { return true; }
    int frameSize = m_ctx->frame_size > 0 ? m_ctx->frame_size : AUDIO_FALLBACK_FRAME_SIZE;

    while (av_audio_fifo_size(m_fifo) >= (flush ? 1 : frameSize)) {
        int toRead = std::min(av_audio_fifo_size(m_fifo), frameSize);
        Logger::instance().trace(
            "AudioEncoder: draining FIFO toRead=" + std::to_string(toRead) +
            " remaining=" + std::to_string(av_audio_fifo_size(m_fifo)));
        av_frame_make_writable(m_encFrame);
        for (int ch = 0; ch < m_encFrame->ch_layout.nb_channels; ++ch) {
            memset(m_encFrame->data[ch], 0, frameSize * sizeof(float));
        }
        av_audio_fifo_read(m_fifo, (void**)m_encFrame->data, toRead);
        m_encFrame->nb_samples = frameSize;
        m_encFrame->pts        = m_sampleCount;
        m_sampleCount         += toRead;

        if (!encodeAndWrite(m_encFrame)) { return false; }
    }
    return true;
}

bool AudioEncoder::startAsync(LockingQueue<AVFrame*>& frameQueue) {
    Logger::instance().debug("AudioEncoder: starting async encode thread");
    m_encodeThread = std::thread([this, &frameQueue] {
        AVFrame* frame = nullptr;
        while (frameQueue.pop(frame)) {
            writeFrame(frame);
            av_frame_free(&frame);
        }
        drainFifo(true);
        encodeAndWrite(nullptr);
        m_outputQueue.close();
        Logger::instance().debug("AudioEncoder: encode thread finished");
    });
    return true;
}

bool AudioEncoder::close() {
    if (!m_open) { return false; }
    Logger::instance().debug("AudioEncoder: closing, flushing codec");
    if (m_encodeThread.joinable()) {
        m_encodeThread.join();
    } else {
        drainFifo(true);
        encodeAndWrite(nullptr);
        m_outputQueue.close();
    }
    cleanup();
    m_open = false;
    return true;
}

bool AudioEncoder::encodeAndWrite(AVFrame* frame) {
    if (avcodec_send_frame(m_ctx, frame) < 0) {
        if (frame != nullptr) {
            Logger::instance().error("AudioEncoder: failed to send frame to codec");
        }
        return frame == nullptr;
    }
    while (avcodec_receive_packet(m_ctx, m_packet) >= 0) {
        Logger::instance().trace(
            "AudioEncoder: packet pts=" + std::to_string(m_packet->pts) +
            " size=" + std::to_string(m_packet->size));
        av_packet_rescale_ts(m_packet, m_ctx->time_base, m_streamTimeBase);
        m_packet->stream_index = m_streamIndex;
        AVPacket* copy = av_packet_alloc();
        av_packet_move_ref(copy, m_packet);
        m_outputQueue.push(copy);
    }
    return true;
}

void AudioEncoder::cleanup() {
    if (m_fifo)     { av_audio_fifo_free(m_fifo); m_fifo = nullptr; }
    if (m_swrFrame) { av_frame_free(&m_swrFrame); }
    if (m_encFrame) { av_frame_free(&m_encFrame); }
    if (m_swrCtx)   { swr_free(&m_swrCtx); }
    if (m_ctx)      { avcodec_free_context(&m_ctx); }
    m_streamTimeBase = {1, 1};
    m_streamIndex    = -1;
}
