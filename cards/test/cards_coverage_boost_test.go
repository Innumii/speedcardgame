package services_test

import (
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"os"
	"path/filepath"
	"testing"

	"github.com/Ryanljk/speedcardgame/cards/models"
	cardservices "github.com/Ryanljk/speedcardgame/cards/services"
)

// ── UpdateCard additional branches ────────────────────────────────────────────
// NOTE: UpdateCard uses an else-if chain where Cost > -1 is always true when
// Name and Type are empty (int defaults to 0, and 0 > -1). This means only
// the Name, Type, and Cost branches are reachable. Tests reflect actual behaviour.

func TestUpdateCard_UpdateType(t *testing.T) {
	db := setupTestDB(t)
	seedCards(t, db, []models.Card{{Cid: 1, Name: "Card", Type: "OldType"}})

	// Type is only updated when Name is empty string — which it is here
	body := map[string]interface{}{"cid": 1, "name": "", "type": "NewType"}
	req := jsonRequest(t, http.MethodPut, "/cards", body)
	rr := httptest.NewRecorder()

	cardservices.UpdateCard(rr, req)

	if rr.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d: %s", rr.Code, rr.Body.String())
	}
	var card models.Card
	if err := json.NewDecoder(rr.Body).Decode(&card); err != nil {
		t.Fatalf("failed to decode response: %v", err)
	}
	if card.Type != "NewType" {
		t.Errorf("expected type 'NewType', got %q", card.Type)
	}
}

func TestUpdateCard_UpdateCost(t *testing.T) {
	db := setupTestDB(t)
	seedCards(t, db, []models.Card{{Cid: 1, Name: "Card", Cost: 1}})

	// When Name="" and Type="", Cost branch is hit (0 > -1 is true, sets cost to 0)
	// To actually set a meaningful cost, we send name="" type="" cost=5
	// But cost=5 still hits the Cost branch — it just sets it to 5
	body := map[string]interface{}{"cid": 1, "name": "", "type": "", "cost": 5}
	req := jsonRequest(t, http.MethodPut, "/cards", body)
	rr := httptest.NewRecorder()

	cardservices.UpdateCard(rr, req)

	if rr.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d: %s", rr.Code, rr.Body.String())
	}
	var card models.Card
	if err := json.NewDecoder(rr.Body).Decode(&card); err != nil {
		t.Fatalf("failed to decode response: %v", err)
	}
	if card.Cost != 5 {
		t.Errorf("expected cost 5, got %d", card.Cost)
	}
}

func TestUpdateCard_CostBranchAlwaysHitWhenNameAndTypeEmpty(t *testing.T) {
	db := setupTestDB(t)
	seedCards(t, db, []models.Card{{Cid: 1, Name: "Card", Cost: 99}})

	// Sending only cid with no name/type — Cost branch fires with value 0
	body := map[string]interface{}{"cid": 1}
	req := jsonRequest(t, http.MethodPut, "/cards", body)
	rr := httptest.NewRecorder()

	cardservices.UpdateCard(rr, req)

	// Returns 200 (Cost branch is hit, sets cost to 0)
	if rr.Code != http.StatusOK {
		t.Errorf("expected 200 (cost branch always fires), got %d", rr.Code)
	}
}

// ── ListCards additional coverage ─────────────────────────────────────────────

func TestListCards_MultipleCards(t *testing.T) {
	db := setupTestDB(t)
	seedCards(t, db, []models.Card{
		{Cid: 1, Name: "A", Type: "Spell", Cost: 1},
		{Cid: 2, Name: "B", Type: "Creature", Cost: 2},
		{Cid: 3, Name: "C", Type: "Defense", Cost: 3},
	})

	req := httptest.NewRequest(http.MethodGet, "/cards", nil)
	rr := httptest.NewRecorder()

	cardservices.ListCards(rr, req)

	if rr.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d", rr.Code)
	}
	var cards []models.Card
	if err := json.NewDecoder(rr.Body).Decode(&cards); err != nil {
		t.Fatalf("failed to decode response: %v", err)
	}
	if len(cards) != 3 {
		t.Errorf("expected 3 cards, got %d", len(cards))
	}
}

// ── ListDecks additional coverage ─────────────────────────────────────────────

func TestListDecks_MultipleDecks(t *testing.T) {
	db := setupTestDB(t)
	db.Create(&models.Deck{Uid: 1, Cards: models.CardCounts{1: 2}})
	db.Create(&models.Deck{Uid: 2, Cards: models.CardCounts{2: 3}})
	db.Create(&models.Deck{Uid: 3, Cards: models.CardCounts{3: 1}})

	req := httptest.NewRequest(http.MethodGet, "/decks", nil)
	rr := httptest.NewRecorder()

	cardservices.ListDecks(rr, req)

	if rr.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d", rr.Code)
	}
	var decks []models.Deck
	if err := json.NewDecoder(rr.Body).Decode(&decks); err != nil {
		t.Fatalf("failed to decode response: %v", err)
	}
	if len(decks) != 3 {
		t.Errorf("expected 3 decks, got %d", len(decks))
	}
}

// ── ListInventories additional coverage ───────────────────────────────────────

