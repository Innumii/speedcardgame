#include "featureFlag/AnimationFlag.hpp"
#include "utils/EnvUtil.hpp"

namespace AnimationFlag {
    bool getAnimationsEnabled() {
        return EnvUtil::getEnvBoolOrDefault("ENABLE_ANIMATIONS", animationsEnabledDefault);
    }
}
