package services

import (
	"encoding/json"
	"fmt"
	"net/http"

	"github.com/Ryanljk/speedcardgame/cards/config"
	"github.com/Ryanljk/speedcardgame/cards/models"
)

var inputCard struct {
	Uid       int    `json:"uid"`
	Cid       int    `json:"cid"`
	Name      string `json:"name"`
	Type      string `json:"type"`
	Cost      int    `json:"cost"`
	Value     int    `json:"value"`
	Power     int    `json:"power"`
	Toughness int    `json:"toughness"`
	Effect    string `json:"effect"`
}


// CreateCard godoc
// @Summary      Create a card
// @Description  Creates a new card. Admin use only.
// @Tags         cards
// @Accept       json
// @Produce      json
// @Param        body  body      object  true  "Card details"
// @Success      200   {object}  models.Card
// @Failure      400   {string}  string  "Invalid input"
// @Failure      500   {string}  string  "Failed to create card"

// create a new card, ONLY FOR ADMIN USE
func CreateCard(w http.ResponseWriter, r *http.Request) {

	// Decode the incoming request body
	if err := json.NewDecoder(r.Body).Decode(&inputCard); err != nil {
		http.Error(w, "Invalid inputCard", http.StatusBadRequest)
		return
	}

	// TODO: Add admin authentication check here

	// Create a new Card instance
	card := models.Card{
		Cid:       inputCard.Cid,
		Name:      inputCard.Name,
		Type:      inputCard.Type,
		Cost:      inputCard.Cost,
		Value:     inputCard.Value,
		Power:     inputCard.Power,
		Toughness: inputCard.Toughness,
		Effect:    inputCard.Effect,
	}

	// Save the card to the database
	if err := config.DB.Create(&card).Error; err != nil {
		http.Error(w, fmt.Sprintf("Failed to create card: %v", err), http.StatusInternalServerError)
		return
	}

	// Return the created card
	w.Header().Set("Content-Type", "application/json")
	if err := json.NewEncoder(w).Encode(card); err != nil {
		http.Error(w, fmt.Sprintf("Failed to encode card response: %v", err), http.StatusInternalServerError)
		return
	}
}


// ListCards godoc
// @Summary      List all cards
// @Description  Returns all cards ordered by card ID
// @Tags         cards
// @Produce      json
// @Success      200  {array}   models.Card
// @Failure      500  {string}  string  "Failed to retrieve cards"
// @Router       /cards [get]

// List cards within the page range by Cid order
func ListCards(w http.ResponseWriter, _ *http.Request) {
	var cards []models.Card

	// Query for all cards ordered by Cid so the client can paginate locally
	if result := config.DB.Order("cid").Find(&cards); result.Error != nil {
		http.Error(w, result.Error.Error(), http.StatusInternalServerError)
		return
	}

	// Respond with the list of decks
	w.Header().Set("Content-Type", "application/json")
	if err := json.NewEncoder(w).Encode(cards); err != nil {
		http.Error(w, fmt.Sprintf("Failed to encode cards response: %v", err), http.StatusInternalServerError)
		return
	}
}


// UpdateCard godoc
// @Summary      Update a card
// @Description  Updates one or more fields of an existing card. Admin use only.
// @Tags         cards
// @Accept       json
// @Produce      json
// @Param        body  body      object  true  "Fields to update (include cid)"
// @Success      200   {object}  models.Card
// @Failure      400   {string}  string  "Invalid input or no valid fields"
// @Failure      404   {string}  string  "Card not found"
// @Failure      500   {string}  string  "Failed to update card"
// @Router       /cards [put]

// Update Card details, ONLY FOR ADMIN USE
func UpdateCard(w http.ResponseWriter, r *http.Request) {

	// TODO: Add admin authentication check here

	// Decode the incoming request body
	if err := json.NewDecoder(r.Body).Decode(&inputCard); err != nil {
		http.Error(w, "Invalid inputCard", http.StatusBadRequest)
		return
	}

	// Extract the CardID from the URL parameter
	cid := inputCard.Cid

	// Find the existing card
	var card models.Card
	if err := config.DB.First(&card, cid).Error; err != nil {
		http.Error(w, fmt.Sprintf("Card not found: %v", err), http.StatusNotFound)
		return
	}

	// Apply updates, if the field is empty, keep the existing value
	if inputCard.Name != "" {
		card.Name = inputCard.Name
	} else if inputCard.Type != "" {
		card.Type = inputCard.Type
	} else if inputCard.Cost > -1 {
		card.Cost = inputCard.Cost
	} else if inputCard.Value > -1 {
		card.Value = inputCard.Value
	} else if inputCard.Power > -1 {
		card.Power = inputCard.Power
	} else if inputCard.Toughness > -1 {
		card.Toughness = inputCard.Toughness
	} else if inputCard.Effect != "" {
		card.Effect = inputCard.Effect
	} else {
		http.Error(w, "No valid fields to update", http.StatusBadRequest)
		return
	}

	// Save the updated card
	if err := config.DB.Save(&card).Error; err != nil {
		http.Error(w, fmt.Sprintf("Failed to update card: %v", err), http.StatusInternalServerError)
		return
	}

	// Return the updated card
	w.Header().Set("Content-Type", "application/json")
	if err := json.NewEncoder(w).Encode(card); err != nil {
		http.Error(w, fmt.Sprintf("Failed to encode card response: %v", err), http.StatusInternalServerError)
		return
	}
}



// DeleteCard godoc
// @Summary      Delete a card
// @Description  Deletes a card by its card ID. Admin use only.
// @Tags         cards
// @Accept       json
// @Produce      json
// @Param        body  body      object  true  "Card ID"
// @Success      200   {object}  models.Card
// @Failure      400   {string}  string  "Invalid input"
// @Failure      404   {string}  string  "Card not found"
// @Failure      500   {string}  string  "Failed to delete card"
// @Router       /cards [delete]

func DeleteCard(w http.ResponseWriter, r *http.Request) {
	if err := json.NewDecoder(r.Body).Decode(&inputCard); err != nil {
		http.Error(w, "Invalid inputCard", http.StatusBadRequest)
		return
	}

	// Extract the CardID from the URL parameter
	cid := inputCard.Cid

	// Find the card to delete
	var card models.Card
	if err := config.DB.First(&card, cid).Error; err != nil {
		http.Error(w, fmt.Sprintf("Card not found: %v", err), http.StatusNotFound)
		return
	}

	// Delete the card
	if err := config.DB.Delete(&card).Error; err != nil {
		http.Error(w, fmt.Sprintf("Failed to delete card: %v", err), http.StatusInternalServerError)
		return
	}

	w.Header().Set("Content-Type", "application/json")
	if err := json.NewEncoder(w).Encode(card); err != nil {
		http.Error(w, fmt.Sprintf("Failed to encode card response: %v", err), http.StatusInternalServerError)
		return
	}
}
