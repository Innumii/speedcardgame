#include "utils/HttpUtil.hpp"

#include <cstdlib>
#include <map>

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

namespace {
    std::string normalizeHost(const std::string& host) {
        std::string normalized = host;

        if (normalized.rfind("http://", 0) == 0) {
            normalized = normalized.substr(7);
        } else if (normalized.rfind("https://", 0) == 0) {
            normalized = normalized.substr(8);
        }

        while (!normalized.empty() && normalized.back() == '/') {
            normalized.pop_back();
        }

        return normalized;
    }
}

namespace HttpUtil {
    bool sendHttp(const std::string& host, int port, const std::string& method,
                  const std::string& path, const std::string& body,
                  int& statusCode, std::string& responseBody) {

        httplib::Result res;
        const std::string normalizedHost = normalizeHost(host);

        httplib::SSLClient client(normalizedHost.c_str(), port);
        client.enable_server_certificate_verification(false);
        client.set_follow_location(true);

        httplib::Headers headers;
        const char* key = std::getenv("INTERNAL_API_KEY");
        if (key && key[0] != '\0') {
            headers.emplace("X-Internal-Key", key);
        }

        if (method == "GET") res = client.Get(path.c_str(), headers);
        else if (method == "POST") res = client.Post(path.c_str(), headers, body, "application/json");
        else if (method == "PUT") res = client.Put(path.c_str(), headers, body, "application/json");
        else if (method == "PATCH") res = client.Patch(path.c_str(), headers, body, "application/json");
        else if (method == "DELETE") res = client.Delete(path.c_str(), headers, body, "application/json");
        else return false;

        if (!res) {
            statusCode = -1;
            responseBody.clear();
            auto err = res.error();
            std::cerr << "[HttpUtil] Request failed: " << httplib::to_string(err)
                    << " | " << method << " https://" << normalizedHost << ":" << port << path << "\n";
            return false;
        }

        statusCode = res->status;
        responseBody = res->body;
        return true;
    }
}