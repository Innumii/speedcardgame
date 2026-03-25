package services

import (
	"fmt"
	"net/http"

	"github.com/Ryanljk/speedcardgame/cards/config"
	"github.com/Ryanljk/speedcardgame/cards/models"
	"github.com/Ryanljk/speedcardgame/cards/util"
	"github.com/go-chi/chi"
)

type inventoryRequest struct {
	Uid   int               `json:"uid"`
	Coins int               `json:"coins"`
	Cards models.CardCounts `json:"cards"` // Map of card ID to quantity
}

const maxCardCopies = 4

// ClampCardCount constrains an inventory quantity to the allowed range.
func ClampCardCount(value int) int {
	if value < 0 {
		return 0
	}
	if value > maxCardCopies {
		return maxCardCopies
	}
	return value
}

// NormalizeInventoryCards removes invalid entries and clamps valid card quantities.
func NormalizeInventoryCards(cards models.CardCounts) bool {
	if cards == nil {
		return false
	}

	changed := false
	for cid, qty := range cards {
		if cid <= 0 {
			delete(cards, cid)
			changed = true
			continue
		}

		clamped := ClampCardCount(qty)
		if clamped <= 0 {
			if qty != 0 {
				changed = true
			}
			delete(cards, cid)
			continue
		}

		if clamped != qty {
			cards[cid] = clamped
			changed = true
		}
	}

	return changed
}

// CreateInventory creates a new inventory for new user
func CreateInventory(w http.ResponseWriter, r *http.Request) {
	var inputInventory inventoryRequest

	// Decode the JSON body into the inputInventory struct
	if ok := util.DecodeJSONBody(w, r, &inputInventory, "Invalid inputInventory"); !ok {
		return
	}
	// Create a new Inventory directly in this handler
	inventory := models.Inventory{
		Uid:   inputInventory.Uid,
		Coins: inputInventory.Coins,
		Cards: inputInventory.Cards,
	}
	// Add in some default cards for new users
	if inventory.Cards == nil {
		IntroCards := make(models.CardCounts)

		// Example: Give 2 of each intro card with IDs 1 to 5
		IntroCards[1] = 4
		IntroCards[2] = 4
		IntroCards[3] = 4
		IntroCards[4] = 4
		IntroCards[5] = 4
		IntroCards[6] = 4
		IntroCards[7] = 4
		IntroCards[8] = 4

		inventory.Cards = IntroCards

		inventory.Coins = 1000
	}
	NormalizeInventoryCards(inventory.Cards)
	// Insert into the database
	if err := config.DB.Create(&inventory).Error; err != nil {
		util.RespondWithError(w, http.StatusInternalServerError, fmt.Sprintf("Failed to create inventory: %v", err))
		return
	}
	// Return the created inventory as JSON
	util.RespondWithJSON(w, http.StatusOK, inventory)
}

// ListInventories lists all inventories
func ListInventories(w http.ResponseWriter, _ *http.Request) {
	var inventories []models.Inventory

	// Query all inventories (Cards field is automatically loaded as JSONB)
	if err := config.DB.Find(&inventories).Error; err != nil {
		util.RespondWithError(w, http.StatusInternalServerError, fmt.Sprintf("Failed to retrieve inventories: %v", err))
		return
	}

	for i := range inventories {
		if NormalizeInventoryCards(inventories[i].Cards) {
			_ = config.DB.Save(&inventories[i]).Error
		}
	}

	// Return the list of inventories
	util.RespondWithJSON(w, http.StatusOK, inventories)
}

