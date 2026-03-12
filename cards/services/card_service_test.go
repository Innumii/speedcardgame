package services

import (
	"bytes"
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"testing"

	"github.com/Ryanljk/speedcardgame/cards/models"
)

// ── CreateCard ─────────────────────────────────────────────────────────────────

func TestCreateCard_Success(t *testing.T) {
	setupTestDB(t)

	body := map[string]interface{}{
		"cid": 1, "name": "Fire Bolt", "type": "Spell",
		"cost": 2, "value": 3, "power": 4, "toughness": 1, "effect": "Deal 3 damage",
	}
	req := jsonRequest(t, http.MethodPost, "/cards", body)
	rr := httptest.NewRecorder()

	CreateCard(rr, req)

	if rr.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d: %s", rr.Code, rr.Body.String())
	}

	var card models.Card
	if err := json.NewDecoder(rr.Body).Decode(&card); err != nil {
		t.Fatalf("failed to decode response: %v", err)
	}
	if card.Name != "Fire Bolt" {
		t.Errorf("expected card name 'Fire Bolt', got %q", card.Name)
	}
}

func TestCreateCard_InvalidBody(t *testing.T) {
	setupTestDB(t)

	req := httptest.NewRequest(http.MethodPost, "/cards", bytes.NewBufferString("not-json"))
	rr := httptest.NewRecorder()

	CreateCard(rr, req)

	if rr.Code != http.StatusBadRequest {
		t.Errorf("expected 400, got %d", rr.Code)
	}
}

// ── ListCards ──────────────────────────────────────────────────────────────────

func TestListCards_Empty(t *testing.T) {
	setupTestDB(t)

	req := httptest.NewRequest(http.MethodGet, "/cards", nil)
	rr := httptest.NewRecorder()

	ListCards(rr, req)

	if rr.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d", rr.Code)
	}

	var cards []models.Card
	json.NewDecoder(rr.Body).Decode(&cards)
	if len(cards) != 0 {
		t.Errorf("expected empty list, got %d cards", len(cards))
	}
}

func TestListCards_ReturnsSeedData(t *testing.T) {
	db := setupTestDB(t)
	seedCards(t, db, []models.Card{
		{Cid: 1, Name: "Card A"},
		{Cid: 2, Name: "Card B"},
	})

	req := httptest.NewRequest(http.MethodGet, "/cards", nil)
	rr := httptest.NewRecorder()

	ListCards(rr, req)

	if rr.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d", rr.Code)
	}

	var cards []models.Card
	json.NewDecoder(rr.Body).Decode(&cards)
	if len(cards) != 2 {
		t.Errorf("expected 2 cards, got %d", len(cards))
	}
}

func TestListCards_OrderedByCid(t *testing.T) {
	db := setupTestDB(t)
	seedCards(t, db, []models.Card{
		{Cid: 3, Name: "Card C"},
		{Cid: 1, Name: "Card A"},
		{Cid: 2, Name: "Card B"},
	})

	req := httptest.NewRequest(http.MethodGet, "/cards", nil)
	rr := httptest.NewRecorder()

	ListCards(rr, req)

	var cards []models.Card
	json.NewDecoder(rr.Body).Decode(&cards)

	for i := 1; i < len(cards); i++ {
		if cards[i].Cid < cards[i-1].Cid {
			t.Errorf("cards not ordered by cid: got %d before %d", cards[i-1].Cid, cards[i].Cid)
		}
	}
}

// ── UpdateCard ─────────────────────────────────────────────────────────────────

func TestUpdateCard_Success(t *testing.T) {
	db := setupTestDB(t)
	seedCards(t, db, []models.Card{{Cid: 1, Name: "Old Name"}})

	body := map[string]interface{}{"cid": 1, "name": "New Name"}
	req := jsonRequest(t, http.MethodPut, "/cards", body)
	rr := httptest.NewRecorder()

	UpdateCard(rr, req)

	if rr.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d: %s", rr.Code, rr.Body.String())
	}

	var card models.Card
	json.NewDecoder(rr.Body).Decode(&card)
	if card.Name != "New Name" {
		t.Errorf("expected 'New Name', got %q", card.Name)
	}
}

func TestUpdateCard_NotFound(t *testing.T) {
	setupTestDB(t)

	body := map[string]interface{}{"cid": 99, "name": "Ghost"}
	req := jsonRequest(t, http.MethodPut, "/cards", body)
	rr := httptest.NewRecorder()

	UpdateCard(rr, req)

	if rr.Code != http.StatusNotFound {
		t.Errorf("expected 404, got %d", rr.Code)
	}
}

func TestUpdateCard_InvalidBody(t *testing.T) {
	setupTestDB(t)

	req := httptest.NewRequest(http.MethodPut, "/cards", bytes.NewBufferString("bad"))
	rr := httptest.NewRecorder()

	UpdateCard(rr, req)

	if rr.Code != http.StatusBadRequest {
		t.Errorf("expected 400, got %d", rr.Code)
	}
}

// ── DeleteCard ─────────────────────────────────────────────────────────────────

func TestDeleteCard_Success(t *testing.T) {
	db := setupTestDB(t)
	seedCards(t, db, []models.Card{{Cid: 1, Name: "To Delete"}})

	body := map[string]interface{}{"cid": 1}
	req := jsonRequest(t, http.MethodDelete, "/cards", body)
	rr := httptest.NewRecorder()

	DeleteCard(rr, req)

	if rr.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d: %s", rr.Code, rr.Body.String())
	}

	// Confirm it's gone
	var count int64
	db.Model(&models.Card{}).Where("cid = ?", 1).Count(&count)
	if count != 0 {
		t.Error("expected card to be deleted from DB")
	}
}

func TestDeleteCard_NotFound(t *testing.T) {
	setupTestDB(t)

	body := map[string]interface{}{"cid": 99}
	req := jsonRequest(t, http.MethodDelete, "/cards", body)
	rr := httptest.NewRecorder()

	DeleteCard(rr, req)

	if rr.Code != http.StatusNotFound {
		t.Errorf("expected 404, got %d", rr.Code)
	}
}

func TestDeleteCard_InvalidBody(t *testing.T) {
	setupTestDB(t)

	req := httptest.NewRequest(http.MethodDelete, "/cards", bytes.NewBufferString("bad"))
	rr := httptest.NewRecorder()

	DeleteCard(rr, req)

	if rr.Code != http.StatusBadRequest {
		t.Errorf("expected 400, got %d", rr.Code)
	}
}

// ── helpers ────────────────────────────────────────────────────────────────────

func jsonRequest(t *testing.T, method, url string, body interface{}) *http.Request {
	t.Helper()
	b, err := json.Marshal(body)
	if err != nil {
		t.Fatalf("failed to marshal request body: %v", err)
	}
	req := httptest.NewRequest(method, url, bytes.NewReader(b))
	req.Header.Set("Content-Type", "application/json")
	return req
}
