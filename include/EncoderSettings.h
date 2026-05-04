#ifndef ENCODER_SETTINGS_H
#define ENCODER_SETTINGS_H

#include <string>

struct EncoderSettings {
    enum class VideoCodec    { H264, MPEG4 };
    enum class OutputFormat  { MP4, TS };

    VideoCodec   videoCodec       = VideoCodec::H264;
    OutputFormat outputFormat     = OutputFormat::MP4;
    int          videoBitRateKbps = 2000;
    int          audioBitRateKbps = 128;
    float        exportStartTime  = 0.0f; // seconds from beginning
    float        exportDuration   = 0.0f; // seconds; 0 = to end of video
    std::string  outputFilePath;
};

#endif // ENCODER_SETTINGS_H
