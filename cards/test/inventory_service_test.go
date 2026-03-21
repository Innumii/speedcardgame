package services_test

import (
	"encoding/json"
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

// ── ClampCardCount ─────────────────────────────────────────────────────────────

func TestClampCardCount_BelowZero(t *testing.T) {
	if got := cardservices.ClampCardCount(-1); got != 0 {
		t.Errorf("expected 0, got %d", got)
	}
}

func TestClampCardCount_Zero(t *testing.T) {
	if got := cardservices.ClampCardCount(0); got != 0 {
		t.Errorf("expected 0, got %d", got)
	}
}

func TestClampCardCount_InRange(t *testing.T) {
	if got := cardservices.ClampCardCount(2); got != 2 {
		t.Errorf("expected 2, got %d", got)
	}
}

func TestClampCardCount_AtMax(t *testing.T) {
	if got := cardservices.ClampCardCount(4); got != 4 {
		t.Errorf("expected 4, got %d", got)
	}
}

func TestClampCardCount_AboveMax(t *testing.T) {
	if got := cardservices.ClampCardCount(5); got != 4 {
		t.Errorf("expected 4, got %d", got)
	}
}

// ── NormalizeInventoryCards ────────────────────────────────────────────────────

func TestNormalizeInventoryCards_Nil(t *testing.T) {
	if cardservices.NormalizeInventoryCards(nil) {
		t.Error("expected false for nil")
	}
}

func TestNormalizeInventoryCards_RemovesNegativeCid(t *testing.T) {
	cards := models.CardCounts{-1: 2, 1: 2}
	cardservices.NormalizeInventoryCards(cards)
	if _, ok := cards[-1]; ok {
		t.Error("expected negative cid removed")
	}
}

func TestNormalizeInventoryCards_ClampsOverMax(t *testing.T) {
	cards := models.CardCounts{1: 10}
	cardservices.NormalizeInventoryCards(cards)
	if cards[1] != 4 {
		t.Errorf("expected 4, got %d", cards[1])
	}
}

func TestNormalizeInventoryCards_RemovesZero(t *testing.T) {
	cards := models.CardCounts{1: 0}
	cardservices.NormalizeInventoryCards(cards)
	if _, ok := cards[1]; ok {
		t.Error("expected zero qty removed")
	}
}

func TestNormalizeInventoryCards_ValidUnchanged(t *testing.T) {
	cards := models.CardCounts{1: 2, 2: 3}
	if changed := cardservices.NormalizeInventoryCards(cards); changed {
		t.Error("expected no changes")
	}
}

func TestNormalizeInventoryCards_ZeroQtyNoChange(t *testing.T) {
	cards := models.CardCounts{1: 0}
	if changed := cardservices.NormalizeInventoryCards(cards); changed {
		t.Error("expected changed=false when qty was already 0")
	}
}

// ── CreateInventory ────────────────────────────────────────────────────────────

func TestCreateInventory_DefaultCards(t *testing.T) {
	setupTestDB(t)
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
}

func TestCreateInventory_WithCards(t *testing.T) {
	setupTestDB(t)
	rr := httptest.NewRecorder()
	cardservices.CreateInventory(rr, jsonRequest(t, http.MethodPost, "/inventories", map[string]interface{}{"uid": 2, "cards": map[string]int{"1": 2}}))
	if rr.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d", rr.Code)
	}
}

func TestCreateInventory_InvalidBody(t *testing.T) {
	setupTestDB(t)
	rr := httptest.NewRecorder()
	cardservices.CreateInventory(rr, jsonRequest(t, http.MethodPost, "/inventories", "bad"))
	if rr.Code != http.StatusBadRequest {
		t.Errorf("expected 400, got %d", rr.Code)
	}
}

func TestCreateInventory_DBError(t *testing.T) {
	setupBrokenDB(t)
	rr := httptest.NewRecorder()
	cardservices.CreateInventory(rr, jsonRequest(t, http.MethodPost, "/inventories", map[string]interface{}{"uid": 1}))
	if rr.Code != http.StatusInternalServerError {
		t.Errorf("expected 500, got %d", rr.Code)
	}
}

// ── ListInventories ────────────────────────────────────────────────────────────

func TestListInventories_Empty(t *testing.T) {
	setupTestDB(t)
	rr := httptest.NewRecorder()
	cardservices.ListInventories(rr, httptest.NewRequest(http.MethodGet, "/inventories", nil))
	if rr.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d", rr.Code)
	}
}

