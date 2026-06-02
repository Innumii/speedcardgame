package middleware

import (
	"net/http"
	"os"
	"strings"

	"github.com/FYL-Studios/speedcardgame/auth/services"
	"github.com/gin-gonic/gin"
)

func RequireAuth(sessionService *services.SessionService) gin.HandlerFunc {
	return func(c *gin.Context) {
		// Allow internal service-to-service calls via API key
		internalKey := os.Getenv("INTERNAL_API_KEY")
		if internalKey != "" && c.GetHeader("X-Internal-Key") == internalKey {
			c.Set("userID", "internal")
			c.Next()
			return
		}

		sessionID := strings.TrimSpace(c.GetHeader("X-Session-ID"))
		if sessionID == "" {
			c.AbortWithStatusJSON(http.StatusUnauthorized, gin.H{"error": "not authenticated"})
			return
		}

		userID, err := sessionService.GetSessionBySessionId(c.Request.Context(), sessionID)
		if err != nil || userID == "" {
			c.AbortWithStatusJSON(http.StatusUnauthorized, gin.H{"error": "invalid or expired session"})
			return
		}

		c.Set("userID", userID)
		c.Next()
	}
}
