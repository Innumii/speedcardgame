package services

import (
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"testing"

	"github.com/Ryanljk/speedcardgame/cards/models"
)

// ── clampCardCount ─────────────────────────────────────────────────────────────

func TestClampCardCount_BelowZero(t *testing.T) {
	if got := clampCardCount(-1); got != 0 {
		t.Errorf("expected 0, got %d", got)
	}
}

func TestClampCardCount_Zero(t *testing.T) {
	if got := clampCardCount(0); got != 0 {
		t.Errorf("expected 0, got %d", got)
	}
}

func TestClampCardCount_WithinRange(t *testing.T) {
	if got := clampCardCount(2); got != 2 {
		t.Errorf("expected 2, got %d", got)
	}
}

func TestClampCardCount_AtMax(t *testing.T) {
	if got := clampCardCount(maxCardCopies); got != maxCardCopies {
		t.Errorf("expected %d, got %d", maxCardCopies, got)
	}
}

func TestClampCardCount_AboveMax(t *testing.T) {
	if got := clampCardCount(maxCardCopies + 1); got != maxCardCopies {
		t.Errorf("expected %d, got %d", maxCardCopies, got)
	}
}

// ── normalizeInventoryCards ────────────────────────────────────────────────────

func TestNormalizeInventoryCards_NilReturnsFalse(t *testing.T) {
	if normalizeInventoryCards(nil) {
		t.Error("expected false for nil input")
	}
}

func TestNormalizeInventoryCards_RemovesNegativeCid(t *testing.T) {
	cards := models.CardCounts{-1: 2, 1: 2}
	normalizeInventoryCards(cards)
	if _, ok := cards[-1]; ok {
		t.Error("expected negative cid to be removed")
	}
}

func TestNormalizeInventoryCards_ClampsOverMax(t *testing.T) {
	cards := models.CardCounts{1: 10}
	normalizeInventoryCards(cards)
	if cards[1] != maxCardCopies {
		t.Errorf("expected %d, got %d", maxCardCopies, cards[1])
	}
}

func TestNormalizeInventoryCards_RemovesZeroQuantity(t *testing.T) {
	cards := models.CardCounts{1: 0}
	normalizeInventoryCards(cards)
	if _, ok := cards[1]; ok {
		t.Error("expected zero-quantity card to be removed")
	}
}

func TestNormalizeInventoryCards_ValidCardsUnchanged(t *testing.T) {
	cards := models.CardCounts{1: 2, 2: 3}
	changed := normalizeInventoryCards(cards)
	if changed {
		t.Error("expected no changes for valid cards")
	}
	if cards[1] != 2 || cards[2] != 3 {
		t.Error("valid cards should remain unchanged")
	}
}

// ── CreateInventory ────────────────────────────────────────────────────────────

func TestCreateInventory_DefaultCards(t *testing.T) {
	setupTestDB(t)

	body := map[string]interface{}{"uid": 1}
	req := jsonRequest(t, http.MethodPost, "/inventories", body)
	rr := httptest.NewRecorder()

	CreateInventory(rr, req)

	if rr.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d: %s", rr.Code, rr.Body.String())
	}

	var inv models.Inventory
	json.NewDecoder(rr.Body).Decode(&inv)
	if len(inv.Cards) == 0 {
		t.Error("expected default starter cards to be assigned")
	}
}

func TestCreateInventory_WithCards(t *testing.T) {
	setupTestDB(t)

	body := map[string]interface{}{
		"uid":   2,
		"cards": map[string]int{"1": 2, "2": 3},
	}
	req := jsonRequest(t, http.MethodPost, "/inventories", body)
	rr := httptest.NewRecorder()

	CreateInventory(rr, req)

	if rr.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d: %s", rr.Code, rr.Body.String())
	}
}

