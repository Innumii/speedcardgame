//go:build integration

package integration_test

import (
	"bytes"
	"context"
	"encoding/json"
	"fmt"
	"net/http"
	"net/http/httptest"
	"os"
	"testing"

	"github.com/joho/godotenv"
	"github.com/Ryanljk/speedcardgame/cards/config"
	"github.com/Ryanljk/speedcardgame/cards/models"
	"github.com/go-chi/chi"
	"gorm.io/driver/postgres"
	"gorm.io/gorm"
	"gorm.io/gorm/logger"
)

func init() {
	_ = godotenv.Load("../.env")
}

func connectTestDB(t *testing.T) *gorm.DB {
	t.Helper()

	// Always use localhost — DATABASE_URL contains docker-internal hostname
	user := getEnvOrDefault("POSTGRES_USER", "postgres")
	pass := getEnvOrDefault("POSTGRES_PASSWORD", "postgres")
	dbname := getEnvOrDefault("POSTGRES_DB", "cards")
	port := getEnvOrDefault("DB_PORT", "5433")
	dsn := fmt.Sprintf("host=localhost user=%s password=%s dbname=%s port=%s sslmode=disable TimeZone=UTC",
		user, pass, dbname, port)

	db, err := gorm.Open(postgres.Open(dsn), &gorm.Config{
		Logger: logger.Default.LogMode(logger.Silent),
	})
	if err != nil {
		t.Fatalf("failed to connect to cards DB: %v\nMake sure docker-compose is running", err)
	}

	if err := db.AutoMigrate(&models.Card{}, &models.Deck{}, &models.Inventory{}); err != nil {
		t.Fatalf("failed to migrate: %v", err)
	}

	prev := config.DB
	config.DB = db
	t.Cleanup(func() { config.DB = prev })

	return db
}

func cleanDB(t *testing.T, db *gorm.DB) {
	t.Helper()
	db.Exec("DELETE FROM inventories")
	db.Exec("DELETE FROM decks")
	db.Exec("DELETE FROM cards")
}

func getEnvOrDefault(key, fallback string) string {
	if v := os.Getenv(key); v != "" {
		return v
	}
	return fallback
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
