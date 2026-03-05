#include "utils/EnvUtil.hpp"
#include <cstdlib>
#include <algorithm>
#include <cctype>
#include <fstream>
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

        std::string trim(std::string value) {
            value.erase(value.begin(), std::find_if(value.begin(), value.end(), [](unsigned char c) {
                return !std::isspace(c);
            }));
            value.erase(std::find_if(value.rbegin(), value.rend(), [](unsigned char c) {
                return !std::isspace(c);
            }).base(), value.end());
            return value;
        }

        void loadEnvFile(const std::string& path) {
            std::ifstream file(path);
            if (!file.is_open()) return;

            std::string line;
            while (std::getline(file, line)) {
                std::string item = trim(line);
                if (item.empty() || item[0] == '#') continue;

                if (item.rfind("export ", 0) == 0) {
                    item = trim(item.substr(7));
                }

                const std::size_t eq = item.find('=');
                if (eq == std::string::npos || eq == 0) continue;

                std::string key = trim(item.substr(0, eq));
                std::string value = trim(item.substr(eq + 1));

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
                ".env",
                "../.env"
            };

            for (const std::string& path : candidatePaths) {
                loadEnvFile(path);
            }
        }

        std::string toUpper(std::string value) {
            std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
                return static_cast<char>(std::toupper(c));
            });
            return value;
        }

        std::string buildServiceKey(const char* servicePrefix, const char* suffix) {
            std::string key = servicePrefix;
            key += suffix;
            return key;
        }

        std::string buildAwsServiceKey(const char* servicePrefix, const char* suffix) {
            std::string key = "AWS_";
            key += servicePrefix;
            key += suffix;
            return key;
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

        const std::string normalized = toUpper(trim(value));
        if (normalized == "1" || normalized == "TRUE" || normalized == "YES" || normalized == "Y" || normalized == "ON") {
            return true;
        }
        if (normalized == "0" || normalized == "FALSE" || normalized == "NO" || normalized == "N" || normalized == "OFF") {
            return false;
        }
        return fallback;
    }

    bool useAwsServices() {
        return getEnvBoolOrDefault("USE_AWS_SERVICES", false);
    }

    std::string getAuthServiceHost() {
        if (useAwsServices()) {
            return getServiceHost("AWS_AUTH_SERVICE_HOST", "127.0.0.1", "");
        } else {
            return getEnvOrDefault("AUTH_SERVICE_HOST", "");
        }
    }

    int getAuthServicePort() {
        if (useAwsServices()) {
            return getServicePort("AWS_AUTH_SERVICE_PORT", 8081, 443);
        } else {
            return getEnvIntOrDefault("AUTH_SERVICE_PORT", 8081);
        }
    }

    std::string getServiceHost(const char* servicePrefix, const char* localFallback, const char* awsFallback) {
        if (useAwsServices()) {
            const std::string awsKey = buildAwsServiceKey(servicePrefix, "_HOST");
            const char* awsConfigured = std::getenv(awsKey.c_str());
            if (awsConfigured && *awsConfigured) {
                return std::string(awsConfigured);
            }

            const char* sharedAwsHost = std::getenv("AWS_API_HOST");
            if (sharedAwsHost && *sharedAwsHost) {
                return std::string(sharedAwsHost);
            }

            if (awsFallback != nullptr && *awsFallback) {
                return std::string(awsFallback);
            }

            return std::string(localFallback);
        }

        const std::string key = buildServiceKey(servicePrefix, "_HOST");
        return getEnvOrDefault(key.c_str(), localFallback);
    }

    int getServicePort(const char* servicePrefix, int localFallback, int awsFallback) {
        if (useAwsServices()) {
            const std::string awsKey = buildAwsServiceKey(servicePrefix, "_PORT");
            return getEnvIntOrDefault(awsKey.c_str(), awsFallback);
        }

        const std::string key = buildServiceKey(servicePrefix, "_PORT");
        return getEnvIntOrDefault(key.c_str(), localFallback);
    }
}