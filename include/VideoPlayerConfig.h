#ifndef VIDEO_PLAYER_CONFIG_H
#define VIDEO_PLAYER_CONFIG_H

// Feature flags that control which UI elements and capabilities are active.
// Pass a config to VideoPlayer's constructor to customise it for each use-site.
struct VideoPlayerConfig {
    bool showPlayPause     = true;  // play/pause button
    bool showSeekBar       = true;  // progress / seek bar
    bool showTimeDisplay   = true;  // current / total time text
    bool showSpeedControl  = true;  // speed-cycle button
    bool showVolumeControl = true;  // mute button + volume bar
    bool showMenu          = true;  // file / recent-files / export menu
    bool allowFullscreen   = true;  // F key and toggle-fullscreen event
    bool autoHideUI        = true;  // hide controls after inactivity
    bool enableAudio       = true;  // open and play the audio stream
};

// Full-featured default — equivalent to the original single hard-coded config.
inline VideoPlayerConfig makeMainWindowConfig() {
    return VideoPlayerConfig{};
}

// Minimal preview: seek and play/pause only, no audio, no menus.
// Suitable for embedding in export-settings or other dialogs.
inline VideoPlayerConfig makePreviewConfig() {
    VideoPlayerConfig cfg;
    cfg.showSpeedControl  = false;
    cfg.showVolumeControl = false;
    cfg.showMenu          = false;
    cfg.allowFullscreen   = false;
    cfg.autoHideUI        = false;
    cfg.enableAudio       = false;
    return cfg;
}

#endif // VIDEO_PLAYER_CONFIG_H
