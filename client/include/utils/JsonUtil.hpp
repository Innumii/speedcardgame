#ifndef JSON_UTIL_HPP
#define JSON_UTIL_HPP

#include <cstddef>
#include <string>

namespace JsonUtil {
    std::string escapeJsonString(const std::string& value);
    bool readJsonStringField(const std::string& json, const std::string& key, std::string& out);
    bool readJsonIntField(const std::string& json, const std::string& key, int& out);
    bool findMatchingBrace(const std::string& text, std::size_t openPos, std::size_t& closePos);
    bool extractJsonObject(const std::string& json, const std::string& key, std::string& out);
    bool parseJsonIntAt(const std::string& text, std::size_t& pos, int& out);
    bool parseJsonQuotedStringAt(const std::string& text, std::size_t& pos, std::string& out);
}

#endif
