package repositories

import (
	"github.com/Ryanljk/speedcardgame/auth/models"
	"time"
)

type UserRepository interface {
	FindByEmail(email string) (*models.User, error)
	Create(user *models.User) error
	Update(user *models.User) error
	Delete(userID uint) error
	FindByID(userID uint) (*models.User, error)
	StorePasswordResetToken(userID uint, tokenHash string, expiry time.Time) error
	UpdatePassword(userID uint, passwordHash string) error
	ClearPasswordResetToken(userID uint) error
	ChangePassword(userID uint, newPassword string) error
}
