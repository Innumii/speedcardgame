#define DEBUG_FLAG_HPP
#ifdef DEBUG_FLAG_HPP

namespace DebugFlag {
    // Set this to true to enable debug output for the cards service endpoint resolution
    inline bool debugEnvUtilsDefault = false;
    inline bool debugEnvHttpDefault = false;

    // This flag can be set via the environment variable DEBUG_ENV_UTILS=1
    bool getDebugEnvUtils();
    bool getDebugEnvHttp();
}

#endif // DEBUG_FLAG_HPP