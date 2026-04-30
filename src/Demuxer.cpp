#include "Demuxer.h"
#include "Logger.h"

Demuxer::Demuxer() = default;

Demuxer::~Demuxer() {
    close();
}

bool Demuxer::open(const std::string& filePath) {
    if (avformat_open_input(&m_fmtCtx, filePath.c_str(), nullptr, nullptr) < 0) {
        Logger::instance().error("Failed to open file: " + filePath);
        return false;
    }

    if (avformat_find_stream_info(m_fmtCtx, nullptr) < 0) {
        Logger::instance().error("Failed to find stream info");
        return false;
    }

    m_videoStreamIdx = av_find_best_stream(m_fmtCtx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    m_audioStreamIdx = av_find_best_stream(m_fmtCtx, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);

    if (m_videoStreamIdx < 0) {
        Logger::instance().error("No video stream found");
        return false;
    }

    return true;
}

void Demuxer::close() {
    if (m_fmtCtx) {
        avformat_close_input(&m_fmtCtx);
    }
    m_videoStreamIdx = -1;
    m_audioStreamIdx = -1;
}

bool Demuxer::readPacket(AVPacket* packet) {
    while (av_read_frame(m_fmtCtx, packet) >= 0) {
        if (isVideoPacket(packet) || isAudioPacket(packet)) {
            return true;
        }
        av_packet_unref(packet);
    }
    return false;
}

bool Demuxer::seek(double seconds) {
    int64_t ts = static_cast<int64_t>(seconds * AV_TIME_BASE);
    return av_seek_frame(m_fmtCtx, -1, ts, AVSEEK_FLAG_BACKWARD) >= 0;
}

bool Demuxer::isVideoPacket(const AVPacket* packet) const {
    return packet->stream_index == m_videoStreamIdx;
}

bool Demuxer::isAudioPacket(const AVPacket* packet) const {
    return m_audioStreamIdx >= 0 && packet->stream_index == m_audioStreamIdx;
}

double Demuxer::duration() const {
    if (!m_fmtCtx) {
        return 0.0;
    }
    return static_cast<double>(m_fmtCtx->duration) / AV_TIME_BASE;
}

AVRational Demuxer::videoTimeBase() const {
    if (!m_fmtCtx || m_videoStreamIdx < 0) {
        return {0, 1};
    }
    return m_fmtCtx->streams[m_videoStreamIdx]->time_base;
}

AVRational Demuxer::audioTimeBase() const {
    if (!m_fmtCtx || m_audioStreamIdx < 0) {
        return {0, 1};
    }
    return m_fmtCtx->streams[m_audioStreamIdx]->time_base;
}

AVCodecParameters* Demuxer::videoCodecParameters() const {
    if (!m_fmtCtx || m_videoStreamIdx < 0) {
        return nullptr;
    }
    return m_fmtCtx->streams[m_videoStreamIdx]->codecpar;
}

AVCodecParameters* Demuxer::audioCodecParameters() const {
    if (!m_fmtCtx || m_audioStreamIdx < 0) {
        return nullptr;
    }
    return m_fmtCtx->streams[m_audioStreamIdx]->codecpar;
}
