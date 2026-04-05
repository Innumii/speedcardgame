#include "utils/HttpUtil.hpp"

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
    bool sendHttpWithHeaders(const std::string& host, int port, const std::string& method,
                             const std::string& path, const std::string& body,
                             const std::map<std::string, std::string>& headers,
                             int& statusCode, std::string& responseBody) {

        httplib::Result res;
        const std::string normalizedHost = normalizeHost(host);

        httplib::SSLClient client(normalizedHost.c_str(), port);
        client.enable_server_certificate_verification(false);
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
