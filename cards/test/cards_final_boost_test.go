package services_test

import (
	"net/http"
	"net/http/httptest"
	"testing"

	"github.com/Ryanljk/speedcardgame/cards/config"
	"github.com/Ryanljk/speedcardgame/cards/models"
	cardservices "github.com/Ryanljk/speedcardgame/cards/services"
	"gorm.io/driver/sqlite"
	"gorm.io/gorm"
	"gorm.io/gorm/logger"
)

// setupBrokenDB migrates a fresh DB then closes it so all queries fail.
func setupBrokenDB(t *testing.T) {
	t.Helper()
	db, err := gorm.Open(sqlite.Open(":memory:"), &gorm.Config{
		Logger: logger.Default.LogMode(logger.Silent),
	})
	if err != nil {
		t.Fatalf("failed to create db: %v", err)
	}
	if err := db.AutoMigrate(&models.Card{}, &models.Deck{}, &models.Inventory{}); err != nil {
		t.Fatalf("failed to migrate db: %v", err)
	}
	sqlDB, _ := db.DB()
	sqlDB.Close()

	prev := config.DB
	config.DB = db
	t.Cleanup(func() { config.DB = prev })
}

// ── DB error paths ─────────────────────────────────────────────────────────────

func TestCreateCard_DBError(t *testing.T) {
	setupBrokenDB(t)
	body := map[string]interface{}{"cid": 1, "name": "Card", "type": "Spell", "cost": 1}
	rr := httptest.NewRecorder()
	cardservices.CreateCard(rr, jsonRequest(t, http.MethodPost, "/cards", body))
	if rr.Code != http.StatusInternalServerError {
		t.Errorf("expected 500, got %d", rr.Code)
	}
}

func TestListCards_DBError(t *testing.T) {
	setupBrokenDB(t)
	rr := httptest.NewRecorder()
	cardservices.ListCards(rr, httptest.NewRequest(http.MethodGet, "/cards", nil))
	if rr.Code != http.StatusInternalServerError {
		t.Errorf("expected 500, got %d", rr.Code)
	}
}

func TestUpdateCard_DBError(t *testing.T) {
	setupBrokenDB(t)
	body := map[string]interface{}{"cid": 1, "name": "New"}
	rr := httptest.NewRecorder()
	cardservices.UpdateCard(rr, jsonRequest(t, http.MethodPut, "/cards", body))
	if rr.Code != http.StatusNotFound {
		t.Errorf("expected 404, got %d", rr.Code)
	}
}

func TestDeleteCard_DBError(t *testing.T) {
	setupBrokenDB(t)
	body := map[string]interface{}{"cid": 1}
	rr := httptest.NewRecorder()
	cardservices.DeleteCard(rr, jsonRequest(t, http.MethodDelete, "/cards", body))
	if rr.Code != http.StatusNotFound {
		t.Errorf("expected 404, got %d", rr.Code)
	}
}

func TestListDecks_DBError(t *testing.T) {
	setupBrokenDB(t)
	rr := httptest.NewRecorder()
	cardservices.ListDecks(rr, httptest.NewRequest(http.MethodGet, "/decks", nil))
	if rr.Code != http.StatusInternalServerError {
		t.Errorf("expected 500, got %d", rr.Code)
	}
}

func TestDeleteDeck_DBError(t *testing.T) {
	setupBrokenDB(t)
	body := map[string]interface{}{"uid": 1}
	rr := httptest.NewRecorder()
	cardservices.DeleteDeck(rr, jsonRequest(t, http.MethodDelete, "/decks", body))
	if rr.Code != http.StatusInternalServerError {
		t.Errorf("expected 500, got %d", rr.Code)
	}
}

func TestListInventories_DBError(t *testing.T) {
	setupBrokenDB(t)
	rr := httptest.NewRecorder()
	cardservices.ListInventories(rr, httptest.NewRequest(http.MethodGet, "/inventories", nil))
	if rr.Code != http.StatusInternalServerError {
		t.Errorf("expected 500, got %d", rr.Code)
	}
}

func TestGetInventoryByUserID_DBError(t *testing.T) {
	setupBrokenDB(t)
	req := withChiParam(httptest.NewRequest(http.MethodGet, "/inventories/1", nil), "uid", "1")
	rr := httptest.NewRecorder()
	cardservices.GetInventoryByUserID(rr, req)
	if rr.Code != http.StatusInternalServerError {
		t.Errorf("expected 500, got %d", rr.Code)
	}
}

