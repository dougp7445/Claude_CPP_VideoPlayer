#include "RecentFiles.h"
#include <algorithm>
#include <fstream>
#include <sstream>

static std::string jsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:   out += c;      break;
        }
    }
    return out;
}

static std::string jsonUnescape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            switch (s[++i]) {
                case '"':  out += '"';  break;
                case '\\': out += '\\'; break;
                case 'n':  out += '\n'; break;
                case 'r':  out += '\r'; break;
                case 't':  out += '\t'; break;
                default:   out += s[i]; break;
            }
        } else {
            out += s[i];
        }
    }
    return out;
}

RecentFiles::RecentFiles(const std::string& filePath) : m_filePath(filePath) {
    load();
}

void RecentFiles::add(const std::string& path) {
    m_entries.erase(std::remove(m_entries.begin(), m_entries.end(), path),
                    m_entries.end());
    m_entries.insert(m_entries.begin(), path);
    if (static_cast<int>(m_entries.size()) > MAX_ENTRIES) {
        m_entries.resize(MAX_ENTRIES);
    }
    save();
}

void RecentFiles::load() {
    std::ifstream f(m_filePath);
    if (!f) { return; }

    std::string content((std::istreambuf_iterator<char>(f)),
                         std::istreambuf_iterator<char>());

    size_t arrayStart = content.find('[');
    if (arrayStart == std::string::npos) { return; }
    size_t arrayEnd = content.rfind(']');
    if (arrayEnd == std::string::npos || arrayEnd <= arrayStart) { return; }

    size_t pos = arrayStart + 1;
    while (pos < arrayEnd) {
        size_t strOpen = content.find('"', pos);
        if (strOpen == std::string::npos || strOpen >= arrayEnd) { break; }

        std::string raw;
        size_t i = strOpen + 1;
        while (i < content.size()) {
            char c = content[i++];
            if (c == '\\' && i < content.size()) { raw += '\\'; raw += content[i++]; }
            else if (c == '"') break;
            else raw += c;
        }

        if (!raw.empty()) {
            m_entries.push_back(jsonUnescape(raw));
        }

        pos = i;
    }

    if (static_cast<int>(m_entries.size()) > MAX_ENTRIES) {
        m_entries.resize(MAX_ENTRIES);
    }
}

void RecentFiles::save() const {
    std::ofstream f(m_filePath);
    f << "{\n    \"recentFiles\": [\n";
    for (size_t i = 0; i < m_entries.size(); ++i) {
        f << "        \"" << jsonEscape(m_entries[i]) << '"';
        if (i + 1 < m_entries.size()) { f << ','; }
        f << '\n';
    }
    f << "    ]\n}\n";
}
