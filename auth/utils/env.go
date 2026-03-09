package utils

import (
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

func isAwsEnabled() bool {
	awsEnabledStr := GetEnvOrDefault("AWS_ENABLED", "false")
	awsEnabled, err := strconv.ParseBool(awsEnabledStr)
	if err != nil {
		return true
	}
	return awsEnabled
}

func ResolveCardsServiceBaseURL() string {
	if isAwsEnabled() {
		host := GetEnvOrDefault("AWS_CARDS_SERVICE_HOST", "api.fylstudios.xyz")
		port := GetEnvOrDefault("AWS_CARDS_SERVICE_PORT", "80")
		if host != "" && port != "" {
			return "http://" + host + ":" + port
		}
	}
	host := GetEnvOrDefault("CARDS_SERVICE_HOST", "host.docker.internal")
	port := GetEnvOrDefault("CARDS_SERVICE_PORT", "8082")
	return "http://" + host + ":" + port
}