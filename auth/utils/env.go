package utils

import (
	"log"
	"os"
	"strconv"
	"strings"
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

func ResolveCardsServiceBaseURL() string {

	normalizeHost := func(raw string) string {
		host := strings.TrimSpace(raw)
		host = strings.TrimPrefix(host, "http://")
		host = strings.TrimPrefix(host, "https://")
		host = strings.TrimRight(host, "/")
		return host
	}

	if GetEnvAsBool("AWS_ENABLED", false) {
		host := normalizeHost(GetEnvOrDefault("AWS_CARDS_SERVICE_HOST", "api.fylstudios.xyz"))
		port := GetEnvOrDefault("AWS_CARDS_SERVICE_PORT", "443")
		if host != "" && port != "" {
			return "https://" + host + ":" + port
		}
	}

	host := normalizeHost(GetEnvOrDefault("CARDS_SERVICE_HOST", "host.docker.internal"))
	port := GetEnvOrDefault("CARDS_SERVICE_PORT", "8082")
	return "https://" + host + ":" + port
}
