#ifndef VIDEO_ENCODER_H
#define VIDEO_ENCODER_H

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

#include <thread>
#include "LockingQueue.h"

class VideoEncoder {
public:
    VideoEncoder();
    ~VideoEncoder();

    // Open the video codec. srcTimeBase is the time base of incoming frames.
    bool open(int width, int height, AVRational srcTimeBase,
              int bitRateKbps = 2000, bool preferH264 = true,
              bool needsGlobalHeader = false);

    // Extract time_base and index from a muxer-supplied stream before writing.
    void setStream(AVStream* stream) {
        if (stream) { m_streamTimeBase = stream->time_base; m_streamIndex = stream->index; }
    }

    AVCodecContext* codecContext() const { return m_ctx; }

    // Write a YUV420P frame. Pass nullptr to flush.
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
    bool encodeAndWrite(AVFrame* frame);
    void cleanup();

    LockingQueue<AVPacket*> m_outputQueue;
    std::thread             m_encodeThread;
    AVCodecContext*         m_ctx            = nullptr;
    AVPacket*               m_packet         = nullptr;
    AVRational              m_srcTimeBase    = {1, 1};
    AVRational              m_streamTimeBase = {1, 1};
    int                     m_streamIndex    = -1;
    int                     m_bitRate        = 2000000;
    bool                    m_preferH264     = true;
    bool                    m_open           = false;
};

#endif // VIDEO_ENCODER_H