func TestGetDeckByUserID_DBError(t *testing.T) {
	setupBrokenDB(t)
	req := withChiParam(httptest.NewRequest(http.MethodGet, "/decks/1", nil), "uid", "1")
	rr := httptest.NewRecorder()
	cardservices.GetDeckByUserID(rr, req)
	if rr.Code != http.StatusNotFound {
		t.Errorf("expected 404, got %d", rr.Code)
	}
}

func TestFillDeckForUser_DBError(t *testing.T) {
	setupBrokenDB(t)
	body := map[string]interface{}{"uid": 1}
	rr := httptest.NewRecorder()
	cardservices.FillDeckForUser(rr, jsonRequest(t, http.MethodPost, "/decks/fill", body))
	if rr.Code != http.StatusNotFound {
		t.Errorf("expected 404, got %d", rr.Code)
	}
}

func TestUpdateInventory_DBError(t *testing.T) {
	setupBrokenDB(t)
	body := map[string]interface{}{"uid": 1, "cards": map[string]int{"1": 1}}
	rr := httptest.NewRecorder()
	cardservices.UpdateInventory(rr, jsonRequest(t, http.MethodPut, "/inventories", body))
	if rr.Code != http.StatusInternalServerError {
		t.Errorf("expected 500, got %d", rr.Code)
	}
}

func TestCreateInventory_DBError(t *testing.T) {
	setupBrokenDB(t)
	body := map[string]interface{}{"uid": 1}
	rr := httptest.NewRecorder()
	cardservices.CreateInventory(rr, jsonRequest(t, http.MethodPost, "/inventories", body))
	if rr.Code != http.StatusInternalServerError {
		t.Errorf("expected 500, got %d", rr.Code)
	}
}

func TestFillDecksFromInventories_DBError(t *testing.T) {
	db, _ := gorm.Open(sqlite.Open(":memory:"), &gorm.Config{
		Logger: logger.Default.LogMode(logger.Silent),
	})
	sqlDB, _ := db.DB()
	sqlDB.Close()
	if err := cardservices.FillDecksFromInventories(db); err == nil {
		t.Error("expected error from broken DB, got nil")
	}
}

func TestCreateDeck_DBError(t *testing.T) {
	t.Setenv("DECK_SIZE", "3")
	setupBrokenDB(t)
	body := map[string]interface{}{
		"uid":   1,
		"cards": map[string]int{"1": 1, "2": 1, "3": 1},
	}
	rr := httptest.NewRecorder()
	cardservices.CreateDeck(rr, jsonRequest(t, http.MethodPost, "/decks", body))
	if rr.Code != http.StatusInternalServerError {
		t.Errorf("expected 500, got %d", rr.Code)
	}
}

// ── UpdateInventory nil Cards branch ──────────────────────────────────────────

func TestUpdateInventory_NilCardsInitialized(t *testing.T) {
	db := setupTestDB(t)
	db.Exec("INSERT INTO inventories (uid, cards) VALUES (?, NULL)", 1)
	body := map[string]interface{}{"uid": 1, "cards": map[string]int{"1": 2}}
	rr := httptest.NewRecorder()
	cardservices.UpdateInventory(rr, jsonRequest(t, http.MethodPut, "/inventories", body))
	if rr.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d: %s", rr.Code, rr.Body.String())
	}
}

// ── FillDeckForUser - no cards in DB ──────────────────────────────────────────

func TestFillDeckForUser_NoCardsInDB(t *testing.T) {
	t.Setenv("DECK_SIZE", "3")
	db := setupTestDB(t)
	seedInventory(t, db, models.Inventory{Uid: 1, Cards: models.CardCounts{1: 2}})
	body := map[string]interface{}{"uid": 1}
	rr := httptest.NewRecorder()
	cardservices.FillDeckForUser(rr, jsonRequest(t, http.MethodPost, "/decks/fill", body))
	if rr.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d: %s", rr.Code, rr.Body.String())
	}
}

// ── getCSVField bounds ─────────────────────────────────────────────────────────

func TestGetCSVField_OutOfBounds(t *testing.T) {
	if got := cardservices.GetCSVField([]string{"a", "b"}, 5); got != "" {
		t.Errorf("expected empty string, got %q", got)
	}
}

func TestGetCSVField_NegativeIndex(t *testing.T) {
	if got := cardservices.GetCSVField([]string{"a", "b"}, -1); got != "" {
		t.Errorf("expected empty string, got %q", got)
	}
}
