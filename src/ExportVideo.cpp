#include "ExportVideo.h"
#include "Decoder.h"
#include "Demuxer.h"
#include "Encoder.h"
#include "Logger.h"
#include "Muxer.h"
#include <cstdint>

ExportVideo::~ExportVideo() {
    cancel();
    wait();
}

void ExportVideo::start(const std::string& inputPath,
                        const std::string& outputPath,
                        const EncoderSettings& settings) {
    m_cancel   = false;
    m_done     = false;
    m_progress = 0.0f;
    m_thread = std::thread([this, inputPath, outputPath, settings] {
        run(inputPath, outputPath, settings);
    });
}

void ExportVideo::cancel() {
    m_cancel = true;
}

void ExportVideo::wait() {
    if (m_thread.joinable()) { m_thread.join(); }
}

void ExportVideo::run(const std::string& inputPath,
                      const std::string& outputPath,
                      const EncoderSettings& settings) {
    Demuxer demuxer;
    if (!demuxer.open(inputPath)) {
        Logger::instance().error("ExportVideo: failed to open demuxer for " + inputPath);
        m_done = true;
        return;
    }

    Decoder decoder;
    if (!decoder.open(demuxer.videoCodecParameters(),
                      demuxer.audioCodecParameters(),
                      demuxer.videoTimeBase(),
                      demuxer.audioTimeBase())) {
        Logger::instance().error("ExportVideo: failed to open decoder for " + inputPath);
        m_done = true;
        return;
    }

    double exportStart = static_cast<double>(settings.exportStartTime);
    if (!demuxer.seek(exportStart)) {
        Logger::instance().error("ExportVideo: failed to seek to start");
        m_done = true;
        return;
    }
    decoder.flushBuffers();

    int  sampleRate = demuxer.hasAudio() ? 44100 : 0;
    int  channels   = demuxer.hasAudio() ? 2 : 0;
    bool preferH264 = settings.videoCodec == EncoderSettings::VideoCodec::H264;

    LockingQueue<AVPacket*>    rawPktQueue;
    LockingQueue<DecodedFrame> frameQueue;
    LockingQueue<AVPacket*>    packetQueue;

    Muxer muxer;
    if (!muxer.open(outputPath)) {
        Logger::instance().error("ExportVideo: failed to open muxer for " + outputPath);
        m_done = true;
        return;
    }

    Encoder encoder;
    if (!encoder.open(muxer, packetQueue,
                      decoder.videoWidth(), decoder.videoHeight(),
                      demuxer.videoTimeBase(),
                      sampleRate, channels,
                      settings.videoBitRateKbps, settings.audioBitRateKbps,
                      preferH264)) {
        Logger::instance().error("ExportVideo: failed to open encoder for " + outputPath);
        m_done = true;
        return;
    }

    if (!muxer.beginFile()) {
        Logger::instance().error("ExportVideo: failed to begin muxer file for " + outputPath);
        m_done = true;
        return;
    }

    muxer.startAsync(packetQueue);
    encoder.startAsync(frameQueue);

    double duration    = demuxer.duration();
    double exportLimit = (settings.exportDuration > 0.0f)
                         ? exportStart + static_cast<double>(settings.exportDuration)
                         : duration;
    double clipLength  = exportLimit - exportStart;

    AVRational vtb = demuxer.videoTimeBase();
    int64_t videoPtsOffset = static_cast<int64_t>(exportStart * vtb.den / vtb.num);

    decoder.startAsync(demuxer, rawPktQueue, frameQueue,
        [&](DecodedFrame& frame) -> bool {
            if (clipLength > 0.0) {
                float p = static_cast<float>((frame.pts - exportStart) / clipLength);
                m_progress = p < 0.0f ? 0.0f : (p < 1.0f ? p : 1.0f);
            }
            if (settings.exportDuration > 0.0f && frame.pts >= exportLimit) {
                av_frame_free(&frame.videoFrame);
                av_frame_free(&frame.audioFrame);
                return false;
            }
            // Skip pre-roll frames that arrive before exportStart.
            if (frame.pts < exportStart) {
                av_frame_free(&frame.videoFrame);
                av_frame_free(&frame.audioFrame);
                return true;
            }
            if (frame.videoFrame) {
                frame.videoFrame->pts -= videoPtsOffset;
                if (frame.videoFrame->pts < 0) { frame.videoFrame->pts = 0; }
            }
            frameQueue.push(frame);
            frame.videoFrame = nullptr;
            frame.audioFrame = nullptr;
            return !m_cancel.load();
        });

    AVPacket* pkt = av_packet_alloc();
    while (!m_cancel) {
        if (!demuxer.readPacket(pkt)) { break; }
        AVPacket* copy = av_packet_alloc();
        av_packet_move_ref(copy, pkt);
        rawPktQueue.push(copy);
    }
    av_packet_free(&pkt);

    rawPktQueue.close();
    encoder.close();
    decoder.waitDone();
    muxer.endFile();

    if (!m_cancel) {
        m_progress = 1.0f;
        Logger::instance().info("ExportVideo: export complete — " + outputPath);
    }
    m_done = true;
}
