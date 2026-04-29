#ifndef DECODER_H
#define DECODER_H

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/frame.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}

#include <string>

struct DecodedFrame {
    AVFrame* videoFrame = nullptr;
    AVFrame* audioFrame = nullptr;
    double pts = 0.0;
};

class Decoder {
public:
    Decoder();
    ~Decoder();

    bool open(const std::string& filePath);
    void close();

    bool readFrame(DecodedFrame& out);
    bool seek(double seconds);

    int videoWidth()  const { return m_codecCtxVideo ? m_codecCtxVideo->width  : 0; }
    int videoHeight() const { return m_codecCtxVideo ? m_codecCtxVideo->height : 0; }
    double duration() const;

    AVRational videoTimeBase() const;
    AVRational audioTimeBase() const;
    bool hasAudio() const { return m_audioStreamIdx >= 0; }

private:
    AVFormatContext* m_fmtCtx        = nullptr;
    AVCodecContext*  m_codecCtxVideo = nullptr;
    AVCodecContext*  m_codecCtxAudio = nullptr;
    SwsContext*      m_swsCtx        = nullptr;
    SwrContext*      m_swrCtx        = nullptr;

    int m_videoStreamIdx = -1;
    int m_audioStreamIdx = -1;

    AVPacket* m_packet = nullptr;
    AVFrame*  m_frame  = nullptr;

    bool initVideoCodec();
    bool initAudioCodec();
};

#endif // DECODER_H
