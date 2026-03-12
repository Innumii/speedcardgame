package services

import (
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"testing"

	"github.com/Ryanljk/speedcardgame/cards/models"
)

// ── getDeckSizeLimit ───────────────────────────────────────────────────────────

func TestGetDeckSizeLimit_Default(t *testing.T) {
	t.Setenv("DECK_SIZE", "")
	if got := getDeckSizeLimit(); got != defaultDeckSizeLimit {
		t.Errorf("expected %d, got %d", defaultDeckSizeLimit, got)
	}
}

func TestGetDeckSizeLimit_CustomValue(t *testing.T) {
	t.Setenv("DECK_SIZE", "20")
	if got := getDeckSizeLimit(); got != 20 {
		t.Errorf("expected 20, got %d", got)
	}
}

func TestGetDeckSizeLimit_InvalidFallsBack(t *testing.T) {
	t.Setenv("DECK_SIZE", "notanumber")
	if got := getDeckSizeLimit(); got != defaultDeckSizeLimit {
		t.Errorf("expected default %d, got %d", defaultDeckSizeLimit, got)
	}
}

func TestGetDeckSizeLimit_ZeroFallsBack(t *testing.T) {
	t.Setenv("DECK_SIZE", "0")
	if got := getDeckSizeLimit(); got != defaultDeckSizeLimit {
		t.Errorf("expected default %d for zero, got %d", defaultDeckSizeLimit, got)
	}
}

// ── countDeckCards ─────────────────────────────────────────────────────────────

func TestCountDeckCards_Empty(t *testing.T) {
	if got := countDeckCards(models.CardCounts{}); got != 0 {
		t.Errorf("expected 0, got %d", got)
	}
}

func TestCountDeckCards_SumsCorrectly(t *testing.T) {
	counts := models.CardCounts{1: 5, 2: 10, 3: 15}
	if got := countDeckCards(counts); got != 30 {
		t.Errorf("expected 30, got %d", got)
	}
}

// ── buildDeckCounts ────────────────────────────────────────────────────────────

func TestBuildDeckCounts_RespectsLimit(t *testing.T) {
	inventory := models.CardCounts{1: 10, 2: 10, 3: 10}
	allCards := []models.Card{{Cid: 1}, {Cid: 2}, {Cid: 3}}

	deck := buildDeckCounts(inventory, allCards, 5)
	total := countDeckCards(deck)
	if total != 5 {
		t.Errorf("expected deck total 5, got %d", total)
	}
}

func TestBuildDeckCounts_NilInventoryFallsBackToAllCards(t *testing.T) {
	allCards := []models.Card{{Cid: 1}, {Cid: 2}, {Cid: 3}}

	deck := buildDeckCounts(nil, allCards, 3)
	total := countDeckCards(deck)
	if total != 3 {
		t.Errorf("expected deck total 3, got %d", total)
	}
}

func TestBuildDeckCounts_InventoryTakesPriorityOverAllCards(t *testing.T) {
	inventory := models.CardCounts{5: 3}
	allCards := []models.Card{{Cid: 1}, {Cid: 2}, {Cid: 3}}

	deck := buildDeckCounts(inventory, allCards, 3)
	if deck[5] != 3 {
		t.Errorf("expected 3 of card 5 from inventory, got %d", deck[5])
	}
}

// ── CreateDeck ─────────────────────────────────────────────────────────────────

func TestCreateDeck_Success(t *testing.T) {
	t.Setenv("DECK_SIZE", "3")
	setupTestDB(t)

	body := map[string]interface{}{
		"uid":   1,
		"cards": map[string]int{"1": 1, "2": 1, "3": 1},
	}
	req := jsonRequest(t, http.MethodPost, "/decks", body)
	rr := httptest.NewRecorder()

	CreateDeck(rr, req)

	if rr.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d: %s", rr.Code, rr.Body.String())
	}

	var deck models.Deck
	json.NewDecoder(rr.Body).Decode(&deck)
	if deck.Uid != 1 {
		t.Errorf("expected uid 1, got %d", deck.Uid)
	}
}

func TestCreateDeck_WrongSize(t *testing.T) {
	t.Setenv("DECK_SIZE", "3")
	setupTestDB(t)

	body := map[string]interface{}{
		"uid":   1,
		"cards": map[string]int{"1": 1}, // only 1 card, need 3
	}
	req := jsonRequest(t, http.MethodPost, "/decks", body)
	rr := httptest.NewRecorder()

	CreateDeck(rr, req)

	if rr.Code != http.StatusBadRequest {
		t.Errorf("expected 400, got %d", rr.Code)
	}
}

func TestCreateDeck_InvalidUid(t *testing.T) {
	setupTestDB(t)

	body := map[string]interface{}{"uid": 0, "cards": map[string]int{}}
	req := jsonRequest(t, http.MethodPost, "/decks", body)
	rr := httptest.NewRecorder()

	CreateDeck(rr, req)

	if rr.Code != http.StatusBadRequest {
		t.Errorf("expected 400 for uid=0, got %d", rr.Code)
	}
}

