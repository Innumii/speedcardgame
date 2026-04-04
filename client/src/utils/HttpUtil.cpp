#include "utils/HttpUtil.hpp"

#include <array>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include "utils/EnvUtil.hpp"

#define CPPHTTPLIB_OPENSSL_SUPPORT
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#pragma GCC diagnostic ignored "-Wconversion-null"
#endif
#include "httplib/httplib.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

namespace HttpUtil {
    namespace {
        std::string normalizeHost(const std::string& host) {
            if (host.rfind("http://", 0) == 0) {
                return host.substr(7);
            }
            if (host.rfind("https://", 0) == 0) {
                return host.substr(8);
            }
            return host;
        }

        bool fileExists(const std::string& path) {
            std::ifstream file(path);
            return file.good();
        }

        bool configureTrustStore(httplib::SSLClient& client) {
            const char* explicitCaPath = std::getenv("API_CA_CERT_PATH");
            if (explicitCaPath != nullptr && std::strlen(explicitCaPath) > 0) {
                const std::string path(explicitCaPath);
                if (!fileExists(path)) {
                    return false;
                }
                client.set_ca_cert_path(path.c_str());
                return true;
            }

            // In AWS mode, API endpoints are expected to use ACM/public chains.
            // Prefer system trust roots unless an explicit API_CA_CERT_PATH is set.
            if (EnvUtil::isAwsEnabled()) {
                return true;
            }

            static const std::array<const char*, 4> candidatePaths = {
                "./certs/ca.crt",
                "../certs/ca.crt",
                "client/certs/ca.crt",
                "/certs/ca.crt",
            };

            for (const char* candidatePath : candidatePaths) {
                if (!fileExists(candidatePath)) {
                    continue;
                }
                client.set_ca_cert_path(candidatePath);
                return true;
            }

            // Fall back to system trust roots (required for ACM/public certs in AWS).
            return true;
        }

        bool shouldUseHttps() {
            return true;  // Default to HTTPS for all service-to-service communication
        }
    }

    bool sendHttpWithHeaders(const std::string& host, int port, const std::string& method,
                             const std::string& path, const std::string& body,
                             const std::map<std::string, std::string>& headers,
                             int& statusCode, std::string& responseBody) {

        const bool useHttps = (port == 443);
        httplib::Result res;
        const std::string normalizedHost = normalizeHost(host);

        if (useHttps) {
            httplib::SSLClient client(normalizedHost.c_str(), port);
            client.enable_server_certificate_verification(true);
            if (!configureTrustStore(client)) {
                statusCode = -1;
                responseBody.clear();
                return false;
            }
            client.set_follow_location(true);

            httplib::Headers requestHeaders;
            for (const auto& entry : headers) {
                requestHeaders.emplace(entry.first, entry.second);
            }

            if (method == "GET") res = client.Get(path.c_str(), requestHeaders);
            else if (method == "POST") res = client.Post(path.c_str(), requestHeaders, body, "application/json");
            else if (method == "PUT") res = client.Put(path.c_str(), requestHeaders, body, "application/json");
            else if (method == "PATCH") res = client.Patch(path.c_str(), requestHeaders, body, "application/json");
            else if (method == "DELETE") res = client.Delete(path.c_str(), requestHeaders, body, "application/json");
            else return false;
        }

        if (!res) {
            statusCode = -1;
            responseBody.clear();
            return false;
        }

        statusCode = res->status;
        responseBody = res->body;
        return true;
    }

    bool sendHttp(const std::string& host, int port, const std::string& method,
                  const std::string& path, const std::string& body,
                  int& statusCode, std::string& responseBody) {
        const std::map<std::string, std::string> noHeaders;
        return sendHttpWithHeaders(host, port, method, path, body, noHeaders, statusCode, responseBody);
    }
}
