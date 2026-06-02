#include "utils/SessionUtil.hpp"

namespace {
    std::string gSessionId;
}

namespace SessionUtil {
    void set(const std::string& sessionId) {
        gSessionId = sessionId;
    }

    const std::string& get() {
        return gSessionId;
    }

    void clear() {
        gSessionId.clear();
    }

    bool hasSession() {
        return !gSessionId.empty();
    }
}