#include "ExportSettingsDialog.h"

ExportSettingsDialog::ExportSettingsDialog(double videoDuration)
    : DialogWindow("Export Settings", 400, 262) {
    m_panel.setVideoDuration(videoDuration);
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
    }
}

void ExportSettingsDialog::onMouseButtonUp(float x, float y) {
    m_panel.handleMouseButtonUp(x, y);
}

bool showExportDialog(SDL_Renderer*      mainRenderer,
                      double             videoDuration,
                      std::atomic<bool>& quit,
                      EncoderSettings&   outSettings) {
    ExportSettingsDialog dlg(videoDuration);
    dlg.run(mainRenderer, quit);
    if (dlg.wasExported()) {
        outSettings = dlg.getSettings();
        return true;
    }
    return false;
}
