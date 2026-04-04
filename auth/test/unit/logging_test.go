package services_test

import (
	"bytes"
	"log"
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"

	"github.com/FYL-Studios/speedcardgame/auth/utils"
	"github.com/gin-gonic/gin"
)

func captureLogOutput(t *testing.T) (*bytes.Buffer, func()) {
	t.Helper()
	var buffer bytes.Buffer
	previousOutput := log.Writer()
	previousFlags := log.Flags()
	log.SetOutput(&buffer)
	log.SetFlags(0)
	return &buffer, func() {
		log.SetOutput(previousOutput)
		log.SetFlags(previousFlags)
	}
}

func TestGinRequestLogger_ReportsForwardedHttpsScheme(t *testing.T) {
	gin.SetMode(gin.ReleaseMode)
	buffer, restore := captureLogOutput(t)
	defer restore()

	router := gin.New()
	router.Use(utils.GinRequestLogger(false))
	router.GET("/login", func(c *gin.Context) {
		c.Status(http.StatusOK)
	})

	req := httptest.NewRequest(http.MethodGet, "http://example.com/login?source=test", nil)
	req.Header.Set("X-Forwarded-Proto", "https")
	rr := httptest.NewRecorder()
	router.ServeHTTP(rr, req)

	logLine := buffer.String()
	if !strings.Contains(logLine, "scheme=https") {
		t.Fatalf("expected log line to include scheme=https, got %q", logLine)
	}
	if !strings.Contains(logLine, "method=GET") || !strings.Contains(logLine, "path=/login?source=test") {
		t.Fatalf("expected log line to include request details, got %q", logLine)
	}
}

func TestGinRequestLogger_DefaultsToHttpScheme(t *testing.T) {
	gin.SetMode(gin.ReleaseMode)
	buffer, restore := captureLogOutput(t)
	defer restore()

	router := gin.New()
	router.Use(utils.GinRequestLogger(false))
	router.GET("/status", func(c *gin.Context) {
		c.Status(http.StatusNoContent)
	})

	req := httptest.NewRequest(http.MethodGet, "http://example.com/status", nil)
	rr := httptest.NewRecorder()
	router.ServeHTTP(rr, req)

	logLine := buffer.String()
	if !strings.Contains(logLine, "scheme=http") {
		t.Fatalf("expected log line to include scheme=http, got %q", logLine)
	}
}
