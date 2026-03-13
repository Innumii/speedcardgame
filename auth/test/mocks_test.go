package services_test

import (
	"time"

	"github.com/Ryanljk/speedcardgame/auth/models"
)

// MockUserRepository implements repositories.UserRepository for testing
type MockUserRepository struct {
	FindByEmailFn             func(email string) (*models.User, error)
	CreateFn                  func(user *models.User) error
	UpdateFn                  func(user *models.User) error
	DeleteFn                  func(userID uint) error
	FindByIDFn                func(userID uint) (*models.User, error)
	StorePasswordResetTokenFn func(userID uint, tokenHash string, expiry time.Time) error
	UpdatePasswordFn          func(userID uint, passwordHash string) error
	ClearPasswordResetTokenFn func(userID uint) error
	ChangePasswordFn          func(userID uint, newPassword string) error
}

func (m *MockUserRepository) FindByEmail(email string) (*models.User, error) {
	return m.FindByEmailFn(email)
}
func (m *MockUserRepository) Create(user *models.User) error {
	return m.CreateFn(user)
}
func (m *MockUserRepository) Update(user *models.User) error {
	return m.UpdateFn(user)
}
func (m *MockUserRepository) Delete(userID uint) error {
	return m.DeleteFn(userID)
}
func (m *MockUserRepository) FindByID(userID uint) (*models.User, error) {
	return m.FindByIDFn(userID)
}
func (m *MockUserRepository) StorePasswordResetToken(userID uint, tokenHash string, expiry time.Time) error {
	return m.StorePasswordResetTokenFn(userID, tokenHash, expiry)
}
func (m *MockUserRepository) UpdatePassword(userID uint, passwordHash string) error {
	return m.UpdatePasswordFn(userID, passwordHash)
}
func (m *MockUserRepository) ClearPasswordResetToken(userID uint) error {
	return m.ClearPasswordResetTokenFn(userID)
}
func (m *MockUserRepository) ChangePassword(userID uint, newPassword string) error {
	return m.ChangePasswordFn(userID, newPassword)
}
