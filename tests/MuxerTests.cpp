#include "Muxer.h"
#include <gtest/gtest.h>
#include <filesystem>
#include <string>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/pixfmt.h>
}

namespace fs = std::filesystem;

class MuxerTest : public ::testing::Test {
protected:
    fs::path    testDir;
    std::string outPath;

    void SetUp() override {
        testDir = fs::temp_directory_path() / "VideoPlayerMuxerTests";
        fs::remove_all(testDir);
        fs::create_directories(testDir);
        outPath = (testDir / "out.mp4").string();
    }

    void TearDown() override {
        fs::remove_all(testDir);
    }

    // Open a minimal MPEG-4 codec context suitable for adding a stream.
    // Caller must avcodec_free_context() the result.
    AVCodecContext* makeVideoCodecCtx(bool globalHeader) {
        const AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_MPEG4);
        if (!codec) { return nullptr; }
        AVCodecContext* ctx = avcodec_alloc_context3(codec);
        ctx->bit_rate     = 400000;
        ctx->width        = 320;
        ctx->height       = 240;
        ctx->time_base    = {1, 25};
        ctx->gop_size     = 12;
        ctx->pix_fmt      = AV_PIX_FMT_YUV420P;
        ctx->max_b_frames = 0;
        if (globalHeader) { ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER; }
        if (avcodec_open2(ctx, codec, nullptr) < 0) {
            avcodec_free_context(&ctx);
            return nullptr;
        }
        return ctx;
    }
};

// ── open() ────────────────────────────────────────────────────────────────────

TEST_F(MuxerTest, OpenValidPathReturnsTrue) {
    Muxer muxer;
    EXPECT_TRUE(muxer.open(outPath));
}

TEST_F(MuxerTest, OpenInvalidFormatReturnsFalse) {
    Muxer muxer;
    EXPECT_FALSE(muxer.open(""));
}

// ── needsGlobalHeader() ───────────────────────────────────────────────────────

TEST_F(MuxerTest, NeedsGlobalHeaderReturnsFalseWhenNotOpen) {
    Muxer muxer;
    EXPECT_FALSE(muxer.needsGlobalHeader());
}

TEST_F(MuxerTest, NeedsGlobalHeaderReturnsTrueForMp4) {
    Muxer muxer;
    ASSERT_TRUE(muxer.open(outPath));
    EXPECT_TRUE(muxer.needsGlobalHeader());
}

// ── beginFile() ───────────────────────────────────────────────────────────────

TEST_F(MuxerTest, BeginFileWithoutStreamsReturnsFalse) {
    Muxer muxer;
    ASSERT_TRUE(muxer.open(outPath));
    EXPECT_FALSE(muxer.beginFile());
}

TEST_F(MuxerTest, BeginFileWithStreamReturnsTrue) {
    Muxer muxer;
    ASSERT_TRUE(muxer.open(outPath));
    AVCodecContext* ctx = makeVideoCodecCtx(muxer.needsGlobalHeader());
    ASSERT_NE(ctx, nullptr);
    ASSERT_NE(muxer.addStream(ctx), nullptr);
    EXPECT_TRUE(muxer.beginFile());
    muxer.endFile();
    avcodec_free_context(&ctx);
}

TEST_F(MuxerTest, BeginFileOnBadPathReturnsFalse) {
    Muxer muxer;
    std::string bad = (testDir / "nonexistent" / "out.mp4").string();
    ASSERT_TRUE(muxer.open(bad));
    EXPECT_FALSE(muxer.beginFile());
}

// ── endFile() ─────────────────────────────────────────────────────────────────

TEST_F(MuxerTest, EndFileWithoutOpenReturnsFalse) {
    Muxer muxer;
    EXPECT_FALSE(muxer.endFile());
}

TEST_F(MuxerTest, EndFileAfterBeginFileReturnsTrue) {
    Muxer muxer;
    ASSERT_TRUE(muxer.open(outPath));
    AVCodecContext* ctx = makeVideoCodecCtx(muxer.needsGlobalHeader());
    ASSERT_NE(ctx, nullptr);
    ASSERT_NE(muxer.addStream(ctx), nullptr);
    ASSERT_TRUE(muxer.beginFile());
    EXPECT_TRUE(muxer.endFile());
    avcodec_free_context(&ctx);
}
