#include "utils/StringUtil.hpp"

#include <string>
#include <algorithm>

namespace StringUtil {

    std::string trim(std::string value) {
            value.erase(value.begin(), std::find_if(value.begin(), value.end(), [](unsigned char c) {
                return !std::isspace(c);
            }));
            value.erase(std::find_if(value.rbegin(), value.rend(), [](unsigned char c) {
                return !std::isspace(c);
            }).base(), value.end());
            return value;
        }

    std::string toUpper(std::string value) {
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
            return static_cast<char>(std::toupper(c));
        });
        return value;
    }

    std::string addPrefix(const std::string& str, const std::string& prefix) {
        return prefix + str;
    }

    std::string addSuffix(const std::string& str, const std::string& suffix) {
        return str + suffix;
    }

    std::string addPrefixSuffix(const std::string& str, const std::string& prefix, const std::string& suffix) {
        return prefix + str + suffix;
    }
}   