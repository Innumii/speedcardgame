#include "utils/StringUtil.hpp"

#include <algorithm>
#include <sstream>

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

    std::vector<std::string> splitTrimmedLines(const std::string& text) {
        std::vector<std::string> lines;
        std::istringstream stream(text);
        std::string line;
        while (std::getline(stream, line)) {
            const std::string cleaned = trim(line);
            if (!cleaned.empty()) {
                lines.push_back(cleaned);
            }
        }
        return lines;
    }

    void splitAbilityTextAndFlavor(const std::string& text,
                                   std::vector<std::string>& abilities,
                                   std::string& flavor) {
        abilities.clear();
        flavor.clear();

        const std::string marker = "Flavor:";
        const auto markerPos = text.find(marker);
        if (markerPos != std::string::npos) {
            abilities = splitTrimmedLines(trim(text.substr(0, markerPos)));
            flavor = trim(text.substr(markerPos + marker.size()));
            return;
        }

        const std::string paragraphBreak = "\n\n";
        const auto breakPos = text.find(paragraphBreak);
        if (breakPos != std::string::npos) {
            abilities = splitTrimmedLines(trim(text.substr(0, breakPos)));
            flavor = trim(text.substr(breakPos + paragraphBreak.size()));
            return;
        }

        abilities = splitTrimmedLines(text);
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