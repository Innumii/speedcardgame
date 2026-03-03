#ifndef HTTP_UTIL_HPP
#define HTTP_UTIL_HPP

#include <string>

namespace HttpUtil {
    bool sendHttp(const std::string& host, int port, const std::string& method,
                  const std::string& path, const std::string& body,
                  int& statusCode, std::string& responseBody);
}

#endif