func TestListInventories_NormalizesOnRead(t *testing.T) {
	db := setupTestDB(t)
	seedInventory(t, db, models.Inventory{Uid: 1, Cards: models.CardCounts{1: 10}})
	rr := httptest.NewRecorder()
	cardservices.ListInventories(rr, httptest.NewRequest(http.MethodGet, "/inventories", nil))
	var invs []models.Inventory
	json.NewDecoder(rr.Body).Decode(&invs)
	if invs[0].Cards[1] != 4 {
		t.Errorf("expected 4, got %d", invs[0].Cards[1])
	}
}

func TestListInventories_MultipleResults(t *testing.T) {
	db := setupTestDB(t)
	seedInventory(t, db, models.Inventory{Uid: 1, Cards: models.CardCounts{1: 2}})
	seedInventory(t, db, models.Inventory{Uid: 2, Cards: models.CardCounts{2: 1}})
	rr := httptest.NewRecorder()
	cardservices.ListInventories(rr, httptest.NewRequest(http.MethodGet, "/inventories", nil))
	var invs []models.Inventory
	json.NewDecoder(rr.Body).Decode(&invs)
	if len(invs) != 2 {
		t.Errorf("expected 2, got %d", len(invs))
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

// ── GetInventoryByUserID ───────────────────────────────────────────────────────

func TestGetInventoryByUserID_Success(t *testing.T) {
	db := setupTestDB(t)
	seedInventory(t, db, models.Inventory{Uid: 1, Cards: models.CardCounts{1: 2}})
	rr := httptest.NewRecorder()
	cardservices.GetInventoryByUserID(rr, withChiParam(httptest.NewRequest(http.MethodGet, "/inventories/1", nil), "uid", "1"))
	if rr.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d", rr.Code)
	}
}

func TestGetInventoryByUserID_NormalizesOnRead(t *testing.T) {
	db := setupTestDB(t)
	seedInventory(t, db, models.Inventory{Uid: 1, Cards: models.CardCounts{1: 10}})
	rr := httptest.NewRecorder()
	cardservices.GetInventoryByUserID(rr, withChiParam(httptest.NewRequest(http.MethodGet, "/inventories/1", nil), "uid", "1"))
	var inv models.Inventory
	json.NewDecoder(rr.Body).Decode(&inv)
	if inv.Cards[1] != 4 {
		t.Errorf("expected 4, got %d", inv.Cards[1])
	}
}

func TestGetInventoryByUserID_MissingUID(t *testing.T) {
	setupTestDB(t)
	rr := httptest.NewRecorder()
	cardservices.GetInventoryByUserID(rr, withChiParam(httptest.NewRequest(http.MethodGet, "/inventories/", nil), "uid", ""))
	if rr.Code != http.StatusBadRequest {
		t.Errorf("expected 400, got %d", rr.Code)
	}
}

func TestGetInventoryByUserID_NotFound(t *testing.T) {
	setupTestDB(t)
	rr := httptest.NewRecorder()
	cardservices.GetInventoryByUserID(rr, withChiParam(httptest.NewRequest(http.MethodGet, "/inventories/99", nil), "uid", "99"))
	if rr.Code != http.StatusInternalServerError {
		t.Errorf("expected 500, got %d", rr.Code)
	}
}

func TestGetInventoryByUserID_DBError(t *testing.T) {
	setupBrokenDB(t)
	rr := httptest.NewRecorder()
	cardservices.GetInventoryByUserID(rr, withChiParam(httptest.NewRequest(http.MethodGet, "/inventories/1", nil), "uid", "1"))
	if rr.Code != http.StatusInternalServerError {
		t.Errorf("expected 500, got %d", rr.Code)
	}
}

func TestGetInventoryByUserID_NormalizeSaveFails(t *testing.T) {
	db, _ := gorm.Open(sqlite.Open(":memory:"), &gorm.Config{Logger: logger.Default.LogMode(logger.Silent)})
	if err := db.AutoMigrate(&models.Card{}, &models.Deck{}, &models.Inventory{}); err != nil {
    t.Fatalf("failed to migrate: %v", err)
}
	db.Create(&models.Inventory{Uid: 1, Cards: models.CardCounts{1: 10}})
	sqlDB, _ := db.DB()
	sqlDB.Close()
	prev := config.DB
	config.DB = db
	t.Cleanup(func() { config.DB = prev })
	rr := httptest.NewRecorder()
	cardservices.GetInventoryByUserID(rr, withChiParam(httptest.NewRequest(http.MethodGet, "/inventories/1", nil), "uid", "1"))
	if rr.Code != http.StatusInternalServerError {
		t.Errorf("expected 500, got %d", rr.Code)
	}
}

// ── UpdateInventory ────────────────────────────────────────────────────────────

func TestUpdateInventory_AddsCards(t *testing.T) {
	db := setupTestDB(t)
	seedInventory(t, db, models.Inventory{Uid: 1, Cards: models.CardCounts{1: 1}})
	rr := httptest.NewRecorder()
	cardservices.UpdateInventory(rr, jsonRequest(t, http.MethodPut, "/inventories", map[string]interface{}{"uid": 1, "cards": map[string]int{"1": 2}}))
	if rr.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d", rr.Code)
	}
	var inv models.Inventory
	json.NewDecoder(rr.Body).Decode(&inv)
	if inv.Cards[1] != 3 {
		t.Errorf("expected 3, got %d", inv.Cards[1])
	}
}

