#ifndef STRING_UTIL_HPP
#define STRING_UTIL_HPP

#include <string>

namespace StringUtil {

    std::string trim(std::string value);

    std::string toUpper(std::string value);

    std::string addPrefix(const std::string& str, const std::string& prefix);
    std::string addSuffix(const std::string& str, const std::string& suffix);
    std::string addPrefixSuffix(const std::string& str, const std::string& prefix, const std::string& suffix);
}

#endif