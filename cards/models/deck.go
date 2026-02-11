package models

// Deck represents a user's deck of cards
type Deck struct {
	Uid   int        `json:"uid"`
	Cards CardCounts `json:"cards" gorm:"type:jsonb"` // Map of card ID to quantity
}
