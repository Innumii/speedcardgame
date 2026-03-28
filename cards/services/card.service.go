package services

import (
	"fmt"
	"net/http"

	"github.com/Ryanljk/speedcardgame/cards/config"
	"github.com/Ryanljk/speedcardgame/cards/models"
	"github.com/Ryanljk/speedcardgame/cards/util"
)

type cardRequest struct {
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
	var inputCard cardRequest

	// Decode the incoming request body
	if ok := util.DecodeJSONBody(w, r, &inputCard, "Invalid inputCard"); !ok {
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
		util.RespondWithError(w, http.StatusInternalServerError, fmt.Sprintf("Failed to create card: %v", err))
		return
	}

	// Return the created card
	util.RespondWithJSON(w, http.StatusOK, card)
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
		util.RespondWithError(w, http.StatusInternalServerError, result.Error.Error())
		return
	}

	// Respond with the list of cards
	util.RespondWithJSON(w, http.StatusOK, cards)
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
	var inputCard cardRequest

	// TODO: Add admin authentication check here

	// Decode the incoming request body
	if ok := util.DecodeJSONBody(w, r, &inputCard, "Invalid inputCard"); !ok {
		return
	}

	// Extract the CardID from the URL parameter
	cid := inputCard.Cid

	// Find the existing card
	var card models.Card
	if err := config.DB.First(&card, cid).Error; err != nil {
		util.RespondWithError(w, http.StatusNotFound, fmt.Sprintf("Card not found: %v", err))
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
		util.RespondWithError(w, http.StatusBadRequest, "No valid fields to update")
		return
	}

	// Save the updated card
	if err := config.DB.Save(&card).Error; err != nil {
		util.RespondWithError(w, http.StatusInternalServerError, fmt.Sprintf("Failed to update card: %v", err))
		return
	}

	// Return the updated card
	util.RespondWithJSON(w, http.StatusOK, card)
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
	var inputCard cardRequest
	if ok := util.DecodeJSONBody(w, r, &inputCard, "Invalid inputCard"); !ok {
		return
	}

	// Extract the CardID from the URL parameter
	cid := inputCard.Cid

	// Find the card to delete
	var card models.Card
	if err := config.DB.First(&card, cid).Error; err != nil {
		util.RespondWithError(w, http.StatusNotFound, fmt.Sprintf("Card not found: %v", err))
		return
	}

	// Delete the card
	if err := config.DB.Delete(&card).Error; err != nil {
		util.RespondWithError(w, http.StatusInternalServerError, fmt.Sprintf("Failed to delete card: %v", err))
		return
	}

	util.RespondWithJSON(w, http.StatusOK, card)
}

func GetCardCount(w http.ResponseWriter, _ *http.Request) {
	var count int64
	if err := config.DB.Model(&models.Card{}).Count(&count).Error; err != nil {
		util.RespondWithError(w, http.StatusInternalServerError, fmt.Sprintf("Failed to count cards: %v", err))
		return
	}
	util.RespondWithJSON(w, http.StatusOK, map[string]int64{"count": count})
}
