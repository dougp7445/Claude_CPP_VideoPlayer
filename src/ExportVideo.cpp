#include "ExportVideo.h"
#include "AudioEncoder.h"
#include "Decoder.h"
#include "Demuxer.h"
#include "Logger.h"
#include "Muxer.h"
#include "VideoEncoder.h"
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

    bool hasAudio   = demuxer.hasAudio();
    int  sampleRate = hasAudio ? 44100 : 0;
    int  channels   = hasAudio ? 2 : 0;
    bool preferH264 = settings.videoCodec == EncoderSettings::VideoCodec::H264;

    Muxer muxer;
    if (!muxer.open(outputPath)) {
        Logger::instance().error("ExportVideo: failed to open muxer for " + outputPath);
        m_done = true;
        return;
    }

    VideoEncoder videoEncoder;
    if (!videoEncoder.open(decoder.videoWidth(), decoder.videoHeight(),
                           demuxer.videoTimeBase(),
                           settings.videoBitRateKbps, preferH264,
                           muxer.needsGlobalHeader())) {
        Logger::instance().error("ExportVideo: failed to open video encoder for " + outputPath);
        m_done = true;
        return;
    }
    videoEncoder.setStream(muxer.addStream(videoEncoder.codecContext()));

    AudioEncoder audioEncoder;
    if (hasAudio) {
        if (!audioEncoder.open(sampleRate, channels,
                               settings.audioBitRateKbps,
                               muxer.needsGlobalHeader())) {
            Logger::instance().error("ExportVideo: failed to open audio encoder for " + outputPath);
            m_done = true;
            return;
        }
        audioEncoder.setStream(muxer.addStream(audioEncoder.codecContext()));
    }

    if (!muxer.beginFile()) {
        Logger::instance().error("ExportVideo: failed to begin muxer file for " + outputPath);
        m_done = true;
        return;
    }

    if (hasAudio) {
        muxer.startAsync(videoEncoder.outputQueue(), audioEncoder.outputQueue());
    } else {
        muxer.startAsync(videoEncoder.outputQueue());
    }

    videoEncoder.startAsync(decoder.videoOutputQueue());
    if (hasAudio) {
        audioEncoder.startAsync(decoder.audioOutputQueue());
    }

    double duration    = demuxer.duration();
    double exportLimit = (settings.exportDuration > 0.0f)
                         ? exportStart + static_cast<double>(settings.exportDuration)
                         : duration;
    double clipLength  = exportLimit - exportStart;

    AVRational vtb = demuxer.videoTimeBase();
    int64_t videoPtsOffset = static_cast<int64_t>(exportStart * vtb.den / vtb.num);

    decoder.startAsync(demuxer.videoStreamIndex(), demuxer.outputQueue(),
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
            if (frame.pts < exportStart) {
                av_frame_free(&frame.videoFrame);
                av_frame_free(&frame.audioFrame);
                return true;
            }
            if (frame.videoFrame) {
                frame.videoFrame->pts -= videoPtsOffset;
                if (frame.videoFrame->pts < 0) { frame.videoFrame->pts = 0; }
            }
            return !m_cancel.load();
        });

    AVPacket* pkt = av_packet_alloc();
    while (!m_cancel) {
        if (!demuxer.readPacket(pkt)) { break; }
        AVPacket* copy = av_packet_alloc();
        av_packet_move_ref(copy, pkt);
        demuxer.outputQueue().push(copy);
    }
    av_packet_free(&pkt);

    demuxer.outputQueue().close();
    videoEncoder.close();
    if (hasAudio) { audioEncoder.close(); }
    decoder.waitDone();
    muxer.endFile();

    if (!m_cancel) {
        m_progress = 1.0f;
        Logger::instance().info("ExportVideo: export complete — " + outputPath);
    }
    m_done = true;
}
