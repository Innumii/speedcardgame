package models

// Deck represents a user's deck of cards
type Deck struct {
	Uid   int        `json:"uid" gorm:"primaryKey;autoIncrement:false"`
	Cards CardCounts `json:"cards" gorm:"type:jsonb"` // Map of card ID to quantity
}
