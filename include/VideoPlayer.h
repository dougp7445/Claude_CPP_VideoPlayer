#ifndef VIDEO_PLAYER_H
#define VIDEO_PLAYER_H

#include "Decoder.h"
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

    Decoder  m_decoder;
    Renderer m_renderer;

    std::atomic<bool> m_quit{false};
    std::atomic<bool> m_requestOpenFile{false};

    RecentFiles m_recentFiles;
};

#endif // VIDEO_PLAYER_H