func TestUpdateInventory_ClampsAtMax(t *testing.T) {
	db := setupTestDB(t)
	seedInventory(t, db, models.Inventory{Uid: 1, Cards: models.CardCounts{1: 3}})
	rr := httptest.NewRecorder()
	cardservices.UpdateInventory(rr, jsonRequest(t, http.MethodPut, "/inventories", map[string]interface{}{"uid": 1, "cards": map[string]int{"1": 4}}))
	var inv models.Inventory
	json.NewDecoder(rr.Body).Decode(&inv)
	if inv.Cards[1] != 4 {
		t.Errorf("expected 4, got %d", inv.Cards[1])
	}
}

func TestUpdateInventory_NilCards(t *testing.T) {
	db := setupTestDB(t)
	db.Exec("INSERT INTO inventories (uid, cards) VALUES (?, NULL)", 1)
	rr := httptest.NewRecorder()
	cardservices.UpdateInventory(rr, jsonRequest(t, http.MethodPut, "/inventories", map[string]interface{}{"uid": 1, "cards": map[string]int{"1": 2}}))
	if rr.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d: %s", rr.Code, rr.Body.String())
	}
}

func TestUpdateInventory_SkipsInvalidCid(t *testing.T) {
	db := setupTestDB(t)
	seedInventory(t, db, models.Inventory{Uid: 1, Cards: models.CardCounts{1: 1}})
	rr := httptest.NewRecorder()
	cardservices.UpdateInventory(rr, jsonRequest(t, http.MethodPut, "/inventories", map[string]interface{}{"uid": 1, "cards": map[string]int{"0": 2, "1": 0}}))
	if rr.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d", rr.Code)
	}
}

func TestUpdateInventory_NotFound(t *testing.T) {
	setupTestDB(t)
	rr := httptest.NewRecorder()
	cardservices.UpdateInventory(rr, jsonRequest(t, http.MethodPut, "/inventories", map[string]interface{}{"uid": 99, "cards": map[string]int{"1": 1}}))
	if rr.Code != http.StatusInternalServerError {
		t.Errorf("expected 500, got %d", rr.Code)
	}
}

func TestUpdateInventory_InvalidBody(t *testing.T) {
	setupTestDB(t)
	rr := httptest.NewRecorder()
	cardservices.UpdateInventory(rr, jsonRequest(t, http.MethodPut, "/inventories", "bad"))
	if rr.Code != http.StatusBadRequest {
		t.Errorf("expected 400, got %d", rr.Code)
	}
}

func TestUpdateInventory_DBError(t *testing.T) {
	setupBrokenDB(t)
	rr := httptest.NewRecorder()
	cardservices.UpdateInventory(rr, jsonRequest(t, http.MethodPut, "/inventories", map[string]interface{}{"uid": 1, "cards": map[string]int{"1": 1}}))
	if rr.Code != http.StatusInternalServerError {
		t.Errorf("expected 500, got %d", rr.Code)
	}
}

// ── UpdateInventoryCoins ───────────────────────────────────────────────────────

func TestUpdateInventoryCoins_Success(t *testing.T) {
	db := setupTestDB(t)
	seedInventory(t, db, models.Inventory{Uid: 1, Cards: models.CardCounts{1: 2}, Coins: 100})
	rr := httptest.NewRecorder()
	cardservices.UpdateInventoryCoins(rr, jsonRequest(t, http.MethodPut, "/inventories/coins", map[string]interface{}{"uid": 1, "coins": 500}))
	if rr.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d: %s", rr.Code, rr.Body.String())
	}
	var inv models.Inventory
	db.Where("uid = ?", 1).First(&inv)
	if inv.Coins != 500 {
		t.Errorf("expected 500, got %d", inv.Coins)
	}
}

