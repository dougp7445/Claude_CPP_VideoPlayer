#include "VideoPlayer.h"
#include "FileOperations.h"
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    std::string filePath;

    if (argc >= 2) {
        filePath = argv[1];
    } else {
        filePath = openFileDialog();
        if (filePath.empty()) {
            return 0;
        }
    }

    VideoPlayer player;
    if (!player.load(filePath)) {
        std::cerr << "Failed to load: " << filePath << "\n";
        return 1;
    }

    player.run();
    return 0;
}
