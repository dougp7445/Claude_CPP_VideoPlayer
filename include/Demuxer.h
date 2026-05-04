#ifndef DEMUXER_H
#define DEMUXER_H

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/rational.h>
}

#include "LockingQueue.h"
#include <string>

class Demuxer {
public:
    Demuxer();
    ~Demuxer();

    bool open(const std::string& filePath);
    void close();

    bool readPacket(AVPacket* packet);
    bool seek(double seconds);

    bool isVideoPacket(const AVPacket* packet) const;
    bool isAudioPacket(const AVPacket* packet) const;

    int  videoStreamIndex() const { return m_videoStreamIdx; }
    int  audioStreamIndex() const { return m_audioStreamIdx; }
    bool hasVideo() const { return m_videoStreamIdx >= 0; }
    bool hasAudio() const { return m_audioStreamIdx >= 0; }

    double     duration()        const;
    AVRational videoTimeBase()   const;
    AVRational audioTimeBase()   const;

    AVCodecParameters* videoCodecParameters() const;
    AVCodecParameters* audioCodecParameters() const;

    LockingQueue<AVPacket*>& outputQueue() { return m_outputQueue; }

private:
    AVFormatContext*        m_fmtCtx         = nullptr;
    int                     m_videoStreamIdx = -1;
    int                     m_audioStreamIdx = -1;
    LockingQueue<AVPacket*> m_outputQueue;
};

#endif // DEMUXER_H
