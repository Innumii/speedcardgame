package services_test

import (
	"encoding/json"
	"errors"
	"net/http"
	"net/http/httptest"
	"testing"

	"github.com/Ryanljk/speedcardgame/cards/models"
	cardservices "github.com/Ryanljk/speedcardgame/cards/services"
	"gorm.io/driver/sqlite"
	"gorm.io/gorm"
	"gorm.io/gorm/logger"
)

func TestGetDeckSizeLimit_Default(t *testing.T) {
	t.Setenv("DECK_SIZE", "")
	if got := cardservices.GetDeckSizeLimit(); got != 30 {
		t.Errorf("expected 30, got %d", got)
	}
}

func TestGetDeckSizeLimit_Custom(t *testing.T) {
	t.Setenv("DECK_SIZE", "20")
	if got := cardservices.GetDeckSizeLimit(); got != 20 {
		t.Errorf("expected 20, got %d", got)
	}
}

func TestGetDeckSizeLimit_Invalid(t *testing.T) {
	t.Setenv("DECK_SIZE", "notanumber")
	if got := cardservices.GetDeckSizeLimit(); got != 30 {
		t.Errorf("expected 30, got %d", got)
	}
}

func TestGetDeckSizeLimit_Zero(t *testing.T) {
	t.Setenv("DECK_SIZE", "0")
	if got := cardservices.GetDeckSizeLimit(); got != 30 {
		t.Errorf("expected 30, got %d", got)
	}
}

func TestCountDeckCards_Empty(t *testing.T) {
	if got := cardservices.CountDeckCards(models.CardCounts{}); got != 0 {
		t.Errorf("expected 0, got %d", got)
	}
}

func TestCountDeckCards_Sums(t *testing.T) {
	if got := cardservices.CountDeckCards(models.CardCounts{1: 5, 2: 10, 3: 15}); got != 30 {
		t.Errorf("expected 30, got %d", got)
	}
}

func TestBuildDeckCounts_RespectsLimit(t *testing.T) {
	deck := cardservices.BuildDeckCounts(models.CardCounts{1: 10, 2: 10}, []models.Card{{Cid: 1}, {Cid: 2}, {Cid: 3}}, 5)
	if total := cardservices.CountDeckCards(deck); total != 5 {
		t.Errorf("expected 5, got %d", total)
	}
}

func TestBuildDeckCounts_NilInventory(t *testing.T) {
	deck := cardservices.BuildDeckCounts(nil, []models.Card{{Cid: 1}, {Cid: 2}, {Cid: 3}}, 3)
	if total := cardservices.CountDeckCards(deck); total != 3 {
		t.Errorf("expected 3, got %d", total)
	}
}

func TestBuildDeckCounts_InventoryPriority(t *testing.T) {
	deck := cardservices.BuildDeckCounts(models.CardCounts{5: 3}, []models.Card{{Cid: 1}, {Cid: 2}}, 3)
	if deck[5] != 3 {
		t.Errorf("expected 3 of card 5, got %d", deck[5])
	}
}

func TestCreateDeck_Success(t *testing.T) {
	t.Setenv("DECK_SIZE", "3")
	setupTestDB(t)
	rr := httptest.NewRecorder()
	cardservices.CreateDeck(rr, jsonRequest(t, http.MethodPost, "/decks", map[string]interface{}{"uid": 1, "cards": map[string]int{"1": 1, "2": 1, "3": 1}}))
	if rr.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d: %s", rr.Code, rr.Body.String())
	}
}

func TestCreateDeck_ReplacesExisting(t *testing.T) {
	t.Setenv("DECK_SIZE", "3")
	db := setupTestDB(t)
	db.Create(&models.Deck{Uid: 1, Cards: models.CardCounts{1: 1, 2: 1, 3: 1}})
	rr := httptest.NewRecorder()
	cardservices.CreateDeck(rr, jsonRequest(t, http.MethodPost, "/decks", map[string]interface{}{"uid": 1, "cards": map[string]int{"4": 1, "5": 1, "6": 1}}))
	if rr.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d", rr.Code)
	}
	var deck models.Deck
	if err := json.NewDecoder(rr.Body).Decode(&deck); err != nil {
		t.Fatalf("failed to decode response: %v", err)
	}
	if _, ok := deck.Cards[1]; ok {
		t.Error("expected old cards to be replaced")
	}
}

