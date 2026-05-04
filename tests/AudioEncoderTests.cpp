#include "AudioEncoder.h"
#include "Muxer.h"
#include "VideoEncoder.h"
#include "LockingQueue.h"
#include <gtest/gtest.h>
#include <cstring>
#include <filesystem>
#include <string>

extern "C" {
#include <libavutil/channel_layout.h>
#include <libavutil/frame.h>
}

namespace fs = std::filesystem;

class AudioEncoderTest : public ::testing::Test {
protected:
    fs::path    testDir;
    std::string outPath;

    void SetUp() override {
        testDir = fs::temp_directory_path() / "VideoPlayerAudioEncoderTests";
        fs::remove_all(testDir);
        fs::create_directories(testDir);
        outPath = (testDir / "out.mp4").string();
    }

    void TearDown() override {
        fs::remove_all(testDir);
    }

    bool openAVPair(Muxer& muxer, VideoEncoder& video, AudioEncoder& audio,
                    int w = 320, int h = 240,
                    int sampleRate = 44100, int channels = 2) {
        if (!muxer.open(outPath)) { return false; }
        if (!video.open(w, h, {1, 12800}, 2000, true,
                        muxer.needsGlobalHeader())) { return false; }
        if (!audio.open(sampleRate, channels, 128,
                        muxer.needsGlobalHeader())) { return false; }
        video.setStream(muxer.addStream(video.codecContext()));
        audio.setStream(muxer.addStream(audio.codecContext()));
        return muxer.beginFile() &&
               muxer.startAsync(video.outputQueue(), audio.outputQueue());
    }

    AVFrame* makeSyntheticVideoFrame(int width, int height, int64_t pts = 0) {
        AVFrame* frame = av_frame_alloc();
        frame->format = AV_PIX_FMT_YUV420P;
        frame->width  = width;
        frame->height = height;
        frame->pts    = pts;
        av_frame_get_buffer(frame, 0);
        av_frame_make_writable(frame);
        memset(frame->data[0], 0,   frame->linesize[0] * height);
        memset(frame->data[1], 128, frame->linesize[1] * height / 2);
        memset(frame->data[2], 128, frame->linesize[2] * height / 2);
        return frame;
    }

    AVFrame* makeSyntheticAudioFrame(int nbSamples = 1024, int sampleRate = 44100) {
        AVFrame* frame = av_frame_alloc();
        frame->format      = AV_SAMPLE_FMT_S16;
        frame->sample_rate = sampleRate;
        frame->nb_samples  = nbSamples;
        AVChannelLayout stereo = AV_CHANNEL_LAYOUT_STEREO;
        av_channel_layout_copy(&frame->ch_layout, &stereo);
        av_frame_get_buffer(frame, 0);
        av_frame_make_writable(frame);
        memset(frame->data[0], 0, frame->linesize[0]);
        return frame;
    }
};

// ── Lifecycle ─────────────────────────────────────────────────────────────────

TEST_F(AudioEncoderTest, IsNotOpenByDefault) {
    AudioEncoder audio;
    EXPECT_FALSE(audio.isOpen());
}

TEST_F(AudioEncoderTest, IsOpenAfterOpen) {
    AudioEncoder audio;
    EXPECT_TRUE(audio.open(44100, 2, 128, false));
    EXPECT_TRUE(audio.isOpen());
    audio.close();
}

TEST_F(AudioEncoderTest, IsClosedAfterClose) {
    AudioEncoder audio;
    ASSERT_TRUE(audio.open(44100, 2, 128, false));
    audio.close();
    EXPECT_FALSE(audio.isOpen());
}

TEST_F(AudioEncoderTest, CloseOnUnopenedReturnsFalse) {
    AudioEncoder audio;
    EXPECT_FALSE(audio.close());
}

// ── writeFrame() ──────────────────────────────────────────────────────────────

TEST_F(AudioEncoderTest, WriteAudioFrameDoesNotCrash) {
    Muxer muxer;
    VideoEncoder video;
    AudioEncoder audio;
    ASSERT_TRUE(openAVPair(muxer, video, audio));
    AVFrame* frame = makeSyntheticAudioFrame();
    EXPECT_TRUE(audio.writeFrame(frame));
    av_frame_free(&frame);
    video.close();
    audio.close();
    muxer.endFile();
}

TEST_F(AudioEncoderTest, WriteMultipleAudioFramesDoesNotCrash) {
    Muxer muxer;
    VideoEncoder video;
    AudioEncoder audio;
    ASSERT_TRUE(openAVPair(muxer, video, audio));
    for (int i = 0; i < 5; ++i) {
        AVFrame* frame = makeSyntheticAudioFrame();
        EXPECT_TRUE(audio.writeFrame(frame));
        av_frame_free(&frame);
    }
    video.close();
    audio.close();
    muxer.endFile();
}

// ── Async ─────────────────────────────────────────────────────────────────────

TEST_F(AudioEncoderTest, AsyncAudioEncodeProducesOutput) {
    LockingQueue<AVFrame*> vfq, afq;
    vfq.close();
    for (int i = 0; i < 5; ++i) {
        afq.push(makeSyntheticAudioFrame());
    }
    afq.close();

    Muxer muxer;
    VideoEncoder video;
    AudioEncoder audio;
    ASSERT_TRUE(openAVPair(muxer, video, audio));
    video.startAsync(vfq);
    audio.startAsync(afq);
    video.close();
    audio.close();
    muxer.endFile();
    EXPECT_GT(fs::file_size(outPath), static_cast<uintmax_t>(0));
}
