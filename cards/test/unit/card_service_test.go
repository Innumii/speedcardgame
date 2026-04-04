package services_test

import (
	"bytes"
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"testing"

	"github.com/FYL-Studios/speedcardgame/cards/models"
	cardservices "github.com/FYL-Studios/speedcardgame/cards/services"
)

func TestCreateCard_Success(t *testing.T) {
	setupTestDB(t)
	cost, value, power, toughness := 2, 3, 4, 1
	body := map[string]interface{}{
		"cid": 1, "name": "Fire Bolt", "type": "Spell",
		"cost": cost, "value": value, "power": power, "toughness": toughness,
		"effect": "Deal 3 damage",
	}
	rr := httptest.NewRecorder()
	cardservices.CreateCard(rr, jsonRequest(t, http.MethodPost, "/cards", body))
	if rr.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d: %s", rr.Code, rr.Body.String())
	}
	var card models.Card
	if err := json.NewDecoder(rr.Body).Decode(&card); err != nil {
		t.Fatalf("failed to decode response: %v", err)
	}
	if card.Name != "Fire Bolt" {
		t.Errorf("expected 'Fire Bolt', got %q", card.Name)
	}
}

func TestCreateCard_InvalidBody(t *testing.T) {
	setupTestDB(t)
	rr := httptest.NewRecorder()
	cardservices.CreateCard(rr, httptest.NewRequest(http.MethodPost, "/cards", bytes.NewBufferString("bad")))
	if rr.Code != http.StatusBadRequest {
		t.Errorf("expected 400, got %d", rr.Code)
	}
}

func TestCreateCard_DBError(t *testing.T) {
	setupBrokenDB(t)
	rr := httptest.NewRecorder()
	cardservices.CreateCard(rr, jsonRequest(t, http.MethodPost, "/cards", map[string]interface{}{"cid": 1, "name": "Card", "type": "Spell"}))
	if rr.Code != http.StatusInternalServerError {
		t.Errorf("expected 500, got %d", rr.Code)
	}
}

func TestListCards_Empty(t *testing.T) {
	setupTestDB(t)
	rr := httptest.NewRecorder()
	cardservices.ListCards(rr, httptest.NewRequest(http.MethodGet, "/cards", nil))
	if rr.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d", rr.Code)
	}
	var cards []models.Card
	if err := json.NewDecoder(rr.Body).Decode(&cards); err != nil {
		t.Fatalf("failed to decode response: %v", err)
	}
	if len(cards) != 0 {
		t.Errorf("expected empty, got %d", len(cards))
	}
}

func TestListCards_ReturnsSeedData(t *testing.T) {
	db := setupTestDB(t)
	seedCards(t, db, []models.Card{{Cid: 1, Name: "A"}, {Cid: 2, Name: "B"}})
	rr := httptest.NewRecorder()
	cardservices.ListCards(rr, httptest.NewRequest(http.MethodGet, "/cards", nil))
	var cards []models.Card
	if err := json.NewDecoder(rr.Body).Decode(&cards); err != nil {
		t.Fatalf("failed to decode response: %v", err)
	}
	if len(cards) != 2 {
		t.Errorf("expected 2, got %d", len(cards))
	}
}

