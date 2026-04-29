#ifndef EXPORT_SETTINGS_DIALOG_H
#define EXPORT_SETTINGS_DIALOG_H

#include <SDL3/SDL.h>
#include <atomic>
#include "DialogWindow.h"
#include "EncoderSettings.h"
#include "EncoderSettingsPanel.h"

class ExportSettingsDialog : public DialogWindow {
public:
    explicit ExportSettingsDialog(double videoDuration);

    bool            wasExported() const { return m_exported; }
    EncoderSettings getSettings()  const { return m_panel.getSettings(); }

protected:
    void onRender(SDL_Renderer* renderer, float w, float h) override;
    void onMouseMotion(float x, float y) override;
    void onMouseButtonDown(float x, float y) override;
    void onMouseButtonUp(float x, float y) override;

private:
    EncoderSettingsPanel m_panel;
    bool                 m_exported = false;
};

// Convenience wrapper: opens the dialog and returns true if the user exported.
bool showExportDialog(SDL_Renderer*      mainRenderer,
                      double             videoDuration,
                      std::atomic<bool>& quit,
                      EncoderSettings&   outSettings);

#endif // EXPORT_SETTINGS_DIALOG_H
