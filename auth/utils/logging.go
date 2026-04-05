package utils

import (
	"fmt"
	"log"
	"net/http"
	"strings"
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

func requestScheme(req *http.Request) string {
	if forwardedProto := strings.TrimSpace(req.Header.Get("X-Forwarded-Proto")); forwardedProto != "" {
		parts := strings.Split(forwardedProto, ",")
		if len(parts) > 0 {
			candidate := strings.TrimSpace(parts[0])
			if candidate != "" {
				return candidate
			}
		}
	}

	if req.TLS != nil {
		return "https"
	}

	if req.URL != nil && req.URL.Scheme != "" {
		return req.URL.Scheme
	}

	return "http"
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

		scheme := requestScheme(c.Request)

		if errors != "" {
			log.Printf("scheme=%s method=%s path=%s status=%d latency=%s ip=%s bytes=%d errors=%s", scheme, method, path, statusCode, latency, clientIP, c.Writer.Size(), errors)
			return
		}

		if debugEnabled {
			log.Printf("scheme=%s method=%s path=%s status=%d latency=%s ip=%s bytes=%d userAgent=%q referer=%q", scheme, method, path, statusCode, latency, clientIP, c.Writer.Size(), c.Request.UserAgent(), c.Request.Referer())
			return
		}

		log.Printf("scheme=%s method=%s path=%s status=%d latency=%s ip=%s bytes=%d", scheme, method, path, statusCode, latency, clientIP, c.Writer.Size())
	}
}
