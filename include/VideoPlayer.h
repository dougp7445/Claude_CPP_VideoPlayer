#ifndef VIDEO_PLAYER_H
#define VIDEO_PLAYER_H

#include "Decoder.h"
#include "Demuxer.h"
#include "Renderer.h"
#include "AppSettings.h"
#include "VideoPlayerConfig.h"
#include <atomic>
#include <string>

class VideoPlayer {
public:
    explicit VideoPlayer(const VideoPlayerConfig& config = makeMainWindowConfig());
    ~VideoPlayer();

    bool load(const std::string& filePath);
    bool reload(const std::string& filePath);
    void resetState();
    void renderLoop();

    std::atomic<bool>& quit()                        { return m_quit; }
    bool requestedOpenFile()  const                  { return m_requestOpenFile.load(); }
    bool wantsExportDialog()  const                  { return m_showExportDialog.load(); }
    void clearWantsExportDialog()                    { m_showExportDialog = false; }
    void setEncodingActive(bool v)                   { m_encodingActive = v; }

    const std::string& lastExportDir() const         { return m_settings.lastExportDir(); }
    void setLastExportDir(const std::string& dir)    { m_settings.setLastExportDir(dir); }

    const std::string& filePath()    const           { return m_filePath; }
    double             duration()    const;
    SDL_Renderer*      sdlRenderer() const;
    std::string        takePendingOpenPath();

private:
    bool seek(double seconds);

    Demuxer     m_demuxer;
    Decoder     m_decoder;
    Renderer    m_renderer;
    std::string m_filePath;
    AppSettings m_settings;

    std::atomic<bool> m_quit{false};
    std::atomic<bool> m_requestOpenFile{false};
    std::atomic<bool> m_showExportDialog{false};
    std::atomic<bool> m_encodingActive{false};
};

#endif // VIDEO_PLAYER_H
