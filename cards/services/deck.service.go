package services

import (
	"encoding/json"
	"fmt"
	"net/http"
	"os"
	"strconv"

	"github.com/Ryanljk/speedcardgame/cards/config"
	"github.com/Ryanljk/speedcardgame/cards/models"
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
	json.NewEncoder(w).Encode(deck)
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
	json.NewEncoder(w).Encode(decks)
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
	json.NewEncoder(w).Encode(map[string]string{"message": "Deck deleted successfully"})
}
