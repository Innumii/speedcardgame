#include "utils/EnvUtil.hpp"
#include <cstdlib>

namespace EnvUtil {
    std::string getEnvOrDefault(const char* key, const char* fallback) {
        const char* v = std::getenv(key);
        return v ? std::string(v) : std::string(fallback);
    }

    int getEnvIntOrDefault(const char* key, int fallback) {
        const char* v = std::getenv(key);
        if (!v) return fallback;
        try { return std::stoi(v); } catch (...) { return fallback; }
    }
}