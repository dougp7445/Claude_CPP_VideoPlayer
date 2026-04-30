#ifndef MUXER_H
#define MUXER_H

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

#include <string>

class Muxer {
public:
    Muxer();
    ~Muxer();

    // Allocate an output format context inferred from the file extension.
    bool open(const std::string& outputPath);

    // True when the output format requires a global codec header
    // (AVFMT_GLOBALHEADER). Codecs must set AV_CODEC_FLAG_GLOBAL_HEADER
    // before being opened when this returns true.
    bool needsGlobalHeader() const;

    // Register a fully-opened codec context as a new output stream.
    // Copies codec parameters and time base to the stream. Returns the
    // created AVStream, or nullptr on failure.
    AVStream* addStream(AVCodecContext* codecCtx);

    // Open the output file and write the container header.
    // Call after all addStream() calls and before any writePacket() calls.
    bool beginFile();

    // Write one encoded packet using interleaved muxing.
    bool writePacket(AVPacket* packet);

    // Write the container trailer and close the output file.
    bool endFile();

    // Release all resources without finalizing (safe to call on error paths).
    void close();

private:
    AVFormatContext* m_fmtCtx     = nullptr;
    std::string      m_outputPath;
};

#endif // MUXER_H
