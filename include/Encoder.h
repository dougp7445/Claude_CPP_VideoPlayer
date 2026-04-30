#ifndef ENCODER_H
#define ENCODER_H

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/audio_fifo.h>
#include <libswresample/swresample.h>
}

#include <string>
#include "Muxer.h"

class Encoder {
public:
    Encoder();
    ~Encoder();

    // Register codec streams with an already-opened Muxer, then open the
    // codecs. The caller must call muxer.beginFile() after this returns true
    // and before the first writeVideoFrame/writeAudioFrame call.
    // Pass sampleRate=0 to produce a video-only output.
    bool open(Muxer& muxer, int width, int height,
              AVRational srcVideoTimeBase, int sampleRate = 44100, int channels = 2,
              int videoBitRateKbps = 2000, int audioBitRateKbps = 128,
              bool preferH264 = true);

    // Write a YUV420P video frame. Pass nullptr to flush the video encoder.
    bool writeVideoFrame(AVFrame* frame);

    // Write an S16 interleaved stereo audio frame. Pass nullptr to flush.
    bool writeAudioFrame(AVFrame* frame);

    // Flush all codecs and send remaining packets to the muxer.
    // The caller is responsible for calling muxer.endFile() afterwards.
    bool close();
    bool isOpen() const { return m_open; }

private:
    bool initVideoStream(int width, int height);
    bool initAudioStream(int sampleRate, int channels);
    bool encodeAndWrite(AVCodecContext* ctx, AVFrame* frame, AVStream* stream);
    bool drainAudioFifo(bool flush);
    void cleanup();

    Muxer*           m_muxer       = nullptr;  // non-owning
    AVCodecContext*  m_videoCtx    = nullptr;
    AVCodecContext*  m_audioCtx    = nullptr;
    AVStream*        m_videoStream = nullptr;
    AVStream*        m_audioStream = nullptr;
    AVPacket*        m_packet      = nullptr;
    SwrContext*      m_swrCtx      = nullptr;
    AVAudioFifo*     m_audioFifo   = nullptr;
    AVFrame*         m_swrFrame    = nullptr;
    AVFrame*         m_encFrame    = nullptr;
    AVRational       m_srcVideoTimeBase = {1, 1};
    int64_t          m_audioSampleCount = 0;
    int              m_videoBitRate     = 2000000;
    int              m_audioBitRate     = 128000;
    bool             m_preferH264       = true;
    bool             m_open             = false;
};

#endif // ENCODER_H
