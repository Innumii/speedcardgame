package services_test

import (
	"context"
	"net/http"
	"testing"

	"github.com/Ryanljk/speedcardgame/cards/config"
	"github.com/Ryanljk/speedcardgame/cards/models"
	"github.com/go-chi/chi"
	"gorm.io/driver/sqlite"
	"gorm.io/gorm"
	"gorm.io/gorm/logger"
)

// setupTestDB wires up an in-memory SQLite DB and points config.DB at it.
func setupTestDB(t *testing.T) *gorm.DB {
	t.Helper()

	db, err := gorm.Open(sqlite.Open(":memory:"), &gorm.Config{
		Logger: logger.Default.LogMode(logger.Silent),
	})
	if err != nil {
		t.Fatalf("failed to open in-memory sqlite: %v", err)
	}

	if err := db.AutoMigrate(&models.Card{}, &models.Deck{}, &models.Inventory{}); err != nil {
		t.Fatalf("failed to migrate: %v", err)
	}

	prev := config.DB
	config.DB = db
	t.Cleanup(func() { config.DB = prev })

	return db
}

// seedCards inserts a slice of cards into the test DB.
func seedCards(t *testing.T, db *gorm.DB, cards []models.Card) {
	t.Helper()
	if err := db.Create(&cards).Error; err != nil {
		t.Fatalf("failed to seed cards: %v", err)
	}
}

// seedInventory inserts an inventory record into the test DB.
func seedInventory(t *testing.T, db *gorm.DB, inv models.Inventory) {
	t.Helper()
	if err := db.Create(&inv).Error; err != nil {
		t.Fatalf("failed to seed inventory: %v", err)
	}
}

// withChiParam injects a chi URL parameter into the request context.
// Use this for handlers that call chi.URLParam(r, "uid").
func withChiParam(r *http.Request, key, value string) *http.Request {
	rctx := chi.NewRouteContext()
	rctx.URLParams.Add(key, value)
	return r.WithContext(context.WithValue(r.Context(), chi.RouteCtxKey, rctx))
}
