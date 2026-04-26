#include <gtest/gtest.h>
#include "VideoPlayer.h"
#include "TestConstants.h"

// ── load() ────────────────────────────────────────────────────────────────────

TEST(VideoPlayerLoadTest, LoadInvalidFileReturnsFalse) {
    VideoPlayer p;
    EXPECT_FALSE(p.load("nonexistent.mp4"));
}

TEST(VideoPlayerLoadTest, LoadValidFileReturnsTrue) {
    VideoPlayer p;
    EXPECT_TRUE(p.load(TEST_VIDEO_PATH));
}

