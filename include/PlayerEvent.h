#pragma once

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
};
