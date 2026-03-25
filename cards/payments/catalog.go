package payments

import "sort"

// CoinPackage defines a fixed, server-owned coin bundle.
type CoinPackage struct {
	ID          string `json:"id"`
	Name        string `json:"name"`
	Coins       int    `json:"coins"`
	AmountCents int64  `json:"amount_cents"`
	Currency    string `json:"currency"`
	DiscountPercent int    `json:"discount_percent,omitempty"`
}

var coinPackages = map[string]CoinPackage{
	"coin_pack_small": {
		ID:          "coin_pack_small",
		Name:        "Small Coin Pack",
		Coins:       1000,
		AmountCents: 499,
		Currency:    "sgd",
		DiscountPercent: 0,
	},
	"coin_pack_medium": {
		ID:          "coin_pack_medium",
		Name:        "Medium Coin Pack",
		Coins:       2500,
		AmountCents: 999,
		Currency:    "sgd",
		DiscountPercent: 0,
	},
	"coin_pack_large": {
		ID:          "coin_pack_large",
		Name:        "Large Coin Pack",
		Coins:       7000,
		AmountCents: 1999,
		Currency:    "sgd",
		DiscountPercent: 0,
	},
}

func GetCoinPackage(id string) (CoinPackage, bool) {
	pkg, ok := coinPackages[id]
	return pkg, ok
}

func ListCoinPackages() []CoinPackage {
	ids := make([]string, 0, len(coinPackages))
	for id := range coinPackages {
		ids = append(ids, id)
	}
	sort.Strings(ids)

	packs := make([]CoinPackage, 0, len(ids))
	for _, id := range ids {
		packs = append(packs, coinPackages[id])
	}

	return packs
}
