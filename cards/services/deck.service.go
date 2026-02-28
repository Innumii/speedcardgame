package services

import (
	"encoding/json"
	"fmt"
	"net/http"
	"os"
	"sort"
	"strconv"

	"github.com/go-chi/chi"
	"github.com/Ryanljk/speedcardgame/cards/config"
	"github.com/Ryanljk/speedcardgame/cards/models"
	"github.com/Ryanljk/speedcardgame/cards/util"
	"gorm.io/gorm"
)

const defaultDeckSizeLimit = 30

func getDeckSizeLimit() int {
	value := os.Getenv("DECK_SIZE")
	if value == "" {
		return defaultDeckSizeLimit
	}
	parsed, err := strconv.Atoi(value)
	if err != nil || parsed <= 0 {
		return defaultDeckSizeLimit
	}
	return parsed
}

func countDeckCards(cards models.CardCounts) int {
	total := 0
	for _, count := range cards {
		total += count
	}
	return total
}

// CreateDeck creates a new deck for a user
func CreateDeck(w http.ResponseWriter, r *http.Request) {
	var inputDeck struct {
		Uid   int               `json:"uid"`
		Cards models.CardCounts `json:"cards"`
	}

	// Decode the JSON body into the inputDeck struct
	if err := json.NewDecoder(r.Body).Decode(&inputDeck); err != nil {
		http.Error(w, "Invalid inputDeck", http.StatusBadRequest)
		return
	}

	if inputDeck.Uid <= 0 {
		http.Error(w, "uid must be a positive integer", http.StatusBadRequest)
		return
	}

	deckSizeLimit := getDeckSizeLimit()
	if countDeckCards(inputDeck.Cards) != deckSizeLimit {
		http.Error(w, fmt.Sprintf("deck must contain exactly %d cards", deckSizeLimit), http.StatusBadRequest)
		return
	}

	// Checks if user already has a deck
	var existingDeck models.Deck
	if err := config.DB.Where("uid = ?", inputDeck.Uid).First(&existingDeck).Error; err == nil {
		if err := config.DB.Where("uid = ?", inputDeck.Uid).Delete(&models.Deck{}).Error; err != nil {
			http.Error(w, fmt.Sprintf("Failed to delete deck: %v", err), http.StatusInternalServerError)
			return
		}
	}

	// Create a new Deck directly in this handler
	deck := models.Deck{
		Uid:   inputDeck.Uid,
		Cards: inputDeck.Cards,
	}

	// Insert into the database
	if err := config.DB.Create(&deck).Error; err != nil {
		http.Error(w, fmt.Sprintf("Failed to create deck: %v", err), http.StatusInternalServerError)
		return
	}

	// Return the created deck as JSON
	w.Header().Set("Content-Type", "application/json")
	if err := json.NewEncoder(w).Encode(deck); err != nil {
		http.Error(w, fmt.Sprintf("Failed to encode deck response: %v", err), http.StatusInternalServerError)
		return
	}
}

func ListDecks(w http.ResponseWriter, _ *http.Request) {
	var decks []models.Deck

	// Query all decks (Cards field is automatically loaded as JSONB)
	if err := config.DB.Find(&decks).Error; err != nil {
		http.Error(w, fmt.Sprintf("Failed to retrieve decks: %v", err), http.StatusInternalServerError)
		return
	}

	// Return the list of decks
	w.Header().Set("Content-Type", "application/json")
	if err := json.NewEncoder(w).Encode(decks); err != nil {
		http.Error(w, fmt.Sprintf("Failed to encode decks response: %v", err), http.StatusInternalServerError)
		return
	}
}

func DeleteDeck(w http.ResponseWriter, r *http.Request) {
	var inputDeck struct {
		Uid   int               `json:"uid"`
		Cards models.CardCounts `json:"cards"`
	}

	// Decode the JSON body into the inputDeck struct
	if err := json.NewDecoder(r.Body).Decode(&inputDeck); err != nil {
		http.Error(w, "Invalid inputDeck", http.StatusBadRequest)
		return
	}

	// Delete the deck from the database
	if err := config.DB.Where("uid = ?", inputDeck.Uid).Delete(&models.Deck{}).Error; err != nil {
		http.Error(w, fmt.Sprintf("Failed to delete deck: %v", err), http.StatusInternalServerError)
		return
	}

	// Return a success message
	w.Header().Set("Content-Type", "application/json")
	if err := json.NewEncoder(w).Encode(map[string]string{"message": "Deck deleted successfully"}); err != nil {
		http.Error(w, fmt.Sprintf("Failed to encode delete response: %v", err), http.StatusInternalServerError)
		return
	}
}

