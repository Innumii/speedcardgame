package repositories

import (
	"log"
	"time"

	"github.com/FYL-Studios/speedcardgame/auth/models"
	"gorm.io/gorm"
)

type GormUserRepository struct {
	db *gorm.DB
}

func NewGormUserRepository(db *gorm.DB) *GormUserRepository {
	return &GormUserRepository{db: db}
}

func (r *GormUserRepository) FindByEmail(email string) (*models.User, error) {
	var user models.User
	if err := r.db.Where("email = ?", email).First(&user).Error; err != nil {
		return nil, err // return nil if user not found
	}
	return &user, nil
}

func (r *GormUserRepository) FindByID(userID uint) (*models.User, error) {
	var user models.User
	if err := r.db.Where("id = ?", userID).First(&user).Error; err != nil {
		return nil, err // return nil if user not found
	}
	return &user, nil
}

func (r *GormUserRepository) Create(user *models.User) error {
	return r.db.Create(user).Error
}

func (r *GormUserRepository) Update(user *models.User) error {
	return r.db.Save(user).Error
}

func (r *GormUserRepository) Delete(userID uint) error {
	return r.db.Delete(&models.User{}, userID).Error
}

func (r *GormUserRepository) StorePasswordResetToken(userID uint, tokenHash string, expiry time.Time) error {
	log.Println("Storing password reset token", tokenHash, "for user", userID)
	return r.db.Model(&models.User{}).Where("id = ?", userID).Updates(map[string]interface{}{
		"password_reset_token":        tokenHash,
		"password_reset_token_expiry": expiry,
	}).Error
}

func (r *GormUserRepository) UpdatePassword(userID uint, passwordHash string) error {
	return r.db.Model(&models.User{}).Where("id = ?", userID).Update("password", passwordHash).Error
}

func (r *GormUserRepository) ClearPasswordResetToken(userID uint) error {
	return r.db.Model(&models.User{}).Where("id = ?", userID).Updates(map[string]interface{}{
		"password_reset_token":        nil,
		"password_reset_token_expiry": nil,
	}).Error
}

func (r *GormUserRepository) ChangePassword(userID uint, newPassword string) error {
	return r.db.Model(&models.User{}).Where("id = ?", userID).Update("password", newPassword).Error
}
