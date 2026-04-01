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

func isAwsEnabled() bool {
	awsEnabledStr := GetEnvOrDefault("AWS_ENABLED", "false")
	awsEnabled, err := strconv.ParseBool(awsEnabledStr)
	if err != nil {
		return false
	}
	return awsEnabled
}

func ResolveCardsServiceBaseURLs() []string {
	candidates := make([]string, 0, 3)
	seen := make(map[string]struct{})

	appendURL := func(raw string) {
		url := strings.TrimSpace(raw)
		if url == "" {
			return
		}
		if _, exists := seen[url]; exists {
			return
		}
		seen[url] = struct{}{}
		candidates = append(candidates, url)
	}

	if baseURL, exists := os.LookupEnv("CARDS_SERVICE_BASE_URL"); exists {
		appendURL(baseURL)
	}

	localScheme := GetEnvOrDefault("CARDS_SERVICE_SCHEME", "https")
	localHost := GetEnvOrDefault("CARDS_SERVICE_HOST", "host.docker.internal")
	localPort := GetEnvOrDefault("CARDS_SERVICE_PORT", "8080")
	if localHost != "" && localPort != "" {
		appendURL(localScheme + "://" + localHost + ":" + localPort)
	}

	if isAwsEnabled() {
		awsScheme := GetEnvOrDefault("AWS_CARDS_SERVICE_SCHEME", "https")
		awsHost := GetEnvOrDefault("AWS_CARDS_SERVICE_HOST", localHost)
		awsPort := GetEnvOrDefault("AWS_CARDS_SERVICE_PORT", localPort)
		if awsHost != "" && awsPort != "" {
			appendURL(awsScheme + "://" + awsHost + ":" + awsPort)
		}
	}

	return candidates
}

func ResolveCardsServiceBaseURL() string {
	urls := ResolveCardsServiceBaseURLs()
	if len(urls) > 0 {
		return urls[0]
	}
	return "https://host.docker.internal:8080"
}
