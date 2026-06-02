#ifndef ENV_UTIL_HPP
#define ENV_UTIL_HPP

#include <cstddef>
#include <string>

namespace EnvUtil {
    std::string getEnvOrDefault(const char* key, const char* fallback);
    int getEnvIntOrDefault(const char* key, int fallback);
    bool getEnvBoolOrDefault(const char* key, bool fallback);

    // AWS-specific helpers
    bool isAwsEnabled();
    std::string resolveEndpoint(const std::string& dockerKey, const std::string& awsKey);
    std::string resolveHostOrPort(const std::string& dockerKey, const std::string& awsKey, bool resolveHost);

    // Auth service
    std::string getAuthServiceEndpoint();
    std::string getAuthServiceHost();
    int getAuthServicePort();

    // Cards service
    std::string getCardsServiceEndpoint();
    std::string getCardsServiceHost();
    int getCardsServicePort();

    // Game server
    std::string getGameServerEndpoint();
    std::string getGameServerHost();
    int getGameServerPort();
}

#endif