func TestUpdateInventoryCoins_InvalidBody(t *testing.T) {
	setupTestDB(t)
	rr := httptest.NewRecorder()
	cardservices.UpdateInventoryCoins(rr, httptest.NewRequest(http.MethodPut, "/inventories/coins", nil))
	if rr.Code != http.StatusBadRequest {
		t.Errorf("expected 400, got %d", rr.Code)
	}
}

func TestUpdateInventoryCoins_NotFound(t *testing.T) {
	setupTestDB(t)
	rr := httptest.NewRecorder()
	cardservices.UpdateInventoryCoins(rr, jsonRequest(t, http.MethodPut, "/inventories/coins", map[string]interface{}{"uid": 99, "coins": 500}))
	if rr.Code != http.StatusInternalServerError {
		t.Errorf("expected 500, got %d", rr.Code)
	}
}

func TestUpdateInventoryCoins_DBError(t *testing.T) {
	setupBrokenDB(t)
	rr := httptest.NewRecorder()
	cardservices.UpdateInventoryCoins(rr, jsonRequest(t, http.MethodPut, "/inventories/coins", map[string]interface{}{"uid": 1, "coins": 500}))
	if rr.Code != http.StatusInternalServerError {
		t.Errorf("expected 500, got %d", rr.Code)
	}
}

func TestUpdateInventoryCoins_FindFails(t *testing.T) {
	setupSeedThenBreak(t, models.Inventory{Uid: 1, Cards: models.CardCounts{1: 2}, Coins: 100})
	rr := httptest.NewRecorder()
	cardservices.UpdateInventoryCoins(rr, jsonRequest(t, http.MethodPut, "/inventories/coins", map[string]interface{}{"uid": 1, "coins": 500}))
	if rr.Code != http.StatusInternalServerError {
		t.Errorf("expected 500, got %d", rr.Code)
	}
}

// ── AddInventoryCoins ──────────────────────────────────────────────────────────

func TestAddInventoryCoins_Success(t *testing.T) {
	db := setupTestDB(t)
	seedInventory(t, db, models.Inventory{Uid: 1, Cards: models.CardCounts{1: 2}, Coins: 100})
	rr := httptest.NewRecorder()
	cardservices.AddInventoryCoins(rr, jsonRequest(t, http.MethodPut, "/inventories/coins/add", map[string]interface{}{"uid": 1, "coins": 200}))
	if rr.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d: %s", rr.Code, rr.Body.String())
	}
	var inv models.Inventory
	db.Where("uid = ?", 1).First(&inv)
	if inv.Coins != 300 {
		t.Errorf("expected 300, got %d", inv.Coins)
	}
}

func TestAddInventoryCoins_InvalidBody(t *testing.T) {
	setupTestDB(t)
	rr := httptest.NewRecorder()
	cardservices.AddInventoryCoins(rr, httptest.NewRequest(http.MethodPut, "/inventories/coins/add", nil))
	if rr.Code != http.StatusBadRequest {
		t.Errorf("expected 400, got %d", rr.Code)
	}
}

func TestAddInventoryCoins_NotFound(t *testing.T) {
	setupTestDB(t)
	rr := httptest.NewRecorder()
	cardservices.AddInventoryCoins(rr, jsonRequest(t, http.MethodPut, "/inventories/coins/add", map[string]interface{}{"uid": 99, "coins": 100}))
	if rr.Code != http.StatusInternalServerError {
		t.Errorf("expected 500, got %d", rr.Code)
	}
}

func TestAddInventoryCoins_DBError(t *testing.T) {
	setupBrokenDB(t)
	rr := httptest.NewRecorder()
	cardservices.AddInventoryCoins(rr, jsonRequest(t, http.MethodPut, "/inventories/coins/add", map[string]interface{}{"uid": 1, "coins": 100}))
	if rr.Code != http.StatusInternalServerError {
		t.Errorf("expected 500, got %d", rr.Code)
	}
}

func TestAddInventoryCoins_FindFails(t *testing.T) {
	setupSeedThenBreak(t, models.Inventory{Uid: 1, Cards: models.CardCounts{1: 2}, Coins: 100})
	rr := httptest.NewRecorder()
	cardservices.AddInventoryCoins(rr, jsonRequest(t, http.MethodPut, "/inventories/coins/add", map[string]interface{}{"uid": 1, "coins": 200}))
	if rr.Code != http.StatusInternalServerError {
		t.Errorf("expected 500, got %d", rr.Code)
	}
}
