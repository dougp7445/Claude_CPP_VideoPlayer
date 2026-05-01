#include "Muxer.h"
#include "Logger.h"

Muxer::Muxer() = default;

Muxer::~Muxer() {
    close();
}

bool Muxer::open(const std::string& outputPath) {
    m_outputPath = outputPath;
    if (avformat_alloc_output_context2(&m_fmtCtx, nullptr, nullptr, outputPath.c_str()) < 0) {
        Logger::instance().error("Muxer: failed to allocate output context for " + outputPath);
        return false;
    }
    return true;
}

bool Muxer::needsGlobalHeader() const {
    return m_fmtCtx && (m_fmtCtx->oformat->flags & AVFMT_GLOBALHEADER);
}

AVStream* Muxer::addStream(AVCodecContext* codecCtx) {
    AVStream* stream = avformat_new_stream(m_fmtCtx, nullptr);
    if (!stream) {
        return nullptr;
    }
    avcodec_parameters_from_context(stream->codecpar, codecCtx);
    stream->time_base = codecCtx->time_base;
    return stream;
}

bool Muxer::beginFile() {
    if (!(m_fmtCtx->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&m_fmtCtx->pb, m_outputPath.c_str(), AVIO_FLAG_WRITE) < 0) {
            Logger::instance().error("Muxer: failed to open output file " + m_outputPath);
            return false;
        }
    }
    if (avformat_write_header(m_fmtCtx, nullptr) < 0) {
        Logger::instance().error("Muxer: failed to write container header");
        return false;
    }
    return true;
}

bool Muxer::startAsync(LockingQueue<AVPacket*>& queue) {
    m_queue = &queue;
    m_writeThread = std::thread([this, &queue] {
        AVPacket* pkt = nullptr;
        while (queue.pop(pkt)) {
            writePacket(pkt);
            av_packet_free(&pkt);
        }
    });
    return true;
}

bool Muxer::writePacket(AVPacket* packet) {
    return av_interleaved_write_frame(m_fmtCtx, packet) >= 0;
}

bool Muxer::endFile() {
    if (!m_fmtCtx) {
        return false;
    }
    // Drain the write thread before writing the trailer.
    if (m_writeThread.joinable()) { m_writeThread.join(); }
    av_write_trailer(m_fmtCtx);
    if (!(m_fmtCtx->oformat->flags & AVFMT_NOFILE)) {
        avio_closep(&m_fmtCtx->pb);
    }
    return true;
}

void Muxer::close() {
    // Ensure the queue is closed so the write thread can exit, then join it.
    if (m_queue) { m_queue->close(); }
    if (m_writeThread.joinable()) { m_writeThread.join(); }
    if (m_fmtCtx) {
        // On error paths endFile() may not have run, so close pb if still open.
        if (!(m_fmtCtx->oformat->flags & AVFMT_NOFILE) && m_fmtCtx->pb) {
            avio_closep(&m_fmtCtx->pb);
        }
        avformat_free_context(m_fmtCtx);
        m_fmtCtx = nullptr;
    }
    m_queue = nullptr;
}