func TestListCards_OrderedByCid(t *testing.T) {
	db := setupTestDB(t)
	seedCards(t, db, []models.Card{{Cid: 3, Name: "C"}, {Cid: 1, Name: "A"}, {Cid: 2, Name: "B"}})
	rr := httptest.NewRecorder()
	cardservices.ListCards(rr, httptest.NewRequest(http.MethodGet, "/cards", nil))
	var cards []models.Card
	if err := json.NewDecoder(rr.Body).Decode(&cards); err != nil {
		t.Fatalf("failed to decode response: %v", err)
	}
	for i := 1; i < len(cards); i++ {
		if cards[i].Cid < cards[i-1].Cid {
			t.Errorf("cards not ordered by cid")
		}
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

func TestUpdateCard_UpdateName(t *testing.T) {
	db := setupTestDB(t)
	seedCards(t, db, []models.Card{{Cid: 1, Name: "Old"}})
	rr := httptest.NewRecorder()
	cardservices.UpdateCard(rr, jsonRequest(t, http.MethodPut, "/cards", map[string]interface{}{"cid": 1, "name": "New"}))
	if rr.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d: %s", rr.Code, rr.Body.String())
	}
	var card models.Card
	if err := json.NewDecoder(rr.Body).Decode(&card); err != nil {
		t.Fatalf("failed to decode response: %v", err)
	}
	if card.Name != "New" {
		t.Errorf("expected 'New', got %q", card.Name)
	}
}

func TestUpdateCard_UpdateType(t *testing.T) {
	db := setupTestDB(t)
	seedCards(t, db, []models.Card{{Cid: 1, Name: "Card", Type: "OldType"}})
	rr := httptest.NewRecorder()
	cardservices.UpdateCard(rr, jsonRequest(t, http.MethodPut, "/cards", map[string]interface{}{"cid": 1, "type": "NewType"}))
	if rr.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d", rr.Code)
	}
	var card models.Card
	if err := json.NewDecoder(rr.Body).Decode(&card); err != nil {
		t.Fatalf("failed to decode response: %v", err)
	}
	if card.Type != "NewType" {
		t.Errorf("expected 'NewType', got %q", card.Type)
	}
}

func TestUpdateCard_UpdateCost(t *testing.T) {
	db := setupTestDB(t)
	seedCards(t, db, []models.Card{{Cid: 1, Name: "Card", Cost: 1}})
	cost := 5
	rr := httptest.NewRecorder()
	cardservices.UpdateCard(rr, jsonRequest(t, http.MethodPut, "/cards", map[string]interface{}{"cid": 1, "cost": cost}))
	if rr.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d", rr.Code)
	}
	var card models.Card
	if err := json.NewDecoder(rr.Body).Decode(&card); err != nil {
		t.Fatalf("failed to decode response: %v", err)
	}
	if card.Cost != 5 {
		t.Errorf("expected cost 5, got %d", card.Cost)
	}
}

func TestUpdateCard_UpdateValue(t *testing.T) {
	db := setupTestDB(t)
	seedCards(t, db, []models.Card{{Cid: 1, Name: "Card", Value: 1}})
	rr := httptest.NewRecorder()
	cardservices.UpdateCard(rr, jsonRequest(t, http.MethodPut, "/cards", map[string]interface{}{"cid": 1, "value": 8}))
	if rr.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d", rr.Code)
	}
	var card models.Card
	if err := json.NewDecoder(rr.Body).Decode(&card); err != nil {
		t.Fatalf("failed to decode response: %v", err)
	}
	if card.Value != 8 {
		t.Errorf("expected value 8, got %d", card.Value)
	}
}

func TestUpdateCard_UpdatePower(t *testing.T) {
	db := setupTestDB(t)
	seedCards(t, db, []models.Card{{Cid: 1, Name: "Card", Power: 1}})
	rr := httptest.NewRecorder()
	cardservices.UpdateCard(rr, jsonRequest(t, http.MethodPut, "/cards", map[string]interface{}{"cid": 1, "power": 9}))
	if rr.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d", rr.Code)
	}
	var card models.Card
	if err := json.NewDecoder(rr.Body).Decode(&card); err != nil {
		t.Fatalf("failed to decode response: %v", err)
	}
	if card.Power != 9 {
		t.Errorf("expected power 9, got %d", card.Power)
	}
}

func TestUpdateCard_UpdateToughness(t *testing.T) {
	db := setupTestDB(t)
	seedCards(t, db, []models.Card{{Cid: 1, Name: "Card", Toughness: 1}})
	rr := httptest.NewRecorder()
	cardservices.UpdateCard(rr, jsonRequest(t, http.MethodPut, "/cards", map[string]interface{}{"cid": 1, "toughness": 7}))
	if rr.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d", rr.Code)
	}
	var card models.Card
	if err := json.NewDecoder(rr.Body).Decode(&card); err != nil {
		t.Fatalf("failed to decode response: %v", err)
	}
	if card.Toughness != 7 {
		t.Errorf("expected toughness 7, got %d", card.Toughness)
	}
}

