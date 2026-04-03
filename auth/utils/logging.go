package utils

import (
	"fmt"
	"log"
	"time"

	"github.com/gin-gonic/gin"
)

func ConfigureGinMode(debugEnabled bool) {
	if debugEnabled {
		gin.SetMode(gin.DebugMode)
		return
	}
	gin.SetMode(gin.ReleaseMode)
}

func LogStartupConfiguration(serviceName string, debugEnabled, httpRequestLoggingEnabled bool) {
	log.Printf("[%s] DEBUG_LOG_ENABLED=%t HTTP_REQUEST_LOG_ENABLED=%t", serviceName, debugEnabled, httpRequestLoggingEnabled)
}

func GinRequestLogger(debugEnabled bool) gin.HandlerFunc {
	return func(c *gin.Context) {
		start := time.Now()
		path := c.Request.URL.Path
		query := c.Request.URL.RawQuery

		c.Next()

		latency := time.Since(start)
		statusCode := c.Writer.Status()
		clientIP := c.ClientIP()
		method := c.Request.Method
		errors := c.Errors.ByType(gin.ErrorTypePrivate).String()

		if query != "" {
			path = fmt.Sprintf("%s?%s", path, query)
		}

		if errors != "" {
			log.Printf("HTTPS method=%s path=%s status=%d latency=%s ip=%s bytes=%d errors=%s", method, path, statusCode, latency, clientIP, c.Writer.Size(), errors)
			return
		}

		if debugEnabled {
			log.Printf("HTTPS method=%s path=%s status=%d latency=%s ip=%s bytes=%d userAgent=%q referer=%q", method, path, statusCode, latency, clientIP, c.Writer.Size(), c.Request.UserAgent(), c.Request.Referer())
			return
		}

		log.Printf("HTTPS method=%s path=%s status=%d latency=%s ip=%s bytes=%d", method, path, statusCode, latency, clientIP, c.Writer.Size())
	}
}
