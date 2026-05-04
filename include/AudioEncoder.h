#ifndef AUDIO_ENCODER_H
#define AUDIO_ENCODER_H

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/audio_fifo.h>
#include <libswresample/swresample.h>
}

#include <thread>
#include "Constants.h"
#include "LockingQueue.h"

class AudioEncoder {
public:
    AudioEncoder();
    ~AudioEncoder();

    // Open the AAC audio codec. Expects S16 interleaved input frames.
    bool open(int sampleRate, int channels, int bitRateKbps = ENC_DEFAULT_AUDIO_BITRATE_KBPS,
              bool needsGlobalHeader = false);

    // Extract time_base and index from a muxer-supplied stream before writing.
    void setStream(AVStream* stream) {
        if (stream) { m_streamTimeBase = stream->time_base; m_streamIndex = stream->index; }
    }

    AVCodecContext* codecContext() const { return m_ctx; }

    // Write an S16 interleaved audio frame. Pass nullptr to flush.
    bool writeFrame(AVFrame* frame);

    // Start a background thread that drains frameQueue and encodes each frame.
    // Call after open() and setStream(). Thread closes outputQueue when done.
    bool startAsync(LockingQueue<AVFrame*>& frameQueue);

    // Flush the codec and close the output queue.
    // If startAsync() was called, joins the encode thread.
    bool close();
    bool isOpen() const { return m_open; }

    LockingQueue<AVPacket*>& outputQueue() { return m_outputQueue; }

private:
    bool drainFifo(bool flush);
    bool encodeAndWrite(AVFrame* frame);
    void cleanup();

    LockingQueue<AVPacket*> m_outputQueue;
    std::thread             m_encodeThread;
    AVCodecContext*         m_ctx            = nullptr;
    AVPacket*               m_packet         = nullptr;
    SwrContext*             m_swrCtx         = nullptr;
    AVAudioFifo*            m_fifo           = nullptr;
    AVFrame*                m_swrFrame       = nullptr;
    AVFrame*                m_encFrame       = nullptr;
    AVRational              m_streamTimeBase = {1, 1};
    int                     m_streamIndex    = -1;
    int64_t                 m_sampleCount    = 0;
    int                     m_bitRate        = ENC_DEFAULT_AUDIO_BITRATE_KBPS * KBPS_TO_BPS;
    bool                    m_open           = false;
};

#endif // AUDIO_ENCODER_H
