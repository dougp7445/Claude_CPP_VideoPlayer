#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include "EncoderSettings.h"
#include "ExportVideo.h"
#include "FileOperations.h"
#include "VideoPlayer.h"
#include <atomic>
#include <functional>
#include <string>

class MainWindow {
public:
    MainWindow();
    ~MainWindow();

    bool load(const std::string& filePath);
    void run(std::function<std::string()> filePicker = openFileDialog);

private:
    void runExportDialog();
    void runExportProgress();

    VideoPlayer     m_player;
    ExportVideo     m_exporter;
    EncoderSettings m_exportSettings;

    std::atomic<bool> m_requestExport{false};
};

#endif // MAIN_WINDOW_H
