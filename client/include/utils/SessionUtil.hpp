#pragma once
#include <string>

namespace SessionUtil {
    void set(const std::string& sessionId);
    const std::string& get();
    void clear();
    bool hasSession();
}