func FillDeckForUser(w http.ResponseWriter, r *http.Request) {
	var input struct {
		Uid int `json:"uid"`
	}

	if err := json.NewDecoder(r.Body).Decode(&input); err != nil {
		http.Error(w, "Invalid request", http.StatusBadRequest)
		return
	}

	if input.Uid <= 0 {
		http.Error(w, "uid must be a positive integer", http.StatusBadRequest)
		return
	}

	var inventory models.Inventory
	if err := config.DB.Where("uid = ?", input.Uid).First(&inventory).Error; err != nil {
		http.Error(w, fmt.Sprintf("Failed to retrieve inventory: %v", err), http.StatusNotFound)
		return
	}

	var allCards []models.Card
	if err := config.DB.Order("cid").Find(&allCards).Error; err != nil {
		http.Error(w, fmt.Sprintf("Failed to retrieve cards: %v", err), http.StatusInternalServerError)
		return
	}

	deckCounts := buildDeckCounts(inventory.Cards, allCards, getDeckSizeLimit())
	deck := models.Deck{
		Uid:   input.Uid,
		Cards: deckCounts,
	}

	if err := config.DB.Where("uid = ?", input.Uid).Delete(&models.Deck{}).Error; err != nil {
		http.Error(w, fmt.Sprintf("Failed to delete deck: %v", err), http.StatusInternalServerError)
		return
	}

	if err := config.DB.Create(&deck).Error; err != nil {
		http.Error(w, fmt.Sprintf("Failed to create deck: %v", err), http.StatusInternalServerError)
		return
	}

	w.Header().Set("Content-Type", "application/json")
	if err := json.NewEncoder(w).Encode(deck); err != nil {
		http.Error(w, fmt.Sprintf("Failed to encode deck response: %v", err), http.StatusInternalServerError)
		return
	}
}

func FillDecksFromInventories(db *gorm.DB) error {
	var inventories []models.Inventory
	if err := db.Find(&inventories).Error; err != nil {
		return err
	}

	if len(inventories) == 0 {
		return nil
	}

	deckSizeLimit := getDeckSizeLimit()

	var allCards []models.Card
	if err := db.Order("cid").Find(&allCards).Error; err != nil {
		return err
	}

	for _, inventory := range inventories {
		deckCounts := buildDeckCounts(inventory.Cards, allCards, deckSizeLimit)
		deck := models.Deck{
			Uid:   inventory.Uid,
			Cards: deckCounts,
		}

		if err := db.Where("uid = ?", inventory.Uid).Delete(&models.Deck{}).Error; err != nil {
			return err
		}
		if err := db.Create(&deck).Error; err != nil {
			return err
		}
	}

	return nil
}

func buildDeckCounts(inventory models.CardCounts, allCards []models.Card, limit int) models.CardCounts {
	deck := make(models.CardCounts)
	remaining := limit

	if inventory != nil {
		cids := make([]int, 0, len(inventory))
		for cid, count := range inventory {
			if count > 0 {
				cids = append(cids, cid)
			}
		}
		sort.Ints(cids)

		for _, cid := range cids {
			if remaining == 0 {
				break
			}
			count := inventory[cid]
			if count > remaining {
				count = remaining
			}
			deck[cid] += count
			remaining -= count
		}
	}

	for _, card := range allCards {
		if remaining == 0 {
			break
		}
		deck[card.Cid]++
		remaining--
	}

	return deck
}

// GetDeckByUserID handles GET /decks/{uid}
func GetDeckByUserID(w http.ResponseWriter, r *http.Request) {
    uidStr := chi.URLParam(r, "uid")
    uid, err := strconv.Atoi(uidStr)
    if err != nil {
        util.RespondWithError(w, http.StatusBadRequest, "Invalid user ID format")
        return
    }

    var deck models.Deck
    if err := config.DB.Where("uid = ?", uid).First(&deck).Error; err != nil {
        util.RespondWithError(w, http.StatusNotFound, "Deck not found")
        return
    }

    w.Header().Set("Content-Type", "application/json")
    json.NewEncoder(w).Encode(deck)
}