func TestCreateDeck_ReplacesExistingDeck(t *testing.T) {
	t.Setenv("DECK_SIZE", "3")
	db := setupTestDB(t)

	// Pre-existing deck
	db.Create(&models.Deck{Uid: 1, Cards: models.CardCounts{1: 1, 2: 1, 3: 1}})

	body := map[string]interface{}{
		"uid":   1,
		"cards": map[string]int{"4": 1, "5": 1, "6": 1},
	}
	req := jsonRequest(t, http.MethodPost, "/decks", body)
	rr := httptest.NewRecorder()

	CreateDeck(rr, req)

	if rr.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d: %s", rr.Code, rr.Body.String())
	}

	var deck models.Deck
	json.NewDecoder(rr.Body).Decode(&deck)
	if _, ok := deck.Cards[1]; ok {
		t.Error("expected old deck cards to be replaced")
	}
}

func TestCreateDeck_InvalidBody(t *testing.T) {
	setupTestDB(t)

	req := jsonRequest(t, http.MethodPost, "/decks", "bad")
	rr := httptest.NewRecorder()

	CreateDeck(rr, req)

	if rr.Code != http.StatusBadRequest {
		t.Errorf("expected 400, got %d", rr.Code)
	}
}

// ── ListDecks ──────────────────────────────────────────────────────────────────

func TestListDecks_Empty(t *testing.T) {
	setupTestDB(t)

	req := httptest.NewRequest(http.MethodGet, "/decks", nil)
	rr := httptest.NewRecorder()

	ListDecks(rr, req)

	if rr.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d", rr.Code)
	}

	var decks []models.Deck
	json.NewDecoder(rr.Body).Decode(&decks)
	if len(decks) != 0 {
		t.Errorf("expected empty list, got %d", len(decks))
	}
}

func TestListDecks_ReturnsSeedData(t *testing.T) {
	db := setupTestDB(t)
	db.Create(&models.Deck{Uid: 1, Cards: models.CardCounts{1: 2}})
	db.Create(&models.Deck{Uid: 2, Cards: models.CardCounts{2: 3}})

	req := httptest.NewRequest(http.MethodGet, "/decks", nil)
	rr := httptest.NewRecorder()

	ListDecks(rr, req)

	var decks []models.Deck
	json.NewDecoder(rr.Body).Decode(&decks)
	if len(decks) != 2 {
		t.Errorf("expected 2 decks, got %d", len(decks))
	}
}

// ── DeleteDeck ─────────────────────────────────────────────────────────────────

func TestDeleteDeck_Success(t *testing.T) {
	db := setupTestDB(t)
	db.Create(&models.Deck{Uid: 1, Cards: models.CardCounts{1: 1}})

	body := map[string]interface{}{"uid": 1}
	req := jsonRequest(t, http.MethodDelete, "/decks", body)
	rr := httptest.NewRecorder()

	DeleteDeck(rr, req)

	if rr.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d", rr.Code)
	}

	var count int64
	db.Model(&models.Deck{}).Where("uid = ?", 1).Count(&count)
	if count != 0 {
		t.Error("expected deck to be deleted")
	}
}

func TestDeleteDeck_InvalidBody(t *testing.T) {
	setupTestDB(t)

	req := jsonRequest(t, http.MethodDelete, "/decks", "bad")
	rr := httptest.NewRecorder()

	DeleteDeck(rr, req)

	if rr.Code != http.StatusBadRequest {
		t.Errorf("expected 400, got %d", rr.Code)
	}
}

// ── FillDecksFromInventories ───────────────────────────────────────────────────

func TestFillDecksFromInventories_NoInventories(t *testing.T) {
	db := setupTestDB(t)

	if err := FillDecksFromInventories(db); err != nil {
		t.Errorf("expected no error with empty inventories, got: %v", err)
	}
}

func TestFillDecksFromInventories_CreatesDeckPerInventory(t *testing.T) {
	t.Setenv("DECK_SIZE", "3")
	db := setupTestDB(t)

	seedCards(t, db, []models.Card{{Cid: 1}, {Cid: 2}, {Cid: 3}})
	seedInventory(t, db, models.Inventory{Uid: 1, Cards: models.CardCounts{1: 2, 2: 1}})
	seedInventory(t, db, models.Inventory{Uid: 2, Cards: models.CardCounts{3: 3}})

	if err := FillDecksFromInventories(db); err != nil {
		t.Fatalf("expected no error, got: %v", err)
	}

	var count int64
	db.Model(&models.Deck{}).Count(&count)
	if count != 2 {
		t.Errorf("expected 2 decks created, got %d", count)
	}
}
