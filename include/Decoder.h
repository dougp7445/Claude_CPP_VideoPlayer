#ifndef DECODER_H
#define DECODER_H

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
#include <libavutil/rational.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}

#include <functional>
#include <thread>
#include "LockingQueue.h"

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

    // Start a background thread that drains packetQueue, decodes each packet,
    // and passes the result to filter. filter receives the DecodedFrame and
    // returns true to continue or false to stop early. When done (queue drained
    // or filter returns false), the thread closes its owned outputQueue.
    bool startAsync(int videoStreamIndex,
                    LockingQueue<AVPacket*>& packetQueue,
                    std::function<bool(DecodedFrame&)> filter);

    // Join the decode thread.
    void waitDone();

    LockingQueue<AVFrame*>& videoOutputQueue() { return m_videoOutputQueue; }
    LockingQueue<AVFrame*>& audioOutputQueue() { return m_audioOutputQueue; }

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

    std::thread            m_decodeThread;
    LockingQueue<AVFrame*> m_videoOutputQueue;
    LockingQueue<AVFrame*> m_audioOutputQueue;
};

#endif // DECODER_H
