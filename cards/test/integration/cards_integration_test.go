//go:build integration

package integration_test

import (
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"testing"

	"github.com/FYL-Studios/speedcardgame/cards/models"
	cardservices "github.com/FYL-Studios/speedcardgame/cards/services"
)

// ════════════════════════════════════════════════════════════════
// CARD SERVICE - Integration Tests
// ════════════════════════════════════════════════════════════════

func TestIntegration_CreateCard_Success(t *testing.T) {
	db := connectTestDB(t)
	cleanDB(t, db)

	body := map[string]interface{}{
		"cid": 1, "name": "Fire Bolt", "type": "Spell",
		"cost": 2, "value": 3, "power": 4, "toughness": 1, "effect": "Deal 3 damage",
	}
	rr := httptest.NewRecorder()
	cardservices.CreateCard(rr, jsonRequest(t, http.MethodPost, "/cards", body))

	if rr.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d: %s", rr.Code, rr.Body.String())
	}
	var card models.Card
	json.NewDecoder(rr.Body).Decode(&card)
	if card.Name != "Fire Bolt" {
		t.Errorf("expected 'Fire Bolt', got %q", card.Name)
	}

	// Verify persisted in real DB
	var dbCard models.Card
	if err := db.First(&dbCard, 1).Error; err != nil {
		t.Errorf("card not found in DB: %v", err)
	}
	if dbCard.Name != "Fire Bolt" {
		t.Errorf("expected DB card name 'Fire Bolt', got %q", dbCard.Name)
	}
}

func TestIntegration_ListCards_OrderedByCid(t *testing.T) {
	db := connectTestDB(t)
	cleanDB(t, db)

	// Insert out of order
	db.Create(&models.Card{Cid: 3, Name: "C", Type: "Spell"})
	db.Create(&models.Card{Cid: 1, Name: "A", Type: "Spell"})
	db.Create(&models.Card{Cid: 2, Name: "B", Type: "Spell"})

	rr := httptest.NewRecorder()
	cardservices.ListCards(rr, httptest.NewRequest(http.MethodGet, "/cards", nil))

	if rr.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d", rr.Code)
	}
	var cards []models.Card
	json.NewDecoder(rr.Body).Decode(&cards)
	if len(cards) != 3 {
		t.Fatalf("expected 3 cards, got %d", len(cards))
	}
	for i := 1; i < len(cards); i++ {
		if cards[i].Cid < cards[i-1].Cid {
			t.Errorf("cards not ordered by cid")
		}
	}
}

func TestIntegration_UpdateCard_AllFields(t *testing.T) {
	db := connectTestDB(t)
	cleanDB(t, db)
	db.Create(&models.Card{Cid: 1, Name: "Old Name", Type: "Spell", Cost: 1, Value: 1, Power: 1, Toughness: 1, Effect: "old"})

	cost := 5
	body := map[string]interface{}{"cid": 1, "name": "New Name", "type": "Creature", "cost": cost, "effect": "new effect"}
	rr := httptest.NewRecorder()
	cardservices.UpdateCard(rr, jsonRequest(t, http.MethodPut, "/cards", body))

	if rr.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d: %s", rr.Code, rr.Body.String())
	}

	var dbCard models.Card
	db.First(&dbCard, 1)
	if dbCard.Name != "New Name" {
		t.Errorf("expected 'New Name', got %q", dbCard.Name)
	}
	if dbCard.Cost != 5 {
		t.Errorf("expected cost 5, got %d", dbCard.Cost)
	}
}

func TestIntegration_DeleteCard_Success(t *testing.T) {
	db := connectTestDB(t)
	cleanDB(t, db)
	db.Create(&models.Card{Cid: 1, Name: "ToDelete", Type: "Spell"})

	rr := httptest.NewRecorder()
	cardservices.DeleteCard(rr, jsonRequest(t, http.MethodDelete, "/cards", map[string]interface{}{"cid": 1}))

	if rr.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d", rr.Code)
	}
	var count int64
	db.Model(&models.Card{}).Where("cid = ?", 1).Count(&count)
	if count != 0 {
		t.Error("expected card deleted from DB")
	}
}

// ════════════════════════════════════════════════════════════════
// DECK SERVICE - Integration Tests
// ════════════════════════════════════════════════════════════════

func TestIntegration_CreateDeck_Success(t *testing.T) {
	t.Setenv("DECK_SIZE", "3")
	db := connectTestDB(t)
	cleanDB(t, db)

	body := map[string]interface{}{
		"uid":   1,
		"cards": map[string]int{"1": 1, "2": 1, "3": 1},
	}
	rr := httptest.NewRecorder()
	cardservices.CreateDeck(rr, jsonRequest(t, http.MethodPost, "/decks", body))

	if rr.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d: %s", rr.Code, rr.Body.String())
	}

	var deck models.Deck
	db.Where("uid = ?", 1).First(&deck)
	if deck.Uid != 1 {
		t.Errorf("expected deck uid 1 in DB, got %d", deck.Uid)
	}
	if len(deck.Cards) != 3 {
		t.Errorf("expected 3 card entries, got %d", len(deck.Cards))
	}
}

func TestIntegration_CreateDeck_ReplacesExisting(t *testing.T) {
	t.Setenv("DECK_SIZE", "3")
	db := connectTestDB(t)
	cleanDB(t, db)
	db.Create(&models.Deck{Uid: 1, Cards: models.CardCounts{1: 1, 2: 1, 3: 1}})

	body := map[string]interface{}{
		"uid":   1,
		"cards": map[string]int{"4": 1, "5": 1, "6": 1},
	}
	rr := httptest.NewRecorder()
	cardservices.CreateDeck(rr, jsonRequest(t, http.MethodPost, "/decks", body))

	if rr.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d", rr.Code)
	}
	var deck models.Deck
	db.Where("uid = ?", 1).First(&deck)
	if _, ok := deck.Cards[1]; ok {
		t.Error("expected old cards to be replaced in DB")
	}
	if deck.Cards[4] != 1 {
		t.Errorf("expected new card 4 in deck")
	}
}

func TestIntegration_ListDecks_ReturnsAll(t *testing.T) {
	db := connectTestDB(t)
	cleanDB(t, db)
	db.Create(&models.Deck{Uid: 1, Cards: models.CardCounts{1: 2}})
	db.Create(&models.Deck{Uid: 2, Cards: models.CardCounts{2: 3}})

	rr := httptest.NewRecorder()
	cardservices.ListDecks(rr, httptest.NewRequest(http.MethodGet, "/decks", nil))

	if rr.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d", rr.Code)
	}
	var decks []models.Deck
	json.NewDecoder(rr.Body).Decode(&decks)
	if len(decks) != 2 {
		t.Errorf("expected 2 decks, got %d", len(decks))
	}
}

func TestIntegration_DeleteDeck_Success(t *testing.T) {
	db := connectTestDB(t)
	cleanDB(t, db)
	db.Create(&models.Deck{Uid: 1, Cards: models.CardCounts{1: 1}})

	rr := httptest.NewRecorder()
	cardservices.DeleteDeck(rr, jsonRequest(t, http.MethodDelete, "/decks", map[string]interface{}{"uid": 1}))

	if rr.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d", rr.Code)
	}
	var count int64
	db.Model(&models.Deck{}).Where("uid = ?", 1).Count(&count)
	if count != 0 {
		t.Error("expected deck deleted")
	}
}

func TestIntegration_GetDeckByUserID_Success(t *testing.T) {
	db := connectTestDB(t)
	cleanDB(t, db)
	db.Create(&models.Deck{Uid: 1, Cards: models.CardCounts{1: 2, 2: 3}})

	rr := httptest.NewRecorder()
	cardservices.GetDeckByUserID(rr, withChiParam(httptest.NewRequest(http.MethodGet, "/decks/1", nil), "uid", "1"))

	if rr.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d", rr.Code)
	}
	var deck models.Deck
	json.NewDecoder(rr.Body).Decode(&deck)
	if deck.Uid != 1 {
		t.Errorf("expected uid 1, got %d", deck.Uid)
	}
	if deck.Cards[1] != 2 {
		t.Errorf("expected card 1 qty 2, got %d", deck.Cards[1])
	}
}