// GetInventory retrieves a specific inventory by user ID
func GetInventoryByUserID(w http.ResponseWriter, r *http.Request) {
	uidParam := chi.URLParam(r, "uid")
	if uidParam == "" {
		util.RespondWithError(w, http.StatusBadRequest, "Missing uid")
		return
	}
	var inventory models.Inventory
	if err := config.DB.Where("uid = ?", uidParam).First(&inventory).Error; err != nil {
		util.RespondWithError(w, http.StatusInternalServerError, fmt.Sprintf("Failed to retrieve inventory: %v", err))
		return
	}

	if NormalizeInventoryCards(inventory.Cards) {
		if err := config.DB.Save(&inventory).Error; err != nil {
			util.RespondWithError(w, http.StatusInternalServerError, fmt.Sprintf("Failed to normalize inventory: %v", err))
			return
		}
	}

	// Return the inventory as JSON
	util.RespondWithJSON(w, http.StatusOK, inventory)
}

func UpdateInventory(w http.ResponseWriter, r *http.Request) {
	var inputInventory inventoryRequest

	// Decode the incoming request body
	if ok := util.DecodeJSONBody(w, r, &inputInventory, "Invalid inputDeck"); !ok {
		return
	}

	// Implementation for adding a card to the inventory
	uid := inputInventory.Uid
	cards := inputInventory.Cards

	var inventory models.Inventory
	if err := config.DB.Where("uid = ?", uid).First(&inventory).Error; err != nil {
		util.RespondWithError(w, http.StatusInternalServerError, fmt.Sprintf("Failed to retrieve inventory: %v", err))
		return
	}

	// Update the inventory with new cards
	if inventory.Cards == nil {
		inventory.Cards = make(models.CardCounts)
	}
	for cid, qty := range cards {
		if cid <= 0 || qty == 0 {
			continue
		}
		nextQty := inventory.Cards[cid] + qty
		clamped := ClampCardCount(nextQty)
		if clamped <= 0 {
			delete(inventory.Cards, cid)
			continue
		}
		inventory.Cards[cid] = clamped
	}

	NormalizeInventoryCards(inventory.Cards)

	// Save the updated inventory
	if err := config.DB.Save(&inventory).Error; err != nil {
		util.RespondWithError(w, http.StatusInternalServerError, fmt.Sprintf("Failed to update inventory: %v", err))
		return
	}

	// Return the updated inventory
	util.RespondWithJSON(w, http.StatusOK, inventory)
}

func UpdateInventoryCoins(w http.ResponseWriter, r *http.Request) {
	var inputInventory inventoryRequest

	// Decode the incoming request body
	if ok := util.DecodeJSONBody(w, r, &inputInventory, "Invalid inputDeck"); !ok {
		return
	}

	uid := inputInventory.Uid
	coins := inputInventory.Coins
	var inventory models.Inventory
	if err := config.DB.Where("uid = ?", uid).First(&inventory).Error; err != nil {
		util.RespondWithError(w, http.StatusInternalServerError, fmt.Sprintf("Failed to retrieve inventory: %v", err))
		return
	}
	inventory.Coins = coins

	// Save the updated inventory
	if err := config.DB.Save(&inventory).Error; err != nil {
		util.RespondWithError(w, http.StatusInternalServerError, fmt.Sprintf("Failed to update inventory: %v", err))
		return
	}

	util.RespondWithJSON(w, http.StatusOK, inventory)
}

func AddInventoryCoins(w http.ResponseWriter, r *http.Request) {
	var inputInventory inventoryRequest

	// Decode the incoming request body
	if ok := util.DecodeJSONBody(w, r, &inputInventory, "Invalid inputDeck"); !ok {
		return
	}

	uid := inputInventory.Uid
	coins := inputInventory.Coins
	var inventory models.Inventory
	if err := config.DB.Where("uid = ?", uid).First(&inventory).Error; err != nil {
		util.RespondWithError(w, http.StatusInternalServerError, fmt.Sprintf("Failed to retrieve inventory: %v", err))
		return
	}
	inventory.Coins += coins

	// Save the updated inventory
	if err := config.DB.Save(&inventory).Error; err != nil {
		util.RespondWithError(w, http.StatusInternalServerError, fmt.Sprintf("Failed to update inventory: %v", err))
		return
	}

	util.RespondWithJSON(w, http.StatusOK, inventory)
}
