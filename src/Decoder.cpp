#include "Decoder.h"
#include "Constants.h"
#include <iostream>

// Allocate the reusable packet and frame once here rather than per-call to
// avoid repeated heap allocation in the decode hot path.
Decoder::Decoder() {
    m_packet = av_packet_alloc();
    m_frame  = av_frame_alloc();
}

Decoder::~Decoder() {
    close();
    av_packet_free(&m_packet);
    av_frame_free(&m_frame);
}

bool Decoder::open(const std::string& filePath) {
    if (avformat_open_input(&m_fmtCtx, filePath.c_str(), nullptr, nullptr) < 0) {
        std::cerr << "Failed to open file: " << filePath << "\n";
        return false;
    }

    // Reads enough packets to populate stream metadata (codec, resolution, etc.).
    if (avformat_find_stream_info(m_fmtCtx, nullptr) < 0) {
        std::cerr << "Failed to find stream info\n";
        return false;
    }

    m_videoStreamIdx = av_find_best_stream(m_fmtCtx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    m_audioStreamIdx = av_find_best_stream(m_fmtCtx, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);

    if (m_videoStreamIdx < 0) {
        std::cerr << "No video stream found\n";
        return false;
    }

    // Audio is optional — files without an audio stream are still valid.
    return initVideoCodec() && (m_audioStreamIdx < 0 || initAudioCodec());
}

bool Decoder::initVideoCodec() {
    AVStream* stream = m_fmtCtx->streams[m_videoStreamIdx];
    const AVCodec* codec = avcodec_find_decoder(stream->codecpar->codec_id);
    
    if (!codec) {
        std::cerr << "Unsupported video codec\n";
        return false;
    }

    m_codecCtxVideo = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(m_codecCtxVideo, stream->codecpar);

    if (avcodec_open2(m_codecCtxVideo, codec, nullptr) < 0) {
        std::cerr << "Failed to open video codec\n";
        return false;
    }

    return true;
}

bool Decoder::initAudioCodec() {
    AVStream* stream = m_fmtCtx->streams[m_audioStreamIdx];
    const AVCodec* codec = avcodec_find_decoder(stream->codecpar->codec_id);
    
    if (!codec) {
        return false;
    } 

    m_codecCtxAudio = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(m_codecCtxAudio, stream->codecpar);

    if (avcodec_open2(m_codecCtxAudio, codec, nullptr) < 0) {
        return false;
    } 

    // Resample decoded audio to S16 interleaved stereo at 44100 Hz so the
    // renderer always receives a consistent format regardless of the source.
    AVChannelLayout stereo = AV_CHANNEL_LAYOUT_STEREO;
    swr_alloc_set_opts2(&m_swrCtx,
        &stereo,                        // out layout
        AV_SAMPLE_FMT_S16,              // out format
        SAMPLE_RATE_44K,              // out sample rate
        &m_codecCtxAudio->ch_layout,    // in layout
        m_codecCtxAudio->sample_fmt,    // in format
        m_codecCtxAudio->sample_rate,   // in sample rate
        0, nullptr);
    return swr_init(m_swrCtx) >= 0;
}

void Decoder::close() {
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

    if (m_fmtCtx) { 
        avformat_close_input(&m_fmtCtx); 
    }
}

// FFmpeg decoding is a two-step pipeline: send a compressed packet to the
// codec, then receive one or more decoded frames. A single packet does not
// always produce exactly one frame (e.g. B-frames, multi-frame audio chunks),
// so the outer loop keeps reading packets until a frame comes out.
bool Decoder::readFrame(DecodedFrame& out) {
    out.videoFrame = nullptr;
    out.audioFrame = nullptr;

    while (av_read_frame(m_fmtCtx, m_packet) >= 0) {
        AVCodecContext* ctx = nullptr;
        bool isVideo = (m_packet->stream_index == m_videoStreamIdx);
        bool isAudio = (m_packet->stream_index == m_audioStreamIdx);

        if (isVideo) {
            ctx = m_codecCtxVideo;
        }
        else if (isAudio) {
            ctx = m_codecCtxAudio;
        }

        // Skip packets from unrelated streams (subtitles, data tracks, etc.).
        if (!ctx) { 
            av_packet_unref(m_packet); continue; 
        }

        if (avcodec_send_packet(ctx, m_packet) < 0) {
            av_packet_unref(m_packet);
            continue;
        }
        av_packet_unref(m_packet);

        if (avcodec_receive_frame(ctx, m_frame) >= 0) {
            AVStream* stream = m_fmtCtx->streams[isVideo ? m_videoStreamIdx : m_audioStreamIdx];

            // Convert the frame's PTS from stream time_base units to seconds.
            out.pts = m_frame->pts * av_q2d(stream->time_base);

            if (isVideo) {
                // Clone the frame so the caller owns its own reference; the
                // shared m_frame is unref'd below and reused on the next call.
                out.videoFrame = av_frame_clone(m_frame);
            } else {
                // swr_get_delay accounts for samples already buffered inside the
                // resampler from previous calls, ensuring we allocate enough
                // output space and don't drop audio at the tail of each chunk.
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
    }
    return false;
}

bool Decoder::seek(double seconds) {
    // AVSEEK_FLAG_BACKWARD seeks to the nearest keyframe at or before the
    // target timestamp; seeking forward to a non-keyframe is not possible
    // without decoding everything in between.
    int64_t ts = static_cast<int64_t>(seconds * AV_TIME_BASE);

    if (av_seek_frame(m_fmtCtx, -1, ts, AVSEEK_FLAG_BACKWARD) < 0)  {
        return false;
    }

    // Flush codec internal buffers so stale frames from before the seek
    // don't bleed into the output after the jump.
    if (m_codecCtxVideo) {
        avcodec_flush_buffers(m_codecCtxVideo);
    }
    
    if (m_codecCtxAudio) {
        avcodec_flush_buffers(m_codecCtxAudio);
    }

    // Drain any samples buffered inside the resampler.
    if (m_swrCtx) {
        swr_convert(m_swrCtx, nullptr, 0, nullptr, 0);
    }

    return true;
}

double Decoder::duration() const {
    if (!m_fmtCtx) {
        return 0.0;
    }

    // m_fmtCtx->duration is in AV_TIME_BASE units (microseconds).
    return static_cast<double>(m_fmtCtx->duration) / AV_TIME_BASE;
}