func TestCreateDeck_WrongSize(t *testing.T) {
	t.Setenv("DECK_SIZE", "3")
	setupTestDB(t)
	rr := httptest.NewRecorder()
	cardservices.CreateDeck(rr, jsonRequest(t, http.MethodPost, "/decks", map[string]interface{}{"uid": 1, "cards": map[string]int{"1": 1}}))
	if rr.Code != http.StatusBadRequest {
		t.Errorf("expected 400, got %d", rr.Code)
	}
}

func TestCreateDeck_InvalidUid(t *testing.T) {
	setupTestDB(t)
	rr := httptest.NewRecorder()
	cardservices.CreateDeck(rr, jsonRequest(t, http.MethodPost, "/decks", map[string]interface{}{"uid": 0}))
	if rr.Code != http.StatusBadRequest {
		t.Errorf("expected 400, got %d", rr.Code)
	}
}

func TestCreateDeck_InvalidBody(t *testing.T) {
	setupTestDB(t)
	rr := httptest.NewRecorder()
	cardservices.CreateDeck(rr, jsonRequest(t, http.MethodPost, "/decks", "bad"))
	if rr.Code != http.StatusBadRequest {
		t.Errorf("expected 400, got %d", rr.Code)
	}
}

func TestCreateDeck_DBError(t *testing.T) {
	t.Setenv("DECK_SIZE", "3")
	setupBrokenDB(t)
	rr := httptest.NewRecorder()
	cardservices.CreateDeck(rr, jsonRequest(t, http.MethodPost, "/decks", map[string]interface{}{"uid": 1, "cards": map[string]int{"1": 1, "2": 1, "3": 1}}))
	if rr.Code != http.StatusInternalServerError {
		t.Errorf("expected 500, got %d", rr.Code)
	}
}

func TestListDecks_Empty(t *testing.T) {
	setupTestDB(t)
	rr := httptest.NewRecorder()
	cardservices.ListDecks(rr, httptest.NewRequest(http.MethodGet, "/decks", nil))
	if rr.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d", rr.Code)
	}
}

func TestListDecks_ReturnsSeedData(t *testing.T) {
	db := setupTestDB(t)
	db.Create(&models.Deck{Uid: 1, Cards: models.CardCounts{1: 2}})
	db.Create(&models.Deck{Uid: 2, Cards: models.CardCounts{2: 3}})
	rr := httptest.NewRecorder()
	cardservices.ListDecks(rr, httptest.NewRequest(http.MethodGet, "/decks", nil))
	var decks []models.Deck
	if err := json.NewDecoder(rr.Body).Decode(&decks); err != nil {
		t.Fatalf("failed to decode response: %v", err)
	}
	if len(decks) != 2 {
		t.Errorf("expected 2, got %d", len(decks))
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

func TestDeleteDeck_Success(t *testing.T) {
	db := setupTestDB(t)
	db.Create(&models.Deck{Uid: 1, Cards: models.CardCounts{1: 1}})
	rr := httptest.NewRecorder()
	cardservices.DeleteDeck(rr, jsonRequest(t, http.MethodDelete, "/decks", map[string]interface{}{"uid": 1}))
	if rr.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d", rr.Code)
	}
}

func TestDeleteDeck_InvalidBody(t *testing.T) {
	setupTestDB(t)
	rr := httptest.NewRecorder()
	cardservices.DeleteDeck(rr, jsonRequest(t, http.MethodDelete, "/decks", "bad"))
	if rr.Code != http.StatusBadRequest {
		t.Errorf("expected 400, got %d", rr.Code)
	}
}

func TestDeleteDeck_DBError(t *testing.T) {
	setupBrokenDB(t)
	rr := httptest.NewRecorder()
	cardservices.DeleteDeck(rr, jsonRequest(t, http.MethodDelete, "/decks", map[string]interface{}{"uid": 1}))
	if rr.Code != http.StatusInternalServerError {
		t.Errorf("expected 500, got %d", rr.Code)
	}
}

func TestGetDeckByUserID_Success(t *testing.T) {
	db := setupTestDB(t)
	db.Create(&models.Deck{Uid: 1, Cards: models.CardCounts{1: 2}})
	rr := httptest.NewRecorder()
	cardservices.GetDeckByUserID(rr, withChiParam(httptest.NewRequest(http.MethodGet, "/decks/1", nil), "uid", "1"))
	if rr.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d", rr.Code)
	}
}

func TestGetDeckByUserID_NotFound(t *testing.T) {
	setupTestDB(t)
	rr := httptest.NewRecorder()
	cardservices.GetDeckByUserID(rr, withChiParam(httptest.NewRequest(http.MethodGet, "/decks/99", nil), "uid", "99"))
	if rr.Code != http.StatusNotFound {
		t.Errorf("expected 404, got %d", rr.Code)
	}
}

