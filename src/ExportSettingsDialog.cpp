#include "ExportSettingsDialog.h"
#include "Demuxer.h"
#include "FileOperations.h"

ExportSettingsDialog::ExportSettingsDialog(double videoDuration,
                                           const std::string& defaultSourceFilePath,
                                           const std::string& defaultFilePath)
    : DialogWindow("Export Settings", 680, 334) {
    m_panel.setVideoDuration(videoDuration);
    m_panel.setSourceFilePath(defaultSourceFilePath);
    m_panel.setOutputFilePath(defaultFilePath);
}

void ExportSettingsDialog::onRender(SDL_Renderer* renderer, float w, float h) {
    m_panel.render(renderer, w, h);
}

void ExportSettingsDialog::onMouseMotion(float x, float y) {
    m_panel.handleMouseMotion(x, y);
}

void ExportSettingsDialog::onMouseButtonDown(float x, float y) {
    auto r = m_panel.handleMouseClick(x, y);
    if (r == EncoderSettingsPanel::Result::Export) {
        m_exported = true;
        close();
    } else if (r == EncoderSettingsPanel::Result::Cancel) {
        close();
    } else if (r == EncoderSettingsPanel::Result::BrowseSourceFile) {
        std::string filePath = openFileDialog();
        if (!filePath.empty()) {
            m_panel.setSourceFilePath(filePath);
            Demuxer demuxer;
            if (demuxer.open(filePath)) {
                m_panel.setVideoDuration(demuxer.duration());
            }
        }
    } else if (r == EncoderSettingsPanel::Result::BrowseFile) {
        std::string ext = (m_panel.getSettings().outputFormat == EncoderSettings::OutputFormat::TS)
            ? "ts" : "mp4";
        std::string filePath = saveFileDialog(ext);
        if (!filePath.empty()) {
            m_panel.setOutputFilePath(filePath);
        }
    }
}

void ExportSettingsDialog::onMouseButtonUp(float x, float y) {
    m_panel.handleMouseButtonUp(x, y);
}

void ExportSettingsDialog::onTextInput(const char* text) {
    m_panel.handleTextInput(text);
}

void ExportSettingsDialog::onKeyDown(SDL_Keycode key) {
    m_panel.handleKeyDown(key);
}

bool showExportDialog(SDL_Renderer*      mainRenderer,
                      double             videoDuration,
                      const std::string& defaultSourceFilePath,
                      const std::string& defaultFilePath,
                      std::atomic<bool>& quit,
                      EncoderSettings&   outSettings) {
    ExportSettingsDialog dlg(videoDuration, defaultSourceFilePath, defaultFilePath);
    dlg.run(mainRenderer, quit);
    if (dlg.wasExported()) {
        outSettings = dlg.getSettings();
        return true;
    }
    return false;
}