func TestIntegration_FillDeckForUser_FromInventory(t *testing.T) {
	t.Setenv("DECK_SIZE", "5")
	db := connectTestDB(t)
	cleanDB(t, db)

	// Seed cards and inventory
	db.Create(&models.Card{Cid: 1, Name: "A", Type: "Spell"})
	db.Create(&models.Card{Cid: 2, Name: "B", Type: "Spell"})
	db.Create(&models.Card{Cid: 3, Name: "C", Type: "Spell"})
	db.Create(&models.Inventory{Uid: 1, Cards: models.CardCounts{1: 3, 2: 2}})

	rr := httptest.NewRecorder()
	cardservices.FillDeckForUser(rr, jsonRequest(t, http.MethodPost, "/decks/fill", map[string]interface{}{"uid": 1}))

	if rr.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d: %s", rr.Code, rr.Body.String())
	}
	var deck models.Deck
	json.NewDecoder(rr.Body).Decode(&deck)
	total := 0
	for _, qty := range deck.Cards {
		total += qty
	}
	if total != 5 {
		t.Errorf("expected deck total 5, got %d", total)
	}
}

func TestIntegration_FillDecksFromInventories(t *testing.T) {
	t.Setenv("DECK_SIZE", "3")
	db := connectTestDB(t)
	cleanDB(t, db)

	db.Create(&models.Card{Cid: 1, Name: "A", Type: "Spell"})
	db.Create(&models.Card{Cid: 2, Name: "B", Type: "Spell"})
	db.Create(&models.Card{Cid: 3, Name: "C", Type: "Spell"})
	db.Create(&models.Inventory{Uid: 1, Cards: models.CardCounts{1: 2, 2: 1}})
	db.Create(&models.Inventory{Uid: 2, Cards: models.CardCounts{3: 3}})

	if err := cardservices.FillDecksFromInventories(db); err != nil {
		t.Fatalf("expected no error, got: %v", err)
	}

	var count int64
	db.Model(&models.Deck{}).Count(&count)
	if count != 2 {
		t.Errorf("expected 2 decks, got %d", count)
	}
}

// ════════════════════════════════════════════════════════════════
// INVENTORY SERVICE - Integration Tests
// ════════════════════════════════════════════════════════════════

func TestIntegration_CreateInventory_DefaultCards(t *testing.T) {
	db := connectTestDB(t)
	cleanDB(t, db)

	rr := httptest.NewRecorder()
	cardservices.CreateInventory(rr, jsonRequest(t, http.MethodPost, "/inventories", map[string]interface{}{"uid": 1}))

	if rr.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d: %s", rr.Code, rr.Body.String())
	}
	var inv models.Inventory
	json.NewDecoder(rr.Body).Decode(&inv)
	if len(inv.Cards) == 0 {
		t.Error("expected default cards")
	}
	if inv.Coins != 1000 {
		t.Errorf("expected 1000 coins, got %d", inv.Coins)
	}

	// Verify in DB
	var dbInv models.Inventory
	db.Where("uid = ?", 1).First(&dbInv)
	if dbInv.Coins != 1000 {
		t.Errorf("expected 1000 coins in DB, got %d", dbInv.Coins)
	}
}

func TestIntegration_CreateInventory_WithJSONB(t *testing.T) {
	db := connectTestDB(t)
	cleanDB(t, db)

	body := map[string]interface{}{
		"uid":   2,
		"cards": map[string]int{"1": 3, "2": 2, "3": 1},
		"coins": 500,
	}
	rr := httptest.NewRecorder()
	cardservices.CreateInventory(rr, jsonRequest(t, http.MethodPost, "/inventories", body))

	if rr.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d: %s", rr.Code, rr.Body.String())
	}

	// Verify JSONB stored and retrieved correctly
	var dbInv models.Inventory
	db.Where("uid = ?", 2).First(&dbInv)
	if dbInv.Cards[1] != 3 {
		t.Errorf("expected card 1 qty 3 from JSONB, got %d", dbInv.Cards[1])
	}
	if dbInv.Cards[2] != 2 {
		t.Errorf("expected card 2 qty 2 from JSONB, got %d", dbInv.Cards[2])
	}
}

func TestIntegration_ListInventories_ReturnsAll(t *testing.T) {
	db := connectTestDB(t)
	cleanDB(t, db)
	db.Create(&models.Inventory{Uid: 1, Cards: models.CardCounts{1: 2}})
	db.Create(&models.Inventory{Uid: 2, Cards: models.CardCounts{2: 3}})
	db.Create(&models.Inventory{Uid: 3, Cards: models.CardCounts{3: 1}})

	rr := httptest.NewRecorder()
	cardservices.ListInventories(rr, httptest.NewRequest(http.MethodGet, "/inventories", nil))

	if rr.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d", rr.Code)
	}
	var invs []models.Inventory
	json.NewDecoder(rr.Body).Decode(&invs)
	if len(invs) != 3 {
		t.Errorf("expected 3, got %d", len(invs))
	}
}

