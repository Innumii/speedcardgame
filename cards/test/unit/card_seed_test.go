package services_test

import (
	"os"
	"testing"

	"github.com/FYL-Studios/speedcardgame/cards/models"
	cardservices "github.com/FYL-Studios/speedcardgame/cards/services"
	"gorm.io/driver/sqlite"
	"gorm.io/gorm"
	"gorm.io/gorm/logger"
)

func writeTempCSV(t *testing.T, name, content string) string {
	t.Helper()
	f := t.TempDir() + "/" + name
	if err := os.WriteFile(f, []byte(content), 0644); err != nil {
		t.Fatalf("failed to write temp csv: %v", err)
	}
	return f
}

func TestSeedCardsFromCSV_Success(t *testing.T) {
	db := setupTestDB(t)
	f := writeTempCSV(t, "cards.csv", "cid,name,type,cost,value,power,toughness,effect\n1,Fire Bolt,Spell,2,3,4,1,Deal 3\n2,Shield,Defense,1,2,0,5,Block\n")
	if err := cardservices.SeedCardsFromCSV(db, f); err != nil {
		t.Fatalf("expected no error, got: %v", err)
	}
	var count int64
	db.Model(&models.Card{}).Count(&count)
	if count != 2 {
		t.Errorf("expected 2, got %d", count)
	}
}

func TestSeedCardsFromCSV_UpdatesExisting(t *testing.T) {
	db := setupTestDB(t)
	seedCards(t, db, []models.Card{{Cid: 1, Name: "Old", Type: "Spell"}})
	f := writeTempCSV(t, "update.csv", "cid,name,type,cost,value,power,toughness,effect\n1,New Name,Creature,3,2,5,2,Updated\n")
	if err := cardservices.SeedCardsFromCSV(db, f); err != nil {
		t.Fatalf("expected no error, got: %v", err)
	}
	var card models.Card
	db.First(&card, 1)
	if card.Name != "New Name" {
		t.Errorf("expected 'New Name', got %q", card.Name)
	}
}

func TestSeedCardsFromCSV_FileNotFound(t *testing.T) {
	db := setupTestDB(t)
	if err := cardservices.SeedCardsFromCSV(db, "/nonexistent/cards.csv"); err == nil {
		t.Error("expected error, got nil")
	}
}

func TestSeedCardsFromCSV_EmptyFile(t *testing.T) {
	db := setupTestDB(t)
	f := writeTempCSV(t, "empty.csv", "cid,name,type,cost,value,power,toughness,effect\n")
	if err := cardservices.SeedCardsFromCSV(db, f); err != nil {
		t.Fatalf("expected no error, got: %v", err)
	}
}

func TestSeedCardsFromCSV_MissingColumn(t *testing.T) {
	db := setupTestDB(t)
	f := writeTempCSV(t, "bad.csv", "cid,name,type,cost,value,power,toughness\n1,Card,Spell,1,1,1,1\n")
	if err := cardservices.SeedCardsFromCSV(db, f); err == nil {
		t.Error("expected error for missing column")
	}
}

func TestSeedCardsFromCSV_InvalidInt(t *testing.T) {
	db := setupTestDB(t)
	f := writeTempCSV(t, "badint.csv", "cid,name,type,cost,value,power,toughness,effect\nNOT,Card,Spell,1,1,1,1,none\n")
	if err := cardservices.SeedCardsFromCSV(db, f); err == nil {
		t.Error("expected error for invalid int")
	}
}

func TestSeedCardsFromCSV_MissingName(t *testing.T) {
	db := setupTestDB(t)
	f := writeTempCSV(t, "noname.csv", "cid,name,type,cost,value,power,toughness,effect\n1,,Spell,1,1,1,1,none\n")
	if err := cardservices.SeedCardsFromCSV(db, f); err == nil {
		t.Error("expected error for missing name")
	}
}

func TestSeedCardsFromCSV_EmptyIntField(t *testing.T) {
	db := setupTestDB(t)
	f := writeTempCSV(t, "emptycost.csv", "cid,name,type,cost,value,power,toughness,effect\n1,Card,Spell,,1,1,1,none\n")
	if err := cardservices.SeedCardsFromCSV(db, f); err != nil {
		t.Fatalf("expected no error for empty int field, got: %v", err)
	}
}

func TestSeedCardsFromCSV_LookupError(t *testing.T) {
	db, _ := gorm.Open(sqlite.Open(":memory:"), &gorm.Config{Logger: logger.Default.LogMode(logger.Silent)})
	if err := db.AutoMigrate(&models.Card{}); err != nil {
		t.Fatalf("failed to migrate: %v", err)
	}
	sqlDB, _ := db.DB()
	sqlDB.Close()
	f := writeTempCSV(t, "lookup.csv", "cid,name,type,cost,value,power,toughness,effect\n1,Card,Spell,1,1,1,1,none\n")
	if err := cardservices.SeedCardsFromCSV(db, f); err == nil {
		t.Error("expected error from closed DB")
	}
}

func TestGetCSVField_OutOfBounds(t *testing.T) {
	if got := cardservices.GetCSVField([]string{"a", "b"}, 5); got != "" {
		t.Errorf("expected empty, got %q", got)
	}
}

func TestGetCSVField_NegativeIndex(t *testing.T) {
	if got := cardservices.GetCSVField([]string{"a", "b"}, -1); got != "" {
		t.Errorf("expected empty, got %q", got)
	}
}

func TestGetCSVField_ValidIndex(t *testing.T) {
	if got := cardservices.GetCSVField([]string{"hello", "world"}, 1); got != "world" {
		t.Errorf("expected 'world', got %q", got)
	}
}
