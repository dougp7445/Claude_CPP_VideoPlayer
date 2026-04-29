#ifndef PLAYER_EVENT_H
#define PLAYER_EVENT_H

enum class PlayerEvent {
    None,
    Quit,
    TogglePause,
    SeekForward,
    SeekBackward,
    SeekTo,
    VolumeUp,
    VolumeDown,
    ToggleFullscreen,
    SpeedChange,
    OpenFile,
    ExportVideo,
};

#endif // PLAYER_EVENT_H