func TestGetDeckByUserID_InvalidUID(t *testing.T) {
	setupTestDB(t)
	rr := httptest.NewRecorder()
	cardservices.GetDeckByUserID(rr, withChiParam(httptest.NewRequest(http.MethodGet, "/decks/abc", nil), "uid", "abc"))
	if rr.Code != http.StatusBadRequest {
		t.Errorf("expected 400, got %d", rr.Code)
	}
}

func TestGetDeckByUserID_DBError(t *testing.T) {
	setupBrokenDB(t)
	rr := httptest.NewRecorder()
	cardservices.GetDeckByUserID(rr, withChiParam(httptest.NewRequest(http.MethodGet, "/decks/1", nil), "uid", "1"))
	if rr.Code != http.StatusNotFound {
		t.Errorf("expected 404, got %d", rr.Code)
	}
}

func TestFillDeckForUser_Success(t *testing.T) {
	t.Setenv("DECK_SIZE", "3")
	db := setupTestDB(t)
	seedCards(t, db, []models.Card{{Cid: 1}, {Cid: 2}, {Cid: 3}})
	seedInventory(t, db, models.Inventory{Uid: 1, Cards: models.CardCounts{1: 2, 2: 1}})
	rr := httptest.NewRecorder()
	cardservices.FillDeckForUser(rr, jsonRequest(t, http.MethodPost, "/decks/fill", map[string]interface{}{"uid": 1}))
	if rr.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d: %s", rr.Code, rr.Body.String())
	}
}

func TestFillDeckForUser_ReplacesExisting(t *testing.T) {
	t.Setenv("DECK_SIZE", "3")
	db := setupTestDB(t)
	seedCards(t, db, []models.Card{{Cid: 1}, {Cid: 2}, {Cid: 3}})
	seedInventory(t, db, models.Inventory{Uid: 1, Cards: models.CardCounts{1: 2, 2: 1}})
	db.Create(&models.Deck{Uid: 1, Cards: models.CardCounts{9: 3}})
	rr := httptest.NewRecorder()
	cardservices.FillDeckForUser(rr, jsonRequest(t, http.MethodPost, "/decks/fill", map[string]interface{}{"uid": 1}))
	if rr.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d", rr.Code)
	}
	var deck models.Deck
	if err := json.NewDecoder(rr.Body).Decode(&deck); err != nil {
		t.Fatalf("failed to decode response: %v", err)
	}
	if _, ok := deck.Cards[9]; ok {
		t.Error("expected old deck replaced")
	}
}

func TestFillDeckForUser_NoCardsInDB(t *testing.T) {
	t.Setenv("DECK_SIZE", "3")
	db := setupTestDB(t)
	seedInventory(t, db, models.Inventory{Uid: 1, Cards: models.CardCounts{1: 2}})
	rr := httptest.NewRecorder()
	cardservices.FillDeckForUser(rr, jsonRequest(t, http.MethodPost, "/decks/fill", map[string]interface{}{"uid": 1}))
	if rr.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d", rr.Code)
	}
}

func TestFillDeckForUser_InvalidUID(t *testing.T) {
	setupTestDB(t)
	rr := httptest.NewRecorder()
	cardservices.FillDeckForUser(rr, jsonRequest(t, http.MethodPost, "/decks/fill", map[string]interface{}{"uid": 0}))
	if rr.Code != http.StatusBadRequest {
		t.Errorf("expected 400, got %d", rr.Code)
	}
}

func TestFillDeckForUser_InventoryNotFound(t *testing.T) {
	setupTestDB(t)
	rr := httptest.NewRecorder()
	cardservices.FillDeckForUser(rr, jsonRequest(t, http.MethodPost, "/decks/fill", map[string]interface{}{"uid": 99}))
	if rr.Code != http.StatusNotFound {
		t.Errorf("expected 404, got %d", rr.Code)
	}
}

func TestFillDeckForUser_InvalidBody(t *testing.T) {
	setupTestDB(t)
	rr := httptest.NewRecorder()
	cardservices.FillDeckForUser(rr, jsonRequest(t, http.MethodPost, "/decks/fill", "bad"))
	if rr.Code != http.StatusBadRequest {
		t.Errorf("expected 400, got %d", rr.Code)
	}
}

func TestFillDeckForUser_DBError(t *testing.T) {
	setupBrokenDB(t)
	rr := httptest.NewRecorder()
	cardservices.FillDeckForUser(rr, jsonRequest(t, http.MethodPost, "/decks/fill", map[string]interface{}{"uid": 1}))
	if rr.Code != http.StatusNotFound {
		t.Errorf("expected 404, got %d", rr.Code)
	}
}

