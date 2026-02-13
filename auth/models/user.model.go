package models

import "time"

type User struct {
	ID                  uint   `gorm:"primaryKey"`
	Name                string `gorm:"not null"`
	Email               string `gorm:"unique;not null"`
	Password            string `gorm:"not null"`
	CreatedAt           time.Time
	UpdatedAt           time.Time
	Role                string    `gorm:"default:User"`
	ResetPasswordToken  string    `gorm:"default:null"`
	PasswordResetExpiry time.Time `gorm:"default:null"`
}

/**
Additional Attributes:
- passwordResetToken (for password reset)

Activity:
- SearchHistory
- recentlyViewedCards (up to 10, for recommendations)
- favouriteCards
- dislikedCards
*/
