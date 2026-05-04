#include "FileOperations.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#include <shobjidl.h>
#endif

std::string executableDir() {
#ifdef _WIN32
    wchar_t buf[MAX_PATH];
    DWORD len = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    if (len == 0) {
        return "";
    }

    std::wstring wpath(buf, len);
    auto pos = wpath.rfind(L'\\');
    if (pos != std::wstring::npos) {
        wpath.resize(pos + 1);
    }

    int n = WideCharToMultiByte(CP_UTF8, 0, wpath.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (n <= 0) {
        return "";
    }
    std::string result(n - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wpath.c_str(), -1, result.data(), n, nullptr, nullptr);
    return result;
#else
    return "";
#endif
}

std::string openFileDialog() {
#ifdef _WIN32
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
#else
    return {};
#endif
}

std::string saveFileDialog(const std::string& extension) {
#ifdef _WIN32
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

    bool isTs = (extension == "ts");

    std::string result;
    IFileSaveDialog* pfd = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_INPROC_SERVER,
                                   IID_PPV_ARGS(&pfd)))) {
        COMDLG_FILTERSPEC filters[1];
        if (isTs) {
            filters[0] = { L"MPEG-TS Video", L"*.ts" };
        } else {
            filters[0] = { L"MP4 Video",     L"*.mp4" };
        }
        pfd->SetFileTypes(1, filters);
        pfd->SetFileTypeIndex(1);
        pfd->SetDefaultExtension(isTs ? L"ts" : L"mp4");

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

    // Guard against the extension being absent or wrong despite SetDefaultExtension,
    // which can happen when the user types a bare name or clears the extension.
    std::string dotExt = "." + extension;
    if (!result.empty() &&
        (result.size() < dotExt.size() ||
         result.compare(result.size() - dotExt.size(), dotExt.size(), dotExt) != 0)) {
        result += dotExt;
    }

    return result;
#else
    return {};
#endif
}

std::string openFolderDialog(const std::string& defaultFolder) {
#ifdef _WIN32
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

    std::string result;
    IFileOpenDialog* pfd = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                                   IID_PPV_ARGS(&pfd)))) {
        FILEOPENDIALOGOPTIONS opts = 0;
        pfd->GetOptions(&opts);
        pfd->SetOptions(opts | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);

        if (!defaultFolder.empty()) {
            int wLen = MultiByteToWideChar(CP_UTF8, 0, defaultFolder.c_str(), -1, nullptr, 0);
            if (wLen > 0) {
                std::wstring wFolder(wLen - 1, L'\0');
                MultiByteToWideChar(CP_UTF8, 0, defaultFolder.c_str(), -1, wFolder.data(), wLen);
                IShellItem* psi = nullptr;
                if (SUCCEEDED(SHCreateItemFromParsingName(wFolder.c_str(), nullptr,
                                                          IID_PPV_ARGS(&psi)))) {
                    pfd->SetFolder(psi);
                    psi->Release();
                }
            }
        }

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
#else
    return {};
#endif
}

void openDirectory(const std::string& path) {
#ifdef _WIN32
    int wLen = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, nullptr, 0);
    if (wLen <= 0) { return; }
    std::wstring wPath(wLen - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, wPath.data(), wLen);
    ShellExecuteW(nullptr, L"explore", wPath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#endif
}