func TestFillDeckForUser_CardsQueryFails(t *testing.T) {
	t.Setenv("DECK_SIZE", "3")
	setupSeedThenBreak(t, models.Inventory{Uid: 1, Cards: models.CardCounts{1: 2}})
	rr := httptest.NewRecorder()
	cardservices.FillDeckForUser(rr, jsonRequest(t, http.MethodPost, "/decks/fill", map[string]interface{}{"uid": 1}))
	if rr.Code != http.StatusNotFound && rr.Code != http.StatusInternalServerError {
		t.Errorf("expected 404 or 500, got %d", rr.Code)
	}
}

func TestFillDecksFromInventories_NoInventories(t *testing.T) {
	db := setupTestDB(t)
	if err := cardservices.FillDecksFromInventories(db); err != nil {
		t.Errorf("expected no error, got: %v", err)
	}
}

func TestFillDecksFromInventories_CreatesDeckPerInventory(t *testing.T) {
	t.Setenv("DECK_SIZE", "3")
	db := setupTestDB(t)
	seedCards(t, db, []models.Card{{Cid: 1}, {Cid: 2}, {Cid: 3}})
	seedInventory(t, db, models.Inventory{Uid: 1, Cards: models.CardCounts{1: 2, 2: 1}})
	seedInventory(t, db, models.Inventory{Uid: 2, Cards: models.CardCounts{3: 3}})
	if err := cardservices.FillDecksFromInventories(db); err != nil {
		t.Fatalf("expected no error, got: %v", err)
	}
	var count int64
	db.Model(&models.Deck{}).Count(&count)
	if count != 2 {
		t.Errorf("expected 2, got %d", count)
	}
}

func TestFillDecksFromInventories_DBError(t *testing.T) {
	db, _ := gorm.Open(sqlite.Open(":memory:"), &gorm.Config{Logger: logger.Default.LogMode(logger.Silent)})
	sqlDB, _ := db.DB()
	sqlDB.Close()
	if err := cardservices.FillDecksFromInventories(db); err == nil {
		t.Error("expected error, got nil")
	}
}

func TestFillDecksFromInventories_AllCardsQueryFails(t *testing.T) {
	t.Setenv("DECK_SIZE", "3")
	db, _ := gorm.Open(sqlite.Open(":memory:"), &gorm.Config{Logger: logger.Default.LogMode(logger.Silent)})
	if err := db.AutoMigrate(&models.Card{}, &models.Deck{}, &models.Inventory{}); err != nil {
		t.Fatalf("failed to migrate: %v", err)
	}
	db.Create(&models.Inventory{Uid: 1, Cards: models.CardCounts{1: 2}})
	sqlDB, _ := db.DB()
	sqlDB.Close()
	if err := cardservices.FillDecksFromInventories(db); err == nil {
		t.Error("expected error when allCards query fails")
	}
}

func TestCreateDeck_DeleteExistingError(t *testing.T) {
	t.Setenv("DECK_SIZE", "3")
	db := setupTestDB(t)
	db.Create(&models.Deck{Uid: 1, Cards: models.CardCounts{1: 1, 2: 1, 3: 1}})
	if err := db.Callback().Delete().Before("gorm:delete").Register("force_delete_error", func(tx *gorm.DB) {
		tx.AddError(errors.New("forced delete error"))
	}); err != nil {
		t.Fatalf("failed to register callback: %v", err)
	}
	t.Cleanup(func() {
		_ = db.Callback().Delete().Remove("force_delete_error")
	})

	rr := httptest.NewRecorder()
	cardservices.CreateDeck(rr, jsonRequest(t, http.MethodPost, "/decks", map[string]interface{}{"uid": 1, "cards": map[string]int{"4": 1, "5": 1, "6": 1}}))
	if rr.Code != http.StatusInternalServerError {
		t.Fatalf("expected 500, got %d", rr.Code)
	}
}

func TestFillDeckForUser_DeleteDeckError(t *testing.T) {
	t.Setenv("DECK_SIZE", "3")
	db := setupTestDB(t)
	seedCards(t, db, []models.Card{{Cid: 1}, {Cid: 2}, {Cid: 3}})
	seedInventory(t, db, models.Inventory{Uid: 1, Cards: models.CardCounts{1: 2, 2: 1}})
	if err := db.Callback().Delete().Before("gorm:delete").Register("force_delete_error", func(tx *gorm.DB) {
		tx.AddError(errors.New("forced delete error"))
	}); err != nil {
		t.Fatalf("failed to register callback: %v", err)
	}
	t.Cleanup(func() {
		_ = db.Callback().Delete().Remove("force_delete_error")
	})

	rr := httptest.NewRecorder()
	cardservices.FillDeckForUser(rr, jsonRequest(t, http.MethodPost, "/decks/fill", map[string]interface{}{"uid": 1}))
	if rr.Code != http.StatusInternalServerError {
		t.Fatalf("expected 500, got %d", rr.Code)
	}
}

