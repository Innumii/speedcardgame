package models

import "time"

// PaymentLedger records processed Stripe events to keep webhook handling idempotent.
type PaymentLedger struct {
	ID          uint      `json:"id" gorm:"primaryKey"`
	EventID     string    `json:"event_id" gorm:"uniqueIndex;not null"`
	SessionID   string    `json:"session_id" gorm:"index"`
	Uid         int       `json:"uid"`
	Coins       int       `json:"coins"`
	AmountCents int64     `json:"amount_cents"`
	Currency    string    `json:"currency"`
	CreatedAt   time.Time `json:"created_at"`
}
