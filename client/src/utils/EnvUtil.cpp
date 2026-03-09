#include "utils/EnvUtil.hpp"
#include "utils/StringUtil.hpp"
#include <cstdlib>
#include <cctype>
#include <fstream>
#include <stdexcept>
#include <vector>

namespace EnvUtil {
    namespace {
        bool envLoaded = false;

        void setProcessEnvVar(const std::string& key, const std::string& value) {
#ifdef _WIN32
            _putenv_s(key.c_str(), value.c_str());
#else
            setenv(key.c_str(), value.c_str(), 1);
#endif
        }

        void loadEnvFile(const std::string& path) {
            std::ifstream file(path);
            if (!file.is_open()) return;

            std::string line;
            while (std::getline(file, line)) {
                std::string item = StringUtil::trim(line);
                if (item.empty() || item[0] == '#') continue;

                if (item.rfind("export ", 0) == 0) {
                    item = StringUtil::trim(item.substr(7));
                }

                const std::size_t eq = item.find('=');
                if (eq == std::string::npos || eq == 0) continue;

                std::string key = StringUtil::trim(item.substr(0, eq));
                std::string value = StringUtil::trim(item.substr(eq + 1));

                if (value.size() >= 2) {
                    const char first = value.front();
                    const char last = value.back();
                    if ((first == '"' && last == '"') || (first == '\'' && last == '\'')) {
                        value = value.substr(1, value.size() - 2);
                    }
                }

                if (!key.empty()) {
                    setProcessEnvVar(key, value);
                }
            }
        }

        void ensureEnvLoaded() {
            if (envLoaded) return;
            envLoaded = true;

            const std::vector<std::string> candidatePaths = {
                "./env/.env"
            };

            for (const std::string& path : candidatePaths) {
                loadEnvFile(path);
            }
        }
    }

    std::string getEnvOrDefault(const char* key, const char* fallback) {
        ensureEnvLoaded();
        const char* v = std::getenv(key);
        return v ? std::string(v) : std::string(fallback);
    }

    int getEnvIntOrDefault(const char* key, int fallback) {
        ensureEnvLoaded();
        const char* v = std::getenv(key);
        if (!v) return fallback;
        try { return std::stoi(v); } catch (...) { return fallback; }
    }

    bool getEnvBoolOrDefault(const char* key, bool fallback) {
        ensureEnvLoaded();
        const char* value = std::getenv(key);
        if (!value) return fallback;

        const std::string normalized = StringUtil::toUpper(StringUtil::trim(value));
        if (normalized == "1" || normalized == "TRUE" || normalized == "YES" || normalized == "Y" || normalized == "ON") {
            return true;
        }
        if (normalized == "0" || normalized == "FALSE" || normalized == "NO" || normalized == "N" || normalized == "OFF") {
            return false;
        }
        return fallback;
    }

    bool isAwsEnabled() {
        return getEnvBoolOrDefault("USE_AWS_SERVICES", false);
    }

    std::string resolveEndpoint(const std::string& dockerKey, const std::string& awsKey) {

        if (!isAwsEnabled()) {
            const std::string host = getEnvOrDefault(StringUtil::addSuffix(dockerKey, "_HOST").c_str(), "localhost");
            const std::string port = getEnvOrDefault(StringUtil::addSuffix(dockerKey, "_PORT").c_str(), "8080");

            return host + ":" + port;
        }

        const std::string awsHost = getEnvOrDefault(StringUtil::addSuffix(awsKey, "_HOST").c_str(), "");
        const std::string awsPort = getEnvOrDefault(StringUtil::addSuffix(awsKey, "_PORT").c_str(), "8080");
        if (!awsHost.empty()) {
            return awsHost + ":" + awsPort;
        }
    }

    std::string resolveHostOrPort(const std::string& dockerKey, const std::string& awsKey, bool resolveHost) {
        const std::string dockerValue = getEnvOrDefault(StringUtil::addSuffix(dockerKey, resolveHost ? "_HOST" : "_PORT").c_str(), resolveHost ? "localhost" : "8080");
        const std::string awsValue = getEnvOrDefault(StringUtil::addSuffix(awsKey, resolveHost ? "_HOST" : "_PORT").c_str(), "");

        return !awsValue.empty() ? awsValue : dockerValue;
    }

    // Auth service
    std::string getAuthServiceHost() {
        return resolveHostOrPort("DOCKER_AUTH_SERVICE", "AWS_AUTH_SERVICE", true);
    }

    int getAuthServicePort() {
        const std::string portStr = resolveHostOrPort("DOCKER_AUTH_SERVICE", "AWS_AUTH_SERVICE", false);
        try { return std::stoi(portStr); } catch (...) { return 8080; }
    }

    std::string getAuthServiceEndpoint() {
        return resolveEndpoint(
            "DOCKER_AUTH_SERVICE",
            "AWS_AUTH_SERVICE"
        );
    }

    // Cards service
    std::string getCardsServiceHost() {
        return resolveHostOrPort("DOCKER_CARDS_SERVICE", "AWS_CARDS_SERVICE", true);
    }

    int getCardsServicePort() {
        const std::string portStr = resolveHostOrPort("DOCKER_CARDS_SERVICE", "AWS_CARDS_SERVICE", false);
        try { return std::stoi(portStr); } catch (...) { return 8080; }
    }

    std::string getCardsServiceEndpoint() {
        return resolveEndpoint(
            "DOCKER_CARDS_SERVICE",
            "AWS_CARDS_SERVICE"
        );
    }

    // Game server
    std::string getGameServerHost() {
        return resolveHostOrPort("DOCKER_GAME_SERVER", "AWS_GAME_SERVER", true);
    }

    int getGameServerPort() {
        const std::string portStr = resolveHostOrPort("DOCKER_GAME_SERVER", "AWS_GAME_SERVER", false);
        try { return std::stoi(portStr); } catch (...) { return 8080; }
    }

    std::string getGameServerEndpoint() {
        return resolveEndpoint(
            "DOCKER_GAME_SERVER",
            "AWS_GAME_SERVER"
        );
    }

}