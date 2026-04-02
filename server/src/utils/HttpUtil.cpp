#include "utils/HttpUtil.hpp"
#include "utils/EnvUtil.hpp"

#include <fstream>
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

        bool tryLoadCaCert(httplib::SSLClient& client) {
            // For the C++ game server running in Docker, the shared
            // CA bundle is mounted at /certs/ca.crt. Use this single
            // canonical path instead of guessing.
            constexpr const char* kCaPath = "/certs/ca.crt";

            std::ifstream file(kCaPath);
            if (!file.good()) {
                return false;
            }

            client.set_ca_cert_path(kCaPath);
            return true;
        }

        bool shouldUseHttps() {
            return true;
        }

        bool shouldVerifyCerts() {
            return EnvUtil::getEnvBoolOrDefault("TLS_VERIFY_CERTS", true);
        }
    }

    bool sendHttp(const std::string& host, int port, const std::string& method,
                  const std::string& path, const std::string& body,
                  int& statusCode, std::string& responseBody) {

        const std::string normalizedHost = normalizeHost(host);
        const bool useHttps = shouldUseHttps();
        httplib::Result res;

        if (useHttps) {
            httplib::SSLClient client(normalizedHost.c_str(), port);
            const bool verifyCerts = shouldVerifyCerts();
            client.enable_server_certificate_verification(verifyCerts);
            if (verifyCerts) {
                tryLoadCaCert(client);
            }
            client.set_follow_location(true);

            if (method == "GET") res = client.Get(path.c_str());
            else if (method == "POST") res = client.Post(path.c_str(), body, "application/json");
            else if (method == "PUT") res = client.Put(path.c_str(), body, "application/json");
            else if (method == "PATCH") res = client.Patch(path.c_str(), body, "application/json");
            else if (method == "DELETE") res = client.Delete(path.c_str());
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
}
