#ifndef ENV_UTIL_HPP
#define ENV_UTIL_HPP

#include <cstddef>
#include <string>

namespace EnvUtil {
    std::string getEnvOrDefault(const char* key, const char* fallback);
    int getEnvIntOrDefault(const char* key, int fallback);
}

#endif
