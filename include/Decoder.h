#ifndef DECODER_H
#define DECODER_H

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
#include <libavutil/rational.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}

struct DecodedFrame {
    AVFrame* videoFrame = nullptr;
    AVFrame* audioFrame = nullptr;
    double pts = 0.0;
};

class Decoder {
public:
    Decoder();
    ~Decoder();

    // Initialize codecs from demuxer-supplied parameters and time bases.
    // Pass nullptr for audioParams to produce a video-only decoder.
    bool open(AVCodecParameters* videoParams, AVCodecParameters* audioParams,
              AVRational videoTimeBase, AVRational audioTimeBase);
    void close();

    // Flush codec and resampler buffers after a seek.
    void flushBuffers();

    // Feed one packet from the demuxer. Returns true if a decoded frame is
    // ready in `out`; returns false when the codec needs more packets (e.g.
    // B-frames) or on error. isVideo must match the packet's stream type.
    bool decode(const AVPacket* packet, bool isVideo, DecodedFrame& out);

    int videoWidth()  const { return m_codecCtxVideo ? m_codecCtxVideo->width  : 0; }
    int videoHeight() const { return m_codecCtxVideo ? m_codecCtxVideo->height : 0; }

private:
    AVCodecContext* m_codecCtxVideo = nullptr;
    AVCodecContext* m_codecCtxAudio = nullptr;
    SwsContext*     m_swsCtx        = nullptr;
    SwrContext*     m_swrCtx        = nullptr;
    AVFrame*        m_frame         = nullptr;
    AVRational      m_videoTimeBase = {0, 1};
    AVRational      m_audioTimeBase = {0, 1};

    bool initVideoCodec(AVCodecParameters* par);
    bool initAudioCodec(AVCodecParameters* par);
};

#endif // DECODER_H
