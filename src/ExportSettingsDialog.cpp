#include "ExportSettingsDialog.h"
#include "FileOperations.h"

ExportSettingsDialog::ExportSettingsDialog(double videoDuration, const std::string& defaultFolder)
    : DialogWindow("Export Settings", 400, 298) {
    m_panel.setVideoDuration(videoDuration);
    m_panel.setOutputFolder(defaultFolder);
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
    } else if (r == EncoderSettingsPanel::Result::BrowseFolder) {
        std::string folder = openFolderDialog(m_panel.getSettings().outputFolder);
        if (!folder.empty()) {
            m_panel.setOutputFolder(folder);
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
                      const std::string& defaultFolder,
                      std::atomic<bool>& quit,
                      EncoderSettings&   outSettings) {
    ExportSettingsDialog dlg(videoDuration, defaultFolder);
    dlg.run(mainRenderer, quit);
    if (dlg.wasExported()) {
        outSettings = dlg.getSettings();
        return true;
    }
    return false;
}
