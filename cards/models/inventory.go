package models

// Inventory represents a user's card inventory
type Inventory struct {
	Uid   int        `json:"uid" gorm:"primaryKey"`
	Coins int        `json:"coins"` // User's coin balance
	Cards CardCounts `json:"cards" gorm:"type:jsonb"` // Map of card ID to quantity
}
