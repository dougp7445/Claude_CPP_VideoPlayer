#pragma once

#include <string>

// Returns the directory containing the running executable, with a trailing
// path separator. Returns an empty string on unsupported platforms.
std::string executableDir();

// Shows a native OS file-open dialog filtered to common video formats.
// Returns the selected path, or an empty string if canceled.
std::string openFileDialog();
