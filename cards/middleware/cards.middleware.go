package middleware

import (
	"net/http"
	"os"
	"strings"

	"context"

	"github.com/redis/go-redis/v9"
)

func RequireAuth(redisClient *redis.Client) func(http.Handler) http.Handler {
	return func(next http.Handler) http.Handler {
		return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
			// Allow internal service-to-service calls via API key
			internalKey := os.Getenv("INTERNAL_API_KEY")
			if internalKey != "" && r.Header.Get("X-Internal-Key") == internalKey {
				next.ServeHTTP(w, r)
				return
			}

			// Otherwise require a valid user session
			sessionID := strings.TrimSpace(r.Header.Get("X-Session-ID"))
			if sessionID == "" {
				http.Error(w, `{"error":"not authenticated"}`, http.StatusUnauthorized)
				return
			}

			userID, err := redisClient.Get(context.Background(), "session:"+sessionID).Result()
			if err != nil || userID == "" {
				http.Error(w, `{"error":"invalid or expired session"}`, http.StatusUnauthorized)
				return
			}

			next.ServeHTTP(w, r)
		})
	}
}
