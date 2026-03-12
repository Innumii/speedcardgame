package services

import (
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"os"
	"path/filepath"
	"testing"

	"github.com/Ryanljk/speedcardgame/cards/models"
)

// ── GetDeckByUserID ────────────────────────────────────────────────────────────

func TestGetDeckByUserID_Success(t *testing.T) {
	db := setupTestDB(t)
	db.Create(&models.Deck{Uid: 1, Cards: models.CardCounts{1: 2}})

	req := withChiParam(httptest.NewRequest(http.MethodGet, "/decks/1", nil), "uid", "1")
	rr := httptest.NewRecorder()

	GetDeckByUserID(rr, req)

	if rr.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d: %s", rr.Code, rr.Body.String())
	}

	var deck models.Deck
	json.NewDecoder(rr.Body).Decode(&deck)
	if deck.Uid != 1 {
		t.Errorf("expected uid 1, got %d", deck.Uid)
	}
}

func TestGetDeckByUserID_NotFound(t *testing.T) {
	setupTestDB(t)

	req := withChiParam(httptest.NewRequest(http.MethodGet, "/decks/99", nil), "uid", "99")
	rr := httptest.NewRecorder()

	GetDeckByUserID(rr, req)

	if rr.Code != http.StatusNotFound {
		t.Errorf("expected 404, got %d", rr.Code)
	}
}

func TestGetDeckByUserID_InvalidUID(t *testing.T) {
	setupTestDB(t)

	req := withChiParam(httptest.NewRequest(http.MethodGet, "/decks/abc", nil), "uid", "abc")
	rr := httptest.NewRecorder()

	GetDeckByUserID(rr, req)

	if rr.Code != http.StatusBadRequest {
		t.Errorf("expected 400, got %d", rr.Code)
	}
}

// ── GetInventoryByUserID ───────────────────────────────────────────────────────

func TestGetInventoryByUserID_Success(t *testing.T) {
	db := setupTestDB(t)
	seedInventory(t, db, models.Inventory{Uid: 1, Cards: models.CardCounts{1: 2}})

	req := withChiParam(httptest.NewRequest(http.MethodGet, "/inventories/1", nil), "uid", "1")
	rr := httptest.NewRecorder()

	GetInventoryByUserID(rr, req)

	if rr.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d: %s", rr.Code, rr.Body.String())
	}

	var inv models.Inventory
	json.NewDecoder(rr.Body).Decode(&inv)
	if inv.Uid != 1 {
		t.Errorf("expected uid 1, got %d", inv.Uid)
	}
}

func TestGetInventoryByUserID_NotFound(t *testing.T) {
	setupTestDB(t)

	req := withChiParam(httptest.NewRequest(http.MethodGet, "/inventories/99", nil), "uid", "99")
	rr := httptest.NewRecorder()

	GetInventoryByUserID(rr, req)

	if rr.Code != http.StatusInternalServerError {
		t.Errorf("expected 500, got %d", rr.Code)
	}
}

func TestGetInventoryByUserID_MissingUID(t *testing.T) {
	setupTestDB(t)

	req := withChiParam(httptest.NewRequest(http.MethodGet, "/inventories/", nil), "uid", "")
	rr := httptest.NewRecorder()

	GetInventoryByUserID(rr, req)

	if rr.Code != http.StatusBadRequest {
		t.Errorf("expected 400, got %d", rr.Code)
	}
}

// ── FillDeckForUser ────────────────────────────────────────────────────────────

func TestFillDeckForUser_Success(t *testing.T) {
	t.Setenv("DECK_SIZE", "3")
	db := setupTestDB(t)
	seedCards(t, db, []models.Card{{Cid: 1}, {Cid: 2}, {Cid: 3}})
	seedInventory(t, db, models.Inventory{Uid: 1, Cards: models.CardCounts{1: 2, 2: 1}})

	body := map[string]interface{}{"uid": 1}
	req := jsonRequest(t, http.MethodPost, "/decks/fill", body)
	rr := httptest.NewRecorder()

	FillDeckForUser(rr, req)

	if rr.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d: %s", rr.Code, rr.Body.String())
	}

	var deck models.Deck
	json.NewDecoder(rr.Body).Decode(&deck)
	if deck.Uid != 1 {
		t.Errorf("expected uid 1, got %d", deck.Uid)
	}
}

func TestFillDeckForUser_InvalidUID(t *testing.T) {
	setupTestDB(t)

	body := map[string]interface{}{"uid": 0}
	req := jsonRequest(t, http.MethodPost, "/decks/fill", body)
	rr := httptest.NewRecorder()

	FillDeckForUser(rr, req)

	if rr.Code != http.StatusBadRequest {
		t.Errorf("expected 400, got %d", rr.Code)
	}
}

func TestFillDeckForUser_InventoryNotFound(t *testing.T) {
	setupTestDB(t)

	body := map[string]interface{}{"uid": 99}
	req := jsonRequest(t, http.MethodPost, "/decks/fill", body)
	rr := httptest.NewRecorder()

	FillDeckForUser(rr, req)

	if rr.Code != http.StatusNotFound {
		t.Errorf("expected 404, got %d", rr.Code)
	}
}

func TestFillDeckForUser_InvalidBody(t *testing.T) {
	setupTestDB(t)

	req := jsonRequest(t, http.MethodPost, "/decks/fill", "bad")
	rr := httptest.NewRecorder()

	FillDeckForUser(rr, req)

	if rr.Code != http.StatusBadRequest {
		t.Errorf("expected 400, got %d", rr.Code)
	}
}

// ── SeedCardsFromCSV ───────────────────────────────────────────────────────────

func TestSeedCardsFromCSV_Success(t *testing.T) {
	db := setupTestDB(t)

	// Write a minimal CSV file
	csvContent := "cid,name,type,cost,value,power,toughness,effect\n1,Fire Bolt,Spell,2,3,4,1,Deal 3 damage\n2,Shield,Defense,1,2,0,5,Block 2\n"
	tmpFile := filepath.Join(t.TempDir(), "cards.csv")
	if err := os.WriteFile(tmpFile, []byte(csvContent), 0644); err != nil {
		t.Fatalf("failed to write temp csv: %v", err)
	}

	if err := SeedCardsFromCSV(db, tmpFile); err != nil {
		t.Fatalf("expected no error, got: %v", err)
	}

	var count int64
	db.Model(&models.Card{}).Count(&count)
	if count != 2 {
		t.Errorf("expected 2 cards seeded, got %d", count)
	}
}

func TestSeedCardsFromCSV_FileNotFound(t *testing.T) {
	db := setupTestDB(t)

	err := SeedCardsFromCSV(db, "/nonexistent/path/cards.csv")
	if err == nil {
		t.Error("expected error for missing file, got nil")
	}
}

func TestSeedCardsFromCSV_EmptyFile(t *testing.T) {
	db := setupTestDB(t)

	// Only headers, no data rows
	csvContent := "cid,name,type,cost,value,power,toughness,effect\n"
	tmpFile := filepath.Join(t.TempDir(), "empty.csv")
	os.WriteFile(tmpFile, []byte(csvContent), 0644)

	if err := SeedCardsFromCSV(db, tmpFile); err != nil {
		t.Fatalf("expected no error for empty CSV, got: %v", err)
	}

	var count int64
	db.Model(&models.Card{}).Count(&count)
	if count != 0 {
		t.Errorf("expected 0 cards, got %d", count)
	}
}
