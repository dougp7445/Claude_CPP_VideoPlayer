#include "VideoPlayer.h"
#include "Constants.h"
#include "FileOperations.h"
#include "Logger.h"
#include <algorithm>
#include <string>

VideoPlayer::VideoPlayer(const VideoPlayerConfig& config)
    : m_settings(executableDir() + SETTINGS_FILENAME)
{
    m_renderer.setConfig(config);
}
VideoPlayer::~VideoPlayer() = default;

bool VideoPlayer::load(const std::string& filePath) {
    if (!m_demuxer.open(filePath)) {
        return false;
    }
    if (!m_decoder.open(m_demuxer.videoCodecParameters(),
                        m_demuxer.audioCodecParameters(),
                        m_demuxer.videoTimeBase(),
                        m_demuxer.audioTimeBase())) {
        return false;
    }
    m_filePath = filePath;
    m_settings.addRecentFile(filePath);
    m_renderer.setRecentFiles(m_settings.recentFiles());
    return m_renderer.initWindow("VideoPlayer - " + filePath,
                                 m_decoder.videoWidth(),
                                 m_decoder.videoHeight());
}

bool VideoPlayer::reload(const std::string& filePath) {
    m_demuxer.close();
    m_decoder.close();
    if (!m_demuxer.open(filePath)) { return false; }
    if (!m_decoder.open(m_demuxer.videoCodecParameters(),
                        m_demuxer.audioCodecParameters(),
                        m_demuxer.videoTimeBase(),
                        m_demuxer.audioTimeBase())) {
        return false;
    }
    m_filePath = filePath;
    m_settings.addRecentFile(filePath);
    m_renderer.setRecentFiles(m_settings.recentFiles());
    return m_renderer.reloadVideo("VideoPlayer - " + filePath,
                                  m_decoder.videoWidth(),
                                  m_decoder.videoHeight());
}

void VideoPlayer::resetState() {
    m_quit             = false;
    m_requestOpenFile  = false;
    m_showExportDialog = false;
    m_encodingActive   = false;
}

double VideoPlayer::duration() const {
    return m_demuxer.duration();
}

SDL_Renderer* VideoPlayer::sdlRenderer() const {
    return m_renderer.getSDLRenderer();
}

std::string VideoPlayer::takePendingOpenPath() {
    return m_renderer.takePendingOpenPath();
}

bool VideoPlayer::seek(double seconds) {
    if (!m_demuxer.seek(seconds)) {
        return false;
    }
    m_decoder.flushBuffers();
    return true;
}

void VideoPlayer::renderLoop() {
    if (!m_renderer.initRenderer()) {
        m_quit = true;
        return;
    }

    const double audioLatency = m_renderer.getAudioLatency();
    const double SEEK_STEP    = SEEK_STEP_SECONDS;
    const double duration     = m_demuxer.duration();

    bool     clocked    = false;
    bool     reclock    = false;
    uint64_t startTick  = 0;
    double   startPts   = 0.0;
    bool     paused     = false;
    double   currentPts = 0.0;
    float    lastSpeed  = 1.0f;
    double   target     = 0.0;

    bool wasSuppress = false;
    while (!m_quit) {
        bool inDialog = m_showExportDialog.load();
        bool suppress = inDialog || m_encodingActive.load();
        if (suppress) { m_renderer.onActivity(); }
        PlayerEvent ev = suppress ? PlayerEvent::None : m_renderer.pollEvents();
        if (!suppress && wasSuppress) { reclock = true; }
        wasSuppress = suppress;

        if (ev == PlayerEvent::Quit) {
            break;
        }

        if (ev == PlayerEvent::OpenFile) {
            m_requestOpenFile = true;
            break;
        }

        if (ev == PlayerEvent::ExportVideo && !inDialog) {
            m_showExportDialog = true;
        }

        switch (ev) {
            case PlayerEvent::TogglePause:
                paused = !paused;
                if (paused) {
                    m_renderer.pauseAudio();
                } else {
                    m_renderer.resumeAudio();
                    reclock = true;
                }
                break;

            case PlayerEvent::SeekForward:
            case PlayerEvent::SeekBackward:
                target = currentPts + (ev == PlayerEvent::SeekForward ? SEEK_STEP : -SEEK_STEP);
                target = std::max(0.0, std::min(target, duration));
                if (seek(target)) {
                    m_renderer.flushAudio();
                    reclock = true;
                    if (paused) {
                        paused = false;
                        m_renderer.resumeAudio();
                    }
                }
                break;
            case PlayerEvent::SeekTo:
                target = m_renderer.getSeekTarget() * duration;
                target = std::max(0.0, std::min(target, duration));
                if (seek(target)) {
                    m_renderer.flushAudio();
                    reclock = true;
                    if (paused) {
                        paused = false;
                        m_renderer.resumeAudio();
                    }
                }
                break;
            case PlayerEvent::VolumeUp:
                m_renderer.adjustVolume( VOLUME_KEY_DELTA);
                break;
            case PlayerEvent::VolumeDown:
                m_renderer.adjustVolume(-VOLUME_KEY_DELTA);
                break;
            case PlayerEvent::ToggleFullscreen:
                m_renderer.toggleFullscreen();
                break;
            case PlayerEvent::SpeedChange:
                reclock = true;
                break;
            default:
                break;
        }

        float curSpeed = m_renderer.getSpeed();
        if (curSpeed != lastSpeed) { lastSpeed = curSpeed; reclock = true; }

        if (paused) {
            m_renderer.renderUI(currentPts, duration, true);
            m_renderer.present();
            SDL_Delay(16);
            continue;
        }

        // Pull packets from the demuxer and feed them to the decoder until
        // a frame comes out (needed for codecs that buffer across packets).
        DecodedFrame frame;
        bool gotFrame = false;
        AVPacket* pkt = av_packet_alloc();
        while (!gotFrame) {
            if (!m_demuxer.readPacket(pkt)) {
                break;
            }
            gotFrame = m_decoder.decode(pkt, m_demuxer.isVideoPacket(pkt), frame);
        }
        av_packet_free(&pkt);

        if (!gotFrame) {
            break;
        }

        if (frame.audioFrame) {
            AVFrame* f = frame.audioFrame;
            m_renderer.queueAudio(f->data[0], f->nb_samples * 2 * sizeof(int16_t));
            av_frame_free(&frame.audioFrame);
        }

        if (frame.videoFrame) {
            AVFrame* f = frame.videoFrame;
            currentPts = frame.pts;

            if (!clocked || reclock) {
                startTick = SDL_GetTicks();
                startPts  = frame.pts;
                clocked   = true;
                reclock   = false;
            }

            float  speed      = m_renderer.getSpeed();
            double targetWall = (frame.pts - startPts) / speed + audioLatency;
            double wall       = static_cast<double>(SDL_GetTicks() - startTick) / 1000.0;

            if (targetWall > wall) {
                SDL_Delay(static_cast<Uint32>((targetWall - wall) * 1000.0));
            }

            m_renderer.renderFrame(
                f->data[0], f->linesize[0],
                f->data[1], f->linesize[1],
                f->data[2], f->linesize[2]);

            m_renderer.renderUI(currentPts, duration, false);
            m_renderer.present();

            av_frame_free(&frame.videoFrame);
        }
    }

    m_quit = true;
}
