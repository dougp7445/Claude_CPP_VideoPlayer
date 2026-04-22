#include "VideoPlayer.h"
#include <iostream>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: VideoPlayer <file>\n";
        return 1;
    }

    VideoPlayer player;
    if (!player.load(argv[1])) {
        std::cerr << "Failed to load: " << argv[1] << "\n";
        return 1;
    }

    player.run();
    return 0;
}
