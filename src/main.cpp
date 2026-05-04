#include "MainWindow.h"
#include "FileOperations.h"
#include "Logger.h"
#include <chrono>
#include <ctime>
#include <string>

int main(int argc, char* argv[]) {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    localtime_s(&tm, &t);
    char tsBuf[20];
    std::strftime(tsBuf, sizeof(tsBuf), "%d-%m-%Y_%H-%M", &tm);
    Logger::instance().init(executableDir() + "Log\\video_player_" + tsBuf + ".log");

    std::string filePath;

    if (argc >= 2) {
        filePath = argv[1];
    } else {
        filePath = openFileDialog();
        if (filePath.empty()) {
            return 0;
        }
    }

    MainWindow window;
    if (!window.load(filePath)) {
        Logger::instance().error("Failed to load: " + filePath);
        return 1;
    }

    window.run();
    return 0;
}