func TestIntegration_GetInventoryByUserID_WithNormalization(t *testing.T) {
	db := connectTestDB(t)
	cleanDB(t, db)
	// Insert inventory with over-max card count — should be normalized on read
	db.Create(&models.Inventory{Uid: 1, Cards: models.CardCounts{1: 10, 2: 2}})

	rr := httptest.NewRecorder()
	cardservices.GetInventoryByUserID(rr, withChiParam(httptest.NewRequest(http.MethodGet, "/inventories/1", nil), "uid", "1"))

	if rr.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d", rr.Code)
	}
	var inv models.Inventory
	json.NewDecoder(rr.Body).Decode(&inv)
	if inv.Cards[1] != 4 {
		t.Errorf("expected card 1 normalized to 4, got %d", inv.Cards[1])
	}

	// Confirm normalization was saved to DB
	var dbInv models.Inventory
	db.Where("uid = ?", 1).First(&dbInv)
	if dbInv.Cards[1] != 4 {
		t.Errorf("expected normalization persisted in DB, got %d", dbInv.Cards[1])
	}
}

func TestIntegration_UpdateInventory_AddsCards(t *testing.T) {
	db := connectTestDB(t)
	cleanDB(t, db)
	db.Create(&models.Inventory{Uid: 1, Cards: models.CardCounts{1: 1, 2: 2}})

	body := map[string]interface{}{"uid": 1, "cards": map[string]int{"1": 2, "3": 1}}
	rr := httptest.NewRecorder()
	cardservices.UpdateInventory(rr, jsonRequest(t, http.MethodPut, "/inventories", body))

	if rr.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d: %s", rr.Code, rr.Body.String())
	}
	var inv models.Inventory
	json.NewDecoder(rr.Body).Decode(&inv)
	if inv.Cards[1] != 3 {
		t.Errorf("expected card 1 qty 3, got %d", inv.Cards[1])
	}
	if inv.Cards[3] != 1 {
		t.Errorf("expected card 3 qty 1, got %d", inv.Cards[3])
	}

	// Verify persisted
	var dbInv models.Inventory
	db.Where("uid = ?", 1).First(&dbInv)
	if dbInv.Cards[1] != 3 {
		t.Errorf("expected DB card 1 qty 3, got %d", dbInv.Cards[1])
	}
}

func TestIntegration_UpdateInventoryCoins_SetsExact(t *testing.T) {
	db := connectTestDB(t)
	cleanDB(t, db)
	db.Create(&models.Inventory{Uid: 1, Cards: models.CardCounts{1: 2}, Coins: 100})

	rr := httptest.NewRecorder()
	cardservices.UpdateInventoryCoins(rr, jsonRequest(t, http.MethodPut, "/inventories/coins", map[string]interface{}{"uid": 1, "coins": 750}))

	if rr.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d: %s", rr.Code, rr.Body.String())
	}

	var dbInv models.Inventory
	db.Where("uid = ?", 1).First(&dbInv)
	if dbInv.Coins != 750 {
		t.Errorf("expected 750 coins in DB, got %d", dbInv.Coins)
	}
}

func TestIntegration_AddInventoryCoins_Accumulates(t *testing.T) {
	db := connectTestDB(t)
	cleanDB(t, db)
	db.Create(&models.Inventory{Uid: 1, Cards: models.CardCounts{1: 2}, Coins: 200})

	// Add coins twice
	cardservices.AddInventoryCoins(httptest.NewRecorder(), jsonRequest(t, http.MethodPut, "/inventories/coins/add", map[string]interface{}{"uid": 1, "coins": 100}))
	cardservices.AddInventoryCoins(httptest.NewRecorder(), jsonRequest(t, http.MethodPut, "/inventories/coins/add", map[string]interface{}{"uid": 1, "coins": 50}))

	var dbInv models.Inventory
	db.Where("uid = ?", 1).First(&dbInv)
	if dbInv.Coins != 350 {
		t.Errorf("expected 350 coins (200+100+50), got %d", dbInv.Coins)
	}
}
