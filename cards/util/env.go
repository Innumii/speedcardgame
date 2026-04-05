package util

import (
	"log"
	"os"
	"strconv"
)

// GetEnvOrDefault retrieves the value of the environment variable named by the key. If the variable is not present, it returns the default value provided.
func GetEnvOrDefault(key, defaultValue string) string {
	if value, exists := os.LookupEnv(key); exists {
		return value
	}
	return defaultValue
}

func GetEnvAsBool(key string, defaultValue bool) bool {
	raw := GetEnvOrDefault(key, strconv.FormatBool(defaultValue))
	value, err := strconv.ParseBool(raw)
	if err != nil {
		log.Printf("Invalid boolean value for %s=%q, using default %t", key, raw, defaultValue)
		return defaultValue
	}
	return value
}

// IsAwsEnabled checks if AWS integration is enabled.
// Defaults to true if APP_ENV is "production", otherwise false.
func IsAwsEnabled() bool {
	return GetEnvAsBool("AWS_ENABLED", false)
}

func ResolveCardsServiceBaseURL() string {
	if IsAwsEnabled() {
		host := GetEnvOrDefault("AWS_CARDS_SERVICE_HOST", "api.fylstudios.xyz")
		port := GetEnvOrDefault("AWS_CARDS_SERVICE_PORT", "443")
		if host != "" && port != "" {
			return "https://" + host + ":" + port
		}
	}
	host := GetEnvOrDefault("CARDS_SERVICE_HOST", "host.docker.internal")
	port := GetEnvOrDefault("CARDS_SERVICE_PORT", "8082")
	return "https://" + host + ":" + port
}
