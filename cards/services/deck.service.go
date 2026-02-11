package services

import (
	"encoding/json"
	"fmt"
	"net/http"

	"github.com/Ryanljk/speedcardgame/cards/config"
	"github.com/Ryanljk/speedcardgame/cards/models"
)

var inputDeck struct {
	Uid   int               `json:"uid"`
	Cards models.CardCounts `json:"cards"`
}

// CreateDeck creates a new deck for a user
func CreateDeck(w http.ResponseWriter, r *http.Request) {

	// Decode the JSON body into the inputDeck struct
	if err := json.NewDecoder(r.Body).Decode(&inputDeck); err != nil {
		http.Error(w, "Invalid inputDeck", http.StatusBadRequest)
		return
	}

	// Checks if user already has a deck
	var existingDeck models.Deck
	if err := config.DB.Where("uid = ?", inputDeck.Uid).First(&existingDeck).Error; err == nil {
		// remove existing deck
		DeleteDeck(w, r)
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

	// Preload Cards to include associated cards in the query
	if err := config.DB.Preload("Cards").Find(&decks).Error; err != nil {
		http.Error(w, fmt.Sprintf("Failed to retrieve decks: %v", err), http.StatusInternalServerError)
		return
	}

	// Return the list of decks
	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(decks)
}

func DeleteDeck(w http.ResponseWriter, r *http.Request) {

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
