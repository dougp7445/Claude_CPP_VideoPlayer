#ifndef VIDEO_PLAYER_H
#define VIDEO_PLAYER_H

#include "Decoder.h"
#include "EncoderSettings.h"
#include "FileOperations.h"
#include "Renderer.h"
#include "RecentFiles.h"
#include <atomic>
#include <functional>
#include <string>

class VideoPlayer {
public:
    VideoPlayer();
    ~VideoPlayer();

    bool load(const std::string& filePath);
    void run(std::function<std::string()> filePicker = openFileDialog);

private:
    void renderLoop();
    void runExportDialog();
    void runEncoding(const std::string& savePath, const EncoderSettings& settings,
                     std::atomic<bool>& cancel, std::atomic<float>& progress);

    Decoder  m_decoder;
    Renderer m_renderer;

    std::atomic<bool> m_quit{false};
    std::atomic<bool> m_requestOpenFile{false};
    std::atomic<bool> m_requestExport{false};
    std::atomic<bool> m_showExportDialog{false};

    EncoderSettings m_exportSettings;
    RecentFiles     m_recentFiles;
};

#endif // VIDEO_PLAYER_H
