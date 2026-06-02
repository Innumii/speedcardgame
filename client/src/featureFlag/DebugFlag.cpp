#include "featureFlag/DebugFlag.hpp"
#include "utils/EnvUtil.hpp"

namespace DebugFlag {
    bool getDebugEnvUtils() {
        return EnvUtil::getEnvBoolOrDefault("DEBUG_ENV_UTILS", debugEnvUtilsDefault);
    }

    bool getDebugEnvHttp() {
        return EnvUtil::getEnvBoolOrDefault("DEBUG_ENV_HTTP", debugEnvHttpDefault);
    }
}