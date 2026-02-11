package services

import ("net/http"
		"encoding/json"
		"fmt"
        
		"github.com/go-chi/chi"
        "github.com/Ryanljk/speedcardgame/cards/config"
        "github.com/Ryanljk/speedcardgame/cards/models"

		)
var inputInventory struct {
	Uid       int    `json:"uid"`
	Cards    map[int]int `json:"cards"` // Map of card ID to quantity
}

// CreateInventory creates a new inventory for a user
func CreateInventory(w http.ResponseWriter, r *http.Request) {

	// Decode the JSON body into the inputInventory struct
	if err := json.NewDecoder(r.Body).Decode(&inputInventory); err != nil {
		http.Error(w, "Invalid inputInventory", http.StatusBadRequest)
		return
	}
	// Create a new Inventory directly in this handler
	inventory := models.Inventory{
		Uid:   inputInventory.Uid,
		Cards: inputInventory.Cards,
	}
	// Add in some default cards for new users
	if inventory.Cards == nil {
		IntroCards := make(map[int]int)

		// Example: Give 2 of each intro card with IDs 1 to 5
		IntroCards[1] = 2
		IntroCards[2] = 2
		IntroCards[3] = 2
		IntroCards[4] = 2
		IntroCards[5] = 2

		inventory.Cards = IntroCards
	}
	// Insert into the database
	if err := config.DB.Create(&inventory).Error; err != nil {
		http.Error(w, fmt.Sprintf("Failed to create inventory: %v", err), http.StatusInternalServerError)
		return
	}
	// Return the created inventory as JSON
	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(inventory)
}

// ListInventories lists all inventories
func ListInventories(w http.ResponseWriter, _ *http.Request) {
	var inventories []models.Inventory

	// Preload Cards to include associated cards in the query
	if err := config.DB.Preload("Cards").Find(&inventories).Error; err != nil {
		http.Error(w, fmt.Sprintf("Failed to retrieve inventories: %v", err), http.StatusInternalServerError)
		return
	}

	// Return the list of inventories
	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(inventories)
}

// GetInventory retrieves a specific inventory by user ID
func GetInventory(w http.ResponseWriter, r *http.Request) {
	uidParam := chi.URLParam(r, "uid")
	var inventory models.Inventory
	if err := config.DB.Where("uid = ?", uidParam).First(&inventory).Error; err != nil {
		http.Error(w, fmt.Sprintf("Failed to retrieve inventory: %v", err), http.StatusInternalServerError)
		return
	}

	// Return the inventory as JSON
	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(inventory)
}

func addCardToInventory(w http.ResponseWriter, r *http.Request) {
	
    // Decode the incoming request body
    if err := json.NewDecoder(r.Body).Decode(&inputInventory); err != nil {
        http.Error(w, "Invalid inputDeck", http.StatusBadRequest)
        return
    }

	// Implementation for adding a card to the inventory
	uid := inputInventory.Uid
	cards := inputInventory.Cards

	var inventory models.Inventory
	if err := config.DB.Where("uid = ?", uid).First(&inventory).Error; err != nil {
		http.Error(w, fmt.Sprintf("Failed to retrieve inventory: %v", err), http.StatusInternalServerError)
		return
	}

	// Update the inventory with new cards
	for cid, qty := range cards {
		inventory.Cards[cid] += qty
	}

	// Save the updated inventory
	if err := config.DB.Save(&inventory).Error; err != nil {
		http.Error(w, fmt.Sprintf("Failed to update inventory: %v", err), http.StatusInternalServerError)
		return
	}

	// Return the updated inventory
	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(inventory)
}