#include "Decoder.h"
#include "Constants.h"
#include "Logger.h"

Decoder::Decoder() {
    m_frame = av_frame_alloc();
}

Decoder::~Decoder() {
    close();
    av_frame_free(&m_frame);
}

bool Decoder::open(AVCodecParameters* videoParams, AVCodecParameters* audioParams,
                   AVRational videoTimeBase, AVRational audioTimeBase) {
    m_videoTimeBase = videoTimeBase;
    m_audioTimeBase = audioTimeBase;

    if (!videoParams || !initVideoCodec(videoParams)) {
        return false;
    }

    return !audioParams || initAudioCodec(audioParams);
}

bool Decoder::initVideoCodec(AVCodecParameters* par) {
    const AVCodec* codec = avcodec_find_decoder(par->codec_id);

    if (!codec) {
        Logger::instance().error("Unsupported video codec");
        return false;
    }

    m_codecCtxVideo = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(m_codecCtxVideo, par);

    if (avcodec_open2(m_codecCtxVideo, codec, nullptr) < 0) {
        Logger::instance().error("Failed to open video codec");
        return false;
    }

    return true;
}

bool Decoder::initAudioCodec(AVCodecParameters* par) {
    const AVCodec* codec = avcodec_find_decoder(par->codec_id);

    if (!codec) {
        return false;
    }

    m_codecCtxAudio = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(m_codecCtxAudio, par);

    if (avcodec_open2(m_codecCtxAudio, codec, nullptr) < 0) {
        return false;
    }

    AVChannelLayout stereo = AV_CHANNEL_LAYOUT_STEREO;
    swr_alloc_set_opts2(&m_swrCtx,
        &stereo,                        // out layout
        AV_SAMPLE_FMT_S16,              // out format
        SAMPLE_RATE_44K,                // out sample rate
        &m_codecCtxAudio->ch_layout,    // in layout
        m_codecCtxAudio->sample_fmt,    // in format
        m_codecCtxAudio->sample_rate,   // in sample rate
        0, nullptr);
    return swr_init(m_swrCtx) >= 0;
}

bool Decoder::startAsync(int videoStreamIndex,
                         LockingQueue<AVPacket*>& packetQueue,
                         std::function<bool(DecodedFrame&)> filter) {
    m_decodeThread = std::thread([this, videoStreamIndex, &packetQueue,
                                  filter = std::move(filter)] {
        bool active = true;
        AVPacket* pkt = nullptr;
        while (packetQueue.pop(pkt)) {
            if (active) {
                bool isVideo = (pkt->stream_index == videoStreamIndex);
                DecodedFrame frame;
                if (decode(pkt, isVideo, frame)) {
                    active = filter(frame);
                    if (active) {
                        if (frame.videoFrame) { m_videoOutputQueue.push(frame.videoFrame); frame.videoFrame = nullptr; }
                        if (frame.audioFrame) { m_audioOutputQueue.push(frame.audioFrame); frame.audioFrame = nullptr; }
                    } else {
                        av_frame_free(&frame.videoFrame);
                        av_frame_free(&frame.audioFrame);
                        m_videoOutputQueue.close();
                        m_audioOutputQueue.close();
                    }
                }
            }
            av_packet_free(&pkt);
        }
        if (active) {
            m_videoOutputQueue.close();
            m_audioOutputQueue.close();
        }
    });
    return true;
}

void Decoder::waitDone() {
    if (m_decodeThread.joinable()) { m_decodeThread.join(); }
}

void Decoder::close() {
    waitDone();

    if (m_swrCtx) {
        swr_free(&m_swrCtx);
    }

    if (m_swsCtx) {
        sws_freeContext(m_swsCtx);
        m_swsCtx = nullptr;
    }

    if (m_codecCtxVideo) {
        avcodec_free_context(&m_codecCtxVideo);
    }

    if (m_codecCtxAudio) {
        avcodec_free_context(&m_codecCtxAudio);
    }
}

void Decoder::flushBuffers() {
    if (m_codecCtxVideo) {
        avcodec_flush_buffers(m_codecCtxVideo);
    }

    if (m_codecCtxAudio) {
        avcodec_flush_buffers(m_codecCtxAudio);
    }

    if (m_swrCtx) {
        swr_convert(m_swrCtx, nullptr, 0, nullptr, 0);
    }
}

bool Decoder::decode(const AVPacket* packet, bool isVideo, DecodedFrame& out) {
    out.videoFrame = nullptr;
    out.audioFrame = nullptr;

    AVCodecContext* ctx = isVideo ? m_codecCtxVideo : m_codecCtxAudio;
    if (!ctx) {
        return false;
    }

    if (avcodec_send_packet(ctx, packet) < 0) {
        return false;
    }

    if (avcodec_receive_frame(ctx, m_frame) < 0) {
        return false;
    }

    AVRational tb = isVideo ? m_videoTimeBase : m_audioTimeBase;
    out.pts = m_frame->pts * av_q2d(tb);

    if (isVideo) {
        if (m_frame->format == AV_PIX_FMT_YUV420P) {
            out.videoFrame = av_frame_clone(m_frame);
        } else {
            m_swsCtx = sws_getCachedContext(m_swsCtx,
                m_frame->width, m_frame->height,
                static_cast<AVPixelFormat>(m_frame->format),
                m_frame->width, m_frame->height,
                AV_PIX_FMT_YUV420P,
                SWS_BILINEAR, nullptr, nullptr, nullptr);

            AVFrame* converted = av_frame_alloc();
            converted->format = AV_PIX_FMT_YUV420P;
            converted->width  = m_frame->width;
            converted->height = m_frame->height;
            converted->pts    = m_frame->pts;
            av_frame_get_buffer(converted, 0);

            sws_scale(m_swsCtx,
                (const uint8_t* const*)m_frame->data, m_frame->linesize,
                0, m_frame->height,
                converted->data, converted->linesize);

            out.videoFrame = converted;
        }
    } else {
        int outSamples = av_rescale_rnd(
            swr_get_delay(m_swrCtx, m_codecCtxAudio->sample_rate) + m_frame->nb_samples,
            SAMPLE_RATE_44K, m_codecCtxAudio->sample_rate, AV_ROUND_UP);

        AVFrame* resampled = av_frame_alloc();
        resampled->format      = AV_SAMPLE_FMT_S16;
        resampled->sample_rate = SAMPLE_RATE_44K;
        resampled->nb_samples  = outSamples;
        AVChannelLayout stereo = AV_CHANNEL_LAYOUT_STEREO;
        av_channel_layout_copy(&resampled->ch_layout, &stereo);
        av_frame_get_buffer(resampled, 0);

        int converted = swr_convert(m_swrCtx,
            resampled->data, outSamples,
            (const uint8_t**)m_frame->data, m_frame->nb_samples);
        resampled->nb_samples = converted;
        out.audioFrame = resampled;
    }

    av_frame_unref(m_frame);
    return true;
}