func TestCreateInventory_InvalidBody(t *testing.T) {
	setupTestDB(t)

	req := jsonRequest(t, http.MethodPost, "/inventories", "bad-json")
	rr := httptest.NewRecorder()

	CreateInventory(rr, req)

	if rr.Code != http.StatusBadRequest {
		t.Errorf("expected 400, got %d", rr.Code)
	}
}

// ── ListInventories ────────────────────────────────────────────────────────────

func TestListInventories_Empty(t *testing.T) {
	setupTestDB(t)

	req := httptest.NewRequest(http.MethodGet, "/inventories", nil)
	rr := httptest.NewRecorder()

	ListInventories(rr, req)

	if rr.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d", rr.Code)
	}

	var invs []models.Inventory
	json.NewDecoder(rr.Body).Decode(&invs)
	if len(invs) != 0 {
		t.Errorf("expected empty list, got %d", len(invs))
	}
}

func TestListInventories_ReturnsSeedData(t *testing.T) {
	db := setupTestDB(t)
	seedInventory(t, db, models.Inventory{Uid: 1, Cards: models.CardCounts{1: 2}})
	seedInventory(t, db, models.Inventory{Uid: 2, Cards: models.CardCounts{2: 3}})

	req := httptest.NewRequest(http.MethodGet, "/inventories", nil)
	rr := httptest.NewRecorder()

	ListInventories(rr, req)

	if rr.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d", rr.Code)
	}

	var invs []models.Inventory
	json.NewDecoder(rr.Body).Decode(&invs)
	if len(invs) != 2 {
		t.Errorf("expected 2 inventories, got %d", len(invs))
	}
}

// ── UpdateInventory ────────────────────────────────────────────────────────────

func TestUpdateInventory_AddsCards(t *testing.T) {
	db := setupTestDB(t)
	seedInventory(t, db, models.Inventory{Uid: 1, Cards: models.CardCounts{1: 1}})

	body := map[string]interface{}{
		"uid":   1,
		"cards": map[string]int{"1": 2},
	}
	req := jsonRequest(t, http.MethodPut, "/inventories", body)
	rr := httptest.NewRecorder()

	UpdateInventory(rr, req)

	if rr.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d: %s", rr.Code, rr.Body.String())
	}

	var inv models.Inventory
	json.NewDecoder(rr.Body).Decode(&inv)
	if inv.Cards[1] != 3 {
		t.Errorf("expected card 1 count to be 3, got %d", inv.Cards[1])
	}
}

func TestUpdateInventory_ClampsAtMax(t *testing.T) {
	db := setupTestDB(t)
	seedInventory(t, db, models.Inventory{Uid: 1, Cards: models.CardCounts{1: 3}})

	body := map[string]interface{}{
		"uid":   1,
		"cards": map[string]int{"1": 4}, // would push to 7, should clamp to 4
	}
	req := jsonRequest(t, http.MethodPut, "/inventories", body)
	rr := httptest.NewRecorder()

	UpdateInventory(rr, req)

	var inv models.Inventory
	json.NewDecoder(rr.Body).Decode(&inv)
	if inv.Cards[1] != maxCardCopies {
		t.Errorf("expected card 1 clamped to %d, got %d", maxCardCopies, inv.Cards[1])
	}
}

func TestUpdateInventory_NotFound(t *testing.T) {
	setupTestDB(t)

	body := map[string]interface{}{"uid": 99, "cards": map[string]int{"1": 1}}
	req := jsonRequest(t, http.MethodPut, "/inventories", body)
	rr := httptest.NewRecorder()

	UpdateInventory(rr, req)

	if rr.Code != http.StatusInternalServerError {
		t.Errorf("expected 500 for missing inventory, got %d", rr.Code)
	}
}

func TestUpdateInventory_InvalidBody(t *testing.T) {
	setupTestDB(t)

	req := jsonRequest(t, http.MethodPut, "/inventories", "bad")
	rr := httptest.NewRecorder()

	UpdateInventory(rr, req)

	if rr.Code != http.StatusBadRequest {
		t.Errorf("expected 400, got %d", rr.Code)
	}
}
