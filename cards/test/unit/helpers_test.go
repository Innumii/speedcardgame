package services_test

import (
	"bytes"
	"context"
	"encoding/json"
	"errors"
	"net/http"
	"net/http/httptest"
	"testing"

	"github.com/FYL-Studios/speedcardgame/cards/config"
	"github.com/FYL-Studios/speedcardgame/cards/models"
	"github.com/go-chi/chi"
	"gorm.io/driver/sqlite"
	"gorm.io/gorm"
	"gorm.io/gorm/logger"
)

func setupTestDB(t *testing.T) *gorm.DB {
	t.Helper()
	db, err := gorm.Open(sqlite.Open(":memory:"), &gorm.Config{Logger: logger.Default.LogMode(logger.Silent)})
	if err != nil {
		t.Fatalf("failed to open sqlite: %v", err)
	}
	if err := db.AutoMigrate(&models.Card{}, &models.Deck{}, &models.Inventory{}, &models.PaymentLedger{}); err != nil {
		t.Fatalf("failed to migrate: %v", err)
	}
	prev := config.DB
	config.DB = db
	t.Cleanup(func() { config.DB = prev })
	return db
}

func setupBrokenDB(t *testing.T) {
	t.Helper()
	db, _ := gorm.Open(sqlite.Open(":memory:"), &gorm.Config{Logger: logger.Default.LogMode(logger.Silent)})
	if err := db.AutoMigrate(&models.Card{}, &models.Deck{}, &models.Inventory{}, &models.PaymentLedger{}); err != nil {
		t.Fatalf("failed to migrate: %v", err)
	}
	sqlDB, _ := db.DB()
	sqlDB.Close()
	prev := config.DB
	config.DB = db
	t.Cleanup(func() { config.DB = prev })
}

// setupSeedThenBreak seeds data then closes connection to simulate mid-operation failures.
func setupSeedThenBreak(t *testing.T, inv models.Inventory) {
	t.Helper()
	db, _ := gorm.Open(sqlite.Open(":memory:"), &gorm.Config{Logger: logger.Default.LogMode(logger.Silent)})
	if err := db.AutoMigrate(&models.Card{}, &models.Deck{}, &models.Inventory{}, &models.PaymentLedger{}); err != nil {
		t.Fatalf("failed to migrate: %v", err)
	}
	db.Create(&inv)
	sqlDB, _ := db.DB()
	sqlDB.Close()
	prev := config.DB
	config.DB = db
	t.Cleanup(func() { config.DB = prev })
}

func seedCards(t *testing.T, db *gorm.DB, cards []models.Card) {
	t.Helper()
	if err := db.Create(&cards).Error; err != nil {
		t.Fatalf("failed to seed cards: %v", err)
	}
}

func seedInventory(t *testing.T, db *gorm.DB, inv models.Inventory) {
	t.Helper()
	if err := db.Create(&inv).Error; err != nil {
		t.Fatalf("failed to seed inventory: %v", err)
	}
}

func withChiParam(r *http.Request, key, value string) *http.Request {
	rctx := chi.NewRouteContext()
	rctx.URLParams.Add(key, value)
	return r.WithContext(context.WithValue(r.Context(), chi.RouteCtxKey, rctx))
}

func jsonRequest(t *testing.T, method, url string, body interface{}) *http.Request {
	t.Helper()
	b, err := json.Marshal(body)
	if err != nil {
		t.Fatalf("failed to marshal body: %v", err)
	}
	req := httptest.NewRequest(method, url, bytes.NewReader(b))
	req.Header.Set("Content-Type", "application/json")
	return req
}

type failingResponseWriter struct {
	header http.Header
	status int
}

func (w *failingResponseWriter) Header() http.Header {
	if w.header == nil {
		w.header = make(http.Header)
	}
	return w.header
}

func (w *failingResponseWriter) WriteHeader(statusCode int) {
	w.status = statusCode
}

func (w *failingResponseWriter) Write(_ []byte) (int, error) {
	if w.status == 0 {
		w.status = http.StatusOK
	}
	return 0, errors.New("forced write failure")
}
