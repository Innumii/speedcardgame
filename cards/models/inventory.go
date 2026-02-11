package models

// Inventory represents a user's card inventory
type inventory struct {
	Uid       int    `json:"uid"`
	Cards    map[int]int `json:"cards"` // Map of card ID to quantity
}