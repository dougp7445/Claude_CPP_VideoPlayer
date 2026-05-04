#ifndef FILE_OPERATIONS_H
#define FILE_OPERATIONS_H

#include <string>

// Returns the directory containing the running executable, with a trailing
// path separator. Returns an empty string on unsupported platforms.
std::string executableDir();

// Shows a native OS file-open dialog filtered to common video formats.
// Returns the selected path, or an empty string if canceled.
std::string openFileDialog();

// Shows a native OS file-save dialog. extension must be "mp4" or "ts".
// Returns the chosen path with the correct extension, or an empty string if canceled.
std::string saveFileDialog(const std::string& extension = "mp4");

// Shows a native OS folder-picker dialog.
// defaultFolder sets the initial location shown; pass empty to use the OS default.
// Returns the selected folder path, or an empty string if canceled.
std::string openFolderDialog(const std::string& defaultFolder = "");

// Opens path in the platform's file explorer.
void openDirectory(const std::string& path);

#endif // FILE_OPERATIONS_H
