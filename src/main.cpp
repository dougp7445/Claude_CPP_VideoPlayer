#include "VideoPlayer.h"
#include <iostream>
#include <string>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shobjidl.h>

static std::string openFileDialog() {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

    std::string result;
    IFileOpenDialog* pfd = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                                   IID_PPV_ARGS(&pfd)))) {
        COMDLG_FILTERSPEC filters[] = {
            { L"Video Files", L"*.mp4;*.mkv;*.avi;*.mov;*.wmv;*.flv;*.webm;*.m4v;*.ts;*.mpg;*.mpeg" },
            { L"All Files",   L"*.*" }
        };
        pfd->SetFileTypes(2, filters);
        pfd->SetDefaultExtension(L"mp4");

        if (SUCCEEDED(pfd->Show(nullptr))) {
            IShellItem* psi = nullptr;
            if (SUCCEEDED(pfd->GetResult(&psi))) {
                PWSTR path = nullptr;
                if (SUCCEEDED(psi->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
                    int len = WideCharToMultiByte(CP_UTF8, 0, path, -1, nullptr, 0, nullptr, nullptr);
                    if (len > 0) {
                        result.resize(len - 1);
                        WideCharToMultiByte(CP_UTF8, 0, path, -1, result.data(), len, nullptr, nullptr);
                    }
                    CoTaskMemFree(path);
                }
                psi->Release();
            }
        }
        pfd->Release();
    }

    CoUninitialize();
    return result;
}
#endif

int main(int argc, char* argv[]) {
    std::string filePath;

    if (argc >= 2) {
        filePath = argv[1];
    } else {
#ifdef _WIN32
        filePath = openFileDialog();
        if (filePath.empty()) {
            return 0;
        }
#else
        std::cerr << "Usage: VideoPlayer <file>\n";
        return 1;
#endif
    }

    VideoPlayer player;
    if (!player.load(filePath)) {
        std::cerr << "Failed to load: " << filePath << "\n";
        return 1;
    }

    player.run();
    return 0;
}
