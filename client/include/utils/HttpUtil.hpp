#ifndef HTTP_UTIL_HPP
#define HTTP_UTIL_HPP

#include <map>
#include <string>

namespace HttpUtil {
    bool sendHttpWithHeaders(const std::string& host, int port, const std::string& method,
                             const std::string& path, const std::string& body,
                             const std::map<std::string, std::string>& headers,
                             int& statusCode, std::string& responseBody);

    bool sendHttp(const std::string& host, int port, const std::string& method,
                  const std::string& path, const std::string& body,
                  int& statusCode, std::string& responseBody);
}

#endif
