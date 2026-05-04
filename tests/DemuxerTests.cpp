#include <gtest/gtest.h>
#include "Demuxer.h"
#include "TestConstants.h"

// ── Default state ─────────────────────────────────────────────────────────────

TEST(DemuxerTest, DefaultDurationIsZero) {
    Demuxer demuxer;
    EXPECT_DOUBLE_EQ(demuxer.duration(), 0.0);
}

// ── open() ────────────────────────────────────────────────────────────────────

TEST(DemuxerTest, OpenInvalidFileReturnsFalse) {
    Demuxer demuxer;
    EXPECT_FALSE(demuxer.open("nonexistent_file.mp4"));
}

TEST(DemuxerTest, OpenValidFileReturnsTrue) {
    Demuxer demuxer;
    EXPECT_TRUE(demuxer.open(TEST_VIDEO_PATH));
}

TEST(DemuxerTest, OpenValidFilePopulatesDuration) {
    Demuxer demuxer;
    ASSERT_TRUE(demuxer.open(TEST_VIDEO_PATH));
    EXPECT_GT(demuxer.duration(), 0.0);
}

TEST(DemuxerTest, HasVideoReturnsTrueForVideoFile) {
    Demuxer demuxer;
    ASSERT_TRUE(demuxer.open(TEST_VIDEO_PATH));
    EXPECT_TRUE(demuxer.hasVideo());
}

TEST(DemuxerTest, HasAudioReturnsTrueForAudioFile) {
    Demuxer demuxer;
    ASSERT_TRUE(demuxer.open(TEST_VIDEO_PATH));
    EXPECT_TRUE(demuxer.hasAudio());
}

TEST(DemuxerTest, VideoStreamIndexIsNonNegative) {
    Demuxer demuxer;
    ASSERT_TRUE(demuxer.open(TEST_VIDEO_PATH));
    EXPECT_GE(demuxer.videoStreamIndex(), 0);
}

TEST(DemuxerTest, AudioStreamIndexIsNonNegative) {
    Demuxer demuxer;
    ASSERT_TRUE(demuxer.open(TEST_VIDEO_PATH));
    EXPECT_GE(demuxer.audioStreamIndex(), 0);
}

TEST(DemuxerTest, AudioStreamIndexDiffersFromVideoStreamIndex) {
    Demuxer demuxer;
    ASSERT_TRUE(demuxer.open(TEST_VIDEO_PATH));
    EXPECT_NE(demuxer.audioStreamIndex(), demuxer.videoStreamIndex());
}

TEST(DemuxerTest, AudioCodecParametersAreNonNull) {
    Demuxer demuxer;
    ASSERT_TRUE(demuxer.open(TEST_VIDEO_PATH));
    EXPECT_NE(demuxer.audioCodecParameters(), nullptr);
}

// ── readPacket() ──────────────────────────────────────────────────────────────

TEST(DemuxerTest, IsVideoPacketIdentifiesVideoPackets) {
    Demuxer demuxer;
    ASSERT_TRUE(demuxer.open(TEST_VIDEO_PATH));
    AVPacket* pkt = av_packet_alloc();
    bool foundVideo = false;
    for (int i = 0; i < TEST_FRAME_SEARCH_LIMIT && !foundVideo; ++i) {
        if (!demuxer.readPacket(pkt)) { break; }
        if (demuxer.isVideoPacket(pkt)) { foundVideo = true; }
    }
    av_packet_free(&pkt);
    EXPECT_TRUE(foundVideo);
}

TEST(DemuxerTest, IsAudioPacketIdentifiesAudioPackets) {
    Demuxer demuxer;
    ASSERT_TRUE(demuxer.open(TEST_VIDEO_PATH));
    AVPacket* pkt = av_packet_alloc();
    bool foundAudio = false;
    for (int i = 0; i < TEST_FRAME_SEARCH_LIMIT && !foundAudio; ++i) {
        if (!demuxer.readPacket(pkt)) { break; }
        if (demuxer.isAudioPacket(pkt)) { foundAudio = true; }
    }
    av_packet_free(&pkt);
    EXPECT_TRUE(foundAudio);
}

TEST(DemuxerTest, ReadPacketReturnsFalseAtEof) {
    Demuxer demuxer;
    ASSERT_TRUE(demuxer.open(TEST_VIDEO_PATH));
    AVPacket* pkt = av_packet_alloc();
    bool reachedEof = false;
    for (int i = 0; i < 100000; ++i) {
        if (!demuxer.readPacket(pkt)) {
            reachedEof = true;
            break;
        }
    }
    av_packet_free(&pkt);
    EXPECT_TRUE(reachedEof);
}
