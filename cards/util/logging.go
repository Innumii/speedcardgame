package util

import (
	"log"
	"net/http"
	"os"
	"strconv"
	"time"
)

type loggingResponseWriter struct {
	http.ResponseWriter
	statusCode int
	bytes      int
}

func (rw *loggingResponseWriter) WriteHeader(code int) {
	rw.statusCode = code
	rw.ResponseWriter.WriteHeader(code)
}

func (rw *loggingResponseWriter) Write(data []byte) (int, error) {
	if rw.statusCode == 0 {
		rw.statusCode = http.StatusOK
	}
	n, err := rw.ResponseWriter.Write(data)
	rw.bytes += n
	return n, err
}

func GetEnvAsBool(key string, defaultValue bool) bool {
	raw := os.Getenv(key)
	if raw == "" {
		return defaultValue
	}
	value, err := strconv.ParseBool(raw)
	if err != nil {
		log.Printf("Invalid boolean value for %s=%q, using default %t", key, raw, defaultValue)
		return defaultValue
	}
	return value
}

func LogStartupConfiguration(serviceName string, debugEnabled, httpRequestLoggingEnabled bool) {
	log.Printf("[%s] DEBUG_LOG_ENABLED=%t HTTP_REQUEST_LOG_ENABLED=%t", serviceName, debugEnabled, httpRequestLoggingEnabled)
}

func HTTPRequestLogger(debugEnabled bool) func(http.Handler) http.Handler {
	return func(next http.Handler) http.Handler {
		return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
			start := time.Now()
			wrapped := &loggingResponseWriter{ResponseWriter: w, statusCode: http.StatusOK}

			next.ServeHTTP(wrapped, r)

			latency := time.Since(start)
			if debugEnabled {
				log.Printf("HTTP method=%s path=%s status=%d latency=%s ip=%s bytes=%d userAgent=%q referer=%q", r.Method, r.URL.RequestURI(), wrapped.statusCode, latency, r.RemoteAddr, wrapped.bytes, r.UserAgent(), r.Referer())
				return
			}

			log.Printf("HTTP method=%s path=%s status=%d latency=%s ip=%s bytes=%d", r.Method, r.URL.RequestURI(), wrapped.statusCode, latency, r.RemoteAddr, wrapped.bytes)
		})
	}
}