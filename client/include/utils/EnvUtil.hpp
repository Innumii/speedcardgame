#ifndef ENV_UTIL_HPP
#define ENV_UTIL_HPP

#include <cstddef>
#include <string>

namespace EnvUtil {
    std::string getEnvOrDefault(const char* key, const char* fallback);
    int getEnvIntOrDefault(const char* key, int fallback);
    bool getEnvBoolOrDefault(const char* key, bool fallback);
    bool useAwsServices();
    std::string getServiceHost(const char* servicePrefix, const char* localFallback, const char* awsFallback);
    int getServicePort(const char* servicePrefix, int localFallback, int awsFallback);
}

#endif
