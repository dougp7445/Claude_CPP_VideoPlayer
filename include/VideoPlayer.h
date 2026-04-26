#pragma once

#include "Decoder.h"
#include "Renderer.h"
#include "RecentFiles.h"
#include <atomic>
#include <string>

class VideoPlayer {
public:
    VideoPlayer();
    ~VideoPlayer();

    bool load(const std::string& filePath);
    void run();

private:
    void renderLoop();

    Decoder  m_decoder;
    Renderer m_renderer;

    std::atomic<bool> m_quit{false};
    std::atomic<bool> m_requestOpenFile{false};

    RecentFiles m_recentFiles;
};
