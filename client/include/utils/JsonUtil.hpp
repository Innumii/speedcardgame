#ifndef JSON_UTIL_HPP
#define JSON_UTIL_HPP

#include <cstddef>
#include <string>

namespace JsonUtil {
    /**
     * Escapes special characters for safe placement inside a JSON string value.
     * @param value Raw input string.
     * @return Escaped JSON string fragment.
     */
    std::string escapeJsonString(const std::string& value);
    bool readJsonStringField(const std::string& json, const std::string& key, std::string& out);
    bool readJsonIntField(const std::string& json, const std::string& key, int& out);
    bool findMatchingBrace(const std::string& text, std::size_t openPos, std::size_t& closePos);
    bool extractJsonObject(const std::string& json, const std::string& key, std::string& out);
    /**
     * Finds the object matching a given user id in a JSON collection and extracts
     * its nested cards object content without outer braces.
     * @param json Source JSON text containing objects with "uid" and "cards" fields.
     * @param userId User id to match against each object's uid field.
     * @param outCards Output containing cards object content (without braces) on success.
     * @return True if a matching user object is found and cards are extracted.
     */
    bool extractCardsObjectForUser(const std::string& json, int userId, std::string& outCards);
    bool parseJsonIntAt(const std::string& text, std::size_t& pos, int& out);
    bool parseJsonQuotedStringAt(const std::string& text, std::size_t& pos, std::string& out);
}

#endif