func TestUpdateCard_UpdateEffect(t *testing.T) {
	db := setupTestDB(t)
	seedCards(t, db, []models.Card{{Cid: 1, Name: "Card", Effect: "old"}})
	rr := httptest.NewRecorder()
	cardservices.UpdateCard(rr, jsonRequest(t, http.MethodPut, "/cards", map[string]interface{}{"cid": 1, "effect": "new effect"}))
	if rr.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d", rr.Code)
	}
	var card models.Card
	if err := json.NewDecoder(rr.Body).Decode(&card); err != nil {
		t.Fatalf("failed to decode response: %v", err)
	}
	if card.Effect != "new effect" {
		t.Errorf("expected 'new effect', got %q", card.Effect)
	}
}

func TestUpdateCard_NoValidFields(t *testing.T) {
	db := setupTestDB(t)
	seedCards(t, db, []models.Card{{Cid: 1, Name: "Card"}})
	rr := httptest.NewRecorder()
	cardservices.UpdateCard(rr, jsonRequest(t, http.MethodPut, "/cards", map[string]interface{}{"cid": 1}))
	if rr.Code != http.StatusBadRequest {
		t.Errorf("expected 400, got %d", rr.Code)
	}
}

func TestUpdateCard_NotFound(t *testing.T) {
	setupTestDB(t)
	rr := httptest.NewRecorder()
	cardservices.UpdateCard(rr, jsonRequest(t, http.MethodPut, "/cards", map[string]interface{}{"cid": 99, "name": "Ghost"}))
	if rr.Code != http.StatusNotFound {
		t.Errorf("expected 404, got %d", rr.Code)
	}
}

func TestUpdateCard_InvalidBody(t *testing.T) {
	setupTestDB(t)
	rr := httptest.NewRecorder()
	cardservices.UpdateCard(rr, httptest.NewRequest(http.MethodPut, "/cards", bytes.NewBufferString("bad")))
	if rr.Code != http.StatusBadRequest {
		t.Errorf("expected 400, got %d", rr.Code)
	}
}

func TestUpdateCard_DBError(t *testing.T) {
	setupBrokenDB(t)
	rr := httptest.NewRecorder()
	cardservices.UpdateCard(rr, jsonRequest(t, http.MethodPut, "/cards", map[string]interface{}{"cid": 1, "name": "New"}))
	if rr.Code != http.StatusNotFound {
		t.Errorf("expected 404, got %d", rr.Code)
	}
}

func TestDeleteCard_Success(t *testing.T) {
	db := setupTestDB(t)
	seedCards(t, db, []models.Card{{Cid: 1, Name: "Del"}})
	rr := httptest.NewRecorder()
	cardservices.DeleteCard(rr, jsonRequest(t, http.MethodDelete, "/cards", map[string]interface{}{"cid": 1}))
	if rr.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d", rr.Code)
	}
	var count int64
	db.Model(&models.Card{}).Where("cid = ?", 1).Count(&count)
	if count != 0 {
		t.Error("expected card to be deleted")
	}
}

func TestDeleteCard_NotFound(t *testing.T) {
	setupTestDB(t)
	rr := httptest.NewRecorder()
	cardservices.DeleteCard(rr, jsonRequest(t, http.MethodDelete, "/cards", map[string]interface{}{"cid": 99}))
	if rr.Code != http.StatusNotFound {
		t.Errorf("expected 404, got %d", rr.Code)
	}
}

func TestDeleteCard_InvalidBody(t *testing.T) {
	setupTestDB(t)
	rr := httptest.NewRecorder()
	cardservices.DeleteCard(rr, httptest.NewRequest(http.MethodDelete, "/cards", bytes.NewBufferString("bad")))
	if rr.Code != http.StatusBadRequest {
		t.Errorf("expected 400, got %d", rr.Code)
	}
}

func TestDeleteCard_DBError(t *testing.T) {
	setupBrokenDB(t)
	rr := httptest.NewRecorder()
	cardservices.DeleteCard(rr, jsonRequest(t, http.MethodDelete, "/cards", map[string]interface{}{"cid": 1}))
	if rr.Code != http.StatusNotFound {
		t.Errorf("expected 404, got %d", rr.Code)
	}
}
