#pragma once
#include <string>
#include "Constants.h"

// ── Test video ────────────────────────────────────────────────────────────────
// TEST_RESOURCES_DIR is injected as a compile definition by CMakeLists.txt.
static const std::string TEST_VIDEO_PATH = std::string(TEST_RESOURCES_DIR) + "/test.mp4";

// ── Audio ─────────────────────────────────────────────────────────────────────
static constexpr int TEST_AUDIO_BYTES_PER_SECOND =
    AUDIO_SAMPLE_RATE * AUDIO_CHANNELS * AUDIO_BYTES_PER_SAMPLE;

// Delay after queuing audio before checking the clock — gives the device time
// to start consuming bytes.
static constexpr int TEST_AUDIO_QUEUE_DELAY_MS = 100;

// ── Decoder ───────────────────────────────────────────────────────────────────
// AVSEEK_FLAG_BACKWARD lands on the prior keyframe, which may be several
// seconds before the target. Allow this much slack in seek accuracy tests.
static constexpr double TEST_SEEK_TOLERANCE_S = 5.0;

// Maximum frames to scan when searching for a specific frame type.
static constexpr int TEST_FRAME_SEARCH_LIMIT = 64;

// ── Window ────────────────────────────────────────────────────────────────────
// Synthetic resolution used by tests that initialize a renderer without a video.
static constexpr int TEST_WINDOW_WIDTH  = RES_720P_WIDTH;
static constexpr int TEST_WINDOW_HEIGHT = RES_720P_HEIGHT;
