#include "utils/JsonUtil.hpp"

#include <cctype>

namespace JsonUtil {
    bool readJsonStringField(const std::string& json, const std::string& key, std::string& out) {
        const std::string needle = "\"" + key + "\"";
        std::size_t pos = json.find(needle);
        if (pos == std::string::npos) return false;
        pos = json.find(':', pos + needle.size());
        if (pos == std::string::npos) return false;
        pos = json.find('"', pos);
        if (pos == std::string::npos) return false;
        std::size_t end = pos + 1;
        while (end < json.size()) {
            if (json[end] == '"' && json[end - 1] != '\\') break;
            ++end;
        }
        if (end >= json.size()) return false;
        out = json.substr(pos + 1, end - pos - 1);
        return true;
    }

    bool readJsonIntField(const std::string& json, const std::string& key, int& out) {
        const std::string needle = "\"" + key + "\"";
        std::size_t pos = json.find(needle);
        if (pos == std::string::npos) return false;
        pos = json.find(':', pos + needle.size());
        if (pos == std::string::npos) return false;
        ++pos;
        while (pos < json.size() && std::isspace(static_cast<unsigned char>(json[pos]))) {
            ++pos;
        }
        std::size_t end = pos;
        if (end < json.size() && json[end] == '-') {
            ++end;
        }
        while (end < json.size() && std::isdigit(static_cast<unsigned char>(json[end]))) {
            ++end;
        }
        if (end == pos) return false;
        try {
            out = std::stoi(json.substr(pos, end - pos));
        } catch (...) {
            return false;
        }
        return true;
    }

    bool findMatchingBrace(const std::string& text, std::size_t openPos, std::size_t& closePos) {
        if (openPos >= text.size() || text[openPos] != '{') return false;
        int depth = 0;
        for (std::size_t i = openPos; i < text.size(); ++i) {
            if (text[i] == '{') {
                ++depth;
            } else if (text[i] == '}') {
                --depth;
                if (depth == 0) {
                    closePos = i;
                    return true;
                }
            }
        }
        return false;
    }

    bool extractJsonObject(const std::string& json, const std::string& key, std::string& out) {
        const std::string needle = "\"" + key + "\"";
        std::size_t pos = json.find(needle);
        if (pos == std::string::npos) return false;
        pos = json.find('{', pos + needle.size());
        if (pos == std::string::npos) return false;
        std::size_t closePos = std::string::npos;
        if (!findMatchingBrace(json, pos, closePos)) return false;
        out = json.substr(pos, closePos - pos + 1);
        return true;
    }

    bool parseJsonIntAt(const std::string& text, std::size_t& pos, int& out) {
        while (pos < text.size() && std::isspace(static_cast<unsigned char>(text[pos]))) {
            ++pos;
        }
        std::size_t start = pos;
        if (pos < text.size() && text[pos] == '-') {
            ++pos;
        }
        while (pos < text.size() && std::isdigit(static_cast<unsigned char>(text[pos]))) {
            ++pos;
        }
        if (pos == start) return false;
        try {
            out = std::stoi(text.substr(start, pos - start));
        } catch (...) {
            return false;
        }
        return true;
    }

    bool parseJsonQuotedStringAt(const std::string& text, std::size_t& pos, std::string& out) {
        while (pos < text.size() && std::isspace(static_cast<unsigned char>(text[pos]))) {
            ++pos;
        }
        if (pos >= text.size() || text[pos] != '"') return false;
        ++pos;
        std::size_t start = pos;
        while (pos < text.size()) {
            if (text[pos] == '"' && text[pos - 1] != '\\') {
                out = text.substr(start, pos - start);
                ++pos;
                return true;
            }
            ++pos;
        }
        return false;
    }
}
