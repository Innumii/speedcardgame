package models

// Card represents a card in the game
type Card struct {
	Cid       int    `json:"cid"`
	Name      string `json:"name"`
	Type      string `json:"type"`
	Cost      int    `json:"cost"`
	Value     int    `json:"value"`
	Power     int    `json:"power"`
	Toughness int    `json:"toughness"`
	Effect    string `json:"effect"`
}