func TestFillDeckForUser_CreateDeckError(t *testing.T) {
	t.Setenv("DECK_SIZE", "3")
	db := setupTestDB(t)
	seedCards(t, db, []models.Card{{Cid: 1}, {Cid: 2}, {Cid: 3}})
	seedInventory(t, db, models.Inventory{Uid: 1, Cards: models.CardCounts{1: 2, 2: 1}})
	if err := db.Callback().Create().Before("gorm:create").Register("force_create_error", func(tx *gorm.DB) {
		tx.AddError(errors.New("forced create error"))
	}); err != nil {
		t.Fatalf("failed to register callback: %v", err)
	}
	t.Cleanup(func() {
		_ = db.Callback().Create().Remove("force_create_error")
	})

	rr := httptest.NewRecorder()
	cardservices.FillDeckForUser(rr, jsonRequest(t, http.MethodPost, "/decks/fill", map[string]interface{}{"uid": 1}))
	if rr.Code != http.StatusInternalServerError {
		t.Fatalf("expected 500, got %d", rr.Code)
	}
}

func TestFillDeckForUser_EncodeError(t *testing.T) {
	t.Setenv("DECK_SIZE", "3")
	db := setupTestDB(t)
	seedCards(t, db, []models.Card{{Cid: 1}, {Cid: 2}, {Cid: 3}})
	seedInventory(t, db, models.Inventory{Uid: 1, Cards: models.CardCounts{1: 2, 2: 1}})

	fw := &failingResponseWriter{}
	cardservices.FillDeckForUser(fw, jsonRequest(t, http.MethodPost, "/decks/fill", map[string]interface{}{"uid": 1}))
	if fw.status != http.StatusInternalServerError {
		t.Fatalf("expected 500, got %d", fw.status)
	}
}

func TestFillDecksFromInventories_SkipsExistingDeck(t *testing.T) {
	t.Setenv("DECK_SIZE", "3")
	db := setupTestDB(t)
	seedCards(t, db, []models.Card{{Cid: 1}, {Cid: 2}, {Cid: 3}})
	seedInventory(t, db, models.Inventory{Uid: 1, Cards: models.CardCounts{1: 2}})
	db.Create(&models.Deck{Uid: 1, Cards: models.CardCounts{9: 3}})

	if err := cardservices.FillDecksFromInventories(db); err != nil {
		t.Fatalf("expected no error, got: %v", err)
	}

	var deck models.Deck
	if err := db.Where("uid = ?", 1).First(&deck).Error; err != nil {
		t.Fatalf("failed to load deck: %v", err)
	}
	if _, ok := deck.Cards[9]; !ok {
		t.Fatalf("expected existing deck to be kept")
	}
}

func TestFillDecksFromInventories_CreateError(t *testing.T) {
	t.Setenv("DECK_SIZE", "3")
	db := setupTestDB(t)
	seedCards(t, db, []models.Card{{Cid: 1}, {Cid: 2}, {Cid: 3}})
	seedInventory(t, db, models.Inventory{Uid: 1, Cards: models.CardCounts{1: 2}})
	if err := db.Callback().Create().Before("gorm:create").Register("force_create_error", func(tx *gorm.DB) {
		tx.AddError(errors.New("forced create error"))
	}); err != nil {
		t.Fatalf("failed to register callback: %v", err)
	}
	t.Cleanup(func() {
		_ = db.Callback().Create().Remove("force_create_error")
	})

	if err := cardservices.FillDecksFromInventories(db); err == nil {
		t.Fatal("expected error, got nil")
	}
}

func TestGetDeckByUserID_EncodeError(t *testing.T) {
	db := setupTestDB(t)
	db.Create(&models.Deck{Uid: 1, Cards: models.CardCounts{1: 2}})
	fw := &failingResponseWriter{}
	cardservices.GetDeckByUserID(fw, withChiParam(httptest.NewRequest(http.MethodGet, "/decks/1", nil), "uid", "1"))
	if fw.status != http.StatusInternalServerError {
		t.Fatalf("expected 500, got %d", fw.status)
	}
}
