package models

// Deck represents a user's deck of cards
type Deck struct {
	Uid   int         `json:"uid"`
	Cards map[int]int `json:"cards"` // Map of card ID to quantity
}