func TestListInventories_NormalizesOnRead(t *testing.T) {
	db := setupTestDB(t)
	seedInventory(t, db, models.Inventory{Uid: 1, Cards: models.CardCounts{1: 10}})

	req := httptest.NewRequest(http.MethodGet, "/inventories", nil)
	rr := httptest.NewRecorder()

	cardservices.ListInventories(rr, req)

	if rr.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d", rr.Code)
	}
	var invs []models.Inventory
	if err := json.NewDecoder(rr.Body).Decode(&invs); err != nil {
		t.Fatalf("failed to decode response: %v", err)
	}
	if len(invs) != 1 {
		t.Fatalf("expected 1 inventory, got %d", len(invs))
	}
	if invs[0].Cards[1] != 4 {
		t.Errorf("expected card 1 normalized to %d, got %d", 4, invs[0].Cards[1])
	}
}

func TestListInventories_MultipleInventories(t *testing.T) {
	db := setupTestDB(t)
	seedInventory(t, db, models.Inventory{Uid: 1, Cards: models.CardCounts{1: 2}})
	seedInventory(t, db, models.Inventory{Uid: 2, Cards: models.CardCounts{2: 1}})
	seedInventory(t, db, models.Inventory{Uid: 3, Cards: models.CardCounts{3: 3}})

	req := httptest.NewRequest(http.MethodGet, "/inventories", nil)
	rr := httptest.NewRecorder()

	cardservices.ListInventories(rr, req)

	if rr.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d", rr.Code)
	}
	var invs []models.Inventory
	if err := json.NewDecoder(rr.Body).Decode(&invs); err != nil {
		t.Fatalf("failed to decode response: %v", err)
	}
	if len(invs) != 3 {
		t.Errorf("expected 3 inventories, got %d", len(invs))
	}
}

// ── SeedCardsFromCSV additional branches ──────────────────────────────────────

func TestSeedCardsFromCSV_UpdatesExistingCard(t *testing.T) {
	db := setupTestDB(t)
	seedCards(t, db, []models.Card{{Cid: 1, Name: "Old Name", Type: "Spell", Cost: 1}})

	csvContent := "cid,name,type,cost,value,power,toughness,effect\n1,New Name,Creature,3,2,5,2,Updated\n"
	tmpFile := filepath.Join(t.TempDir(), "update.csv")
	if err := os.WriteFile(tmpFile, []byte(csvContent), 0644); err != nil {
		t.Fatalf("failed to write temp csv: %v", err)
	}

	if err := cardservices.SeedCardsFromCSV(db, tmpFile); err != nil {
		t.Fatalf("expected no error, got: %v", err)
	}

	var card models.Card
	db.First(&card, 1)
	if card.Name != "New Name" {
		t.Errorf("expected updated name 'New Name', got %q", card.Name)
	}
}

func TestSeedCardsFromCSV_MissingRequiredColumn(t *testing.T) {
	db := setupTestDB(t)

	csvContent := "cid,name,type,cost,value,power,toughness\n1,Card,Spell,1,1,1,1\n"
	tmpFile := filepath.Join(t.TempDir(), "bad.csv")
	if err := os.WriteFile(tmpFile, []byte(csvContent), 0644); err != nil {
		t.Fatalf("failed to write temp csv: %v", err)
	}

	err := cardservices.SeedCardsFromCSV(db, tmpFile)
	if err == nil {
		t.Error("expected error for missing column, got nil")
	}
}

func TestSeedCardsFromCSV_InvalidIntField(t *testing.T) {
	db := setupTestDB(t)

	csvContent := "cid,name,type,cost,value,power,toughness,effect\nNOTANINT,Card,Spell,1,1,1,1,none\n"
	tmpFile := filepath.Join(t.TempDir(), "badint.csv")
	if err := os.WriteFile(tmpFile, []byte(csvContent), 0644); err != nil {
		t.Fatalf("failed to write temp csv: %v", err)
	}

	err := cardservices.SeedCardsFromCSV(db, tmpFile)
	if err == nil {
		t.Error("expected error for invalid int field, got nil")
	}
}

func TestSeedCardsFromCSV_SkipsEmptyRows(t *testing.T) {
	db := setupTestDB(t)

	csvContent := "cid,name,type,cost,value,power,toughness,effect\n1,Card A,Spell,1,1,1,1,none\n\n2,Card B,Creature,2,2,2,2,none\n"
	tmpFile := filepath.Join(t.TempDir(), "empty_rows.csv")
	if err := os.WriteFile(tmpFile, []byte(csvContent), 0644); err != nil {
		t.Fatalf("failed to write temp csv: %v", err)
	}

	if err := cardservices.SeedCardsFromCSV(db, tmpFile); err != nil {
		t.Fatalf("expected no error, got: %v", err)
	}

	var count int64
	db.Model(&models.Card{}).Count(&count)
	if count != 2 {
		t.Errorf("expected 2 cards, got %d", count)
	}
}

func TestSeedCardsFromCSV_MissingNameOrType(t *testing.T) {
	db := setupTestDB(t)

	csvContent := "cid,name,type,cost,value,power,toughness,effect\n1,,Spell,1,1,1,1,none\n"
	tmpFile := filepath.Join(t.TempDir(), "noname.csv")
	if err := os.WriteFile(tmpFile, []byte(csvContent), 0644); err != nil {
		t.Fatalf("failed to write temp csv: %v", err)
	}

	err := cardservices.SeedCardsFromCSV(db, tmpFile)
	if err == nil {
		t.Error("expected error for missing name, got nil")
	}
}
