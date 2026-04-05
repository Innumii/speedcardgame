package services_test

import (
	"bytes"
	"log"
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"

	"github.com/FYL-Studios/speedcardgame/cards/util"
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

func TestHTTPRequestLogger_ReportsForwardedHttpsScheme(t *testing.T) {
	buffer, restore := captureLogOutput(t)
	defer restore()

	middleware := util.HTTPRequestLogger(false)
	handler := middleware(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		w.WriteHeader(http.StatusOK)
	}))

	req := httptest.NewRequest(http.MethodPost, "http://example.com/cards/payments/checkout-session?source=test", nil)
	req.Header.Set("X-Forwarded-Proto", "https")
	rr := httptest.NewRecorder()
	handler.ServeHTTP(rr, req)

	logLine := buffer.String()
	if !strings.Contains(logLine, "scheme=https") {
		t.Fatalf("expected log line to include scheme=https, got %q", logLine)
	}
	if !strings.Contains(logLine, "method=POST") || !strings.Contains(logLine, "path=/cards/payments/checkout-session?source=test") {
		t.Fatalf("expected log line to include request details, got %q", logLine)
	}
}

func TestHTTPRequestLogger_DefaultsToHttpScheme(t *testing.T) {
	buffer, restore := captureLogOutput(t)
	defer restore()

	middleware := util.HTTPRequestLogger(false)
	handler := middleware(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		w.WriteHeader(http.StatusNoContent)
	}))

	req := httptest.NewRequest(http.MethodGet, "http://example.com/cards/status", nil)
	rr := httptest.NewRecorder()
	handler.ServeHTTP(rr, req)

	logLine := buffer.String()
	if !strings.Contains(logLine, "scheme=http") {
		t.Fatalf("expected log line to include scheme=http, got %q", logLine)
	}
}
