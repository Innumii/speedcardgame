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
	if os.Getenv("APP_ENV") == "production" {
		return true
	}
	awsEnabledStr := GetEnvOrDefault("AWS_ENABLED", "false")
	awsEnabled, err := strconv.ParseBool(awsEnabledStr)
	if err != nil {
		return true
	}
	return awsEnabled
}

func ResolveCardsServiceBaseURL() string {
	if IsAwsEnabled() {
		scheme := GetEnvOrDefault("AWS_CARDS_SERVICE_SCHEME", "https")
		host := GetEnvOrDefault("AWS_CARDS_SERVICE_HOST", "api.fylstudios.xyz")
		port := GetEnvOrDefault("AWS_CARDS_SERVICE_PORT", "443")
		if host != "" && port != "" {
			return scheme + "://" + host + ":" + port
		}
	}
	scheme := GetEnvOrDefault("CARDS_SERVICE_SCHEME", "http")
	host := GetEnvOrDefault("CARDS_SERVICE_HOST", "host.docker.internal")
	port := GetEnvOrDefault("CARDS_SERVICE_PORT", "8082")
	return scheme + "://" + host + ":" + port
}
