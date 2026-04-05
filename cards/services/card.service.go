package services

import (
	"fmt"
	"net/http"

	"github.com/FYL-Studios/speedcardgame/cards/config"
	"github.com/FYL-Studios/speedcardgame/cards/models"
	"github.com/FYL-Studios/speedcardgame/cards/util"
)

type cardRequest struct {
	Uid       int    `json:"uid"`
	Cid       int    `json:"cid"`
	Name      string `json:"name"`
	Type      string `json:"type"`
	Cost      *int   `json:"cost"`      // pointer so we can distinguish "not provided" from 0
	Value     *int   `json:"value"`     // pointer so we can distinguish "not provided" from 0
	Power     *int   `json:"power"`     // pointer so we can distinguish "not provided" from 0
	Toughness *int   `json:"toughness"` // pointer so we can distinguish "not provided" from 0
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
// @Router       /cards [post]

// create a new card, ONLY FOR ADMIN USE
func CreateCard(w http.ResponseWriter, r *http.Request) {
	var inputCard cardRequest

	// Decode the incoming request body
	if ok := util.DecodeJSONBody(w, r, &inputCard, "Invalid inputCard"); !ok {
		return
	}

	// TODO: Add admin authentication check here

	// Default numeric fields to 0 if not provided
	cost, value, power, toughness := 0, 0, 0, 0
	if inputCard.Cost != nil {
		cost = *inputCard.Cost
	}
	if inputCard.Value != nil {
		value = *inputCard.Value
	}
	if inputCard.Power != nil {
		power = *inputCard.Power
	}
	if inputCard.Toughness != nil {
		toughness = *inputCard.Toughness
	}

	// Create a new Card instance
	card := models.Card{
		Cid:       inputCard.Cid,
		Name:      inputCard.Name,
		Type:      inputCard.Type,
		Cost:      cost,
		Value:     value,
		Power:     power,
		Toughness: toughness,
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

	// Extract the CardID from the request body
	cid := inputCard.Cid

	// Find the existing card
	var card models.Card
	if err := config.DB.First(&card, cid).Error; err != nil {
		util.RespondWithError(w, http.StatusNotFound, fmt.Sprintf("Card not found: %v", err))
		return
	}

	// Apply updates independently — each field is checked separately so multiple
	// fields can be updated in a single request
	updated := false

	if inputCard.Name != "" {
		card.Name = inputCard.Name
		updated = true
	}
	if inputCard.Type != "" {
		card.Type = inputCard.Type
		updated = true
	}
	if inputCard.Cost != nil {
		card.Cost = *inputCard.Cost
		updated = true
	}
	if inputCard.Value != nil {
		card.Value = *inputCard.Value
		updated = true
	}
	if inputCard.Power != nil {
		card.Power = *inputCard.Power
		updated = true
	}
	if inputCard.Toughness != nil {
		card.Toughness = *inputCard.Toughness
		updated = true
	}
	if inputCard.Effect != "" {
		card.Effect = inputCard.Effect
		updated = true
	}

	if !updated {
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

	// Extract the CardID from the request body
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
