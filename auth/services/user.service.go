package services

import (
	"context"
	"errors"
	"fmt"
	"log"
	"strconv"
	"time"

	"github.com/Ryanljk/speedcardgame/auth/dtos"
	"github.com/Ryanljk/speedcardgame/auth/models"
	"github.com/Ryanljk/speedcardgame/auth/repositories"
	"github.com/Ryanljk/speedcardgame/auth/utils"
)

type AuthService struct {
	UserRepository repositories.UserRepository
	SessionService *SessionService
}

func NewAuthService(userRepository repositories.UserRepository, sessionService *SessionService) *AuthService {
	return &AuthService{UserRepository: userRepository, SessionService: sessionService}
}

// Register a new user
func (service *AuthService) Register(registerDTO dtos.RegisterDTO) (*models.User, error) {
	// Check if user already exists
	_, err := service.UserRepository.FindByEmail(registerDTO.Email)
	if err == nil { // If nill, user exists
		return nil, errors.New("email already taken")
	}

	// Hash the password
	hashedPassword, err := utils.Hash(registerDTO.Password)
	if err != nil {
		return nil, err
	}

	// Create new user
	user := models.User{
		Name:     registerDTO.Name,
		Email:    registerDTO.Email,
		Password: hashedPassword,
	}

	err = service.UserRepository.Create(&user)
	if err != nil {
		return nil, err
	}

	return &user, nil
}

// Authenticate user with username and password
func (service *AuthService) Login(ctx context.Context, loginDTO dtos.LoginDTO) (*models.User, string, error) {
	var user *models.User

	// Find the user by username
	user, err := service.UserRepository.FindByEmail(loginDTO.Email)
	if err != nil {
		return nil, "", errors.New("user not found")
	}

	// Check if the password is correct
	err = utils.CheckHash(user.Password, loginDTO.Password)
	if err != nil {
		return nil, "", errors.New("invalid password")
	}

	userIDStr := fmt.Sprintf("%d", user.ID)

	// Create session using SessionService
	sessionID, err := service.SessionService.CreateSession(ctx, userIDStr)
	if err != nil {
		return nil, "", err
	}

	return user, sessionID, nil
}

func (service *AuthService) Logout(ctx context.Context, sessionID string) error {
	// Check if an existing session exists in Redis
	existingUserID, err := service.SessionService.GetSessionBySessionId(ctx, sessionID)
	if err != nil {
		fmt.Println("Session not found:", err)
		return err
	}

	// Session exists, delete the session
	fmt.Println("Existing session found for user:", existingUserID, ", deleting...")

	// Delete the session from Redis
	err = service.SessionService.DeleteSession(ctx, sessionID)
	if err != nil {
		return err
	}

	fmt.Println("Session deleted successfully")
	return nil
}

// Get logged in user info, or return error
func (service *AuthService) GetMe(ctx context.Context, sessionID string) (*models.User, error) {
	userID, err := service.SessionService.GetSessionBySessionId(ctx, sessionID)

	if err != nil {
		fmt.Println("Session not found:", err)
		return nil, err
	}

	userIDInt, err := strconv.ParseUint(userID, 10, 32) // parse to uint
	if err != nil {
		return nil, err
	}

	var user *models.User
	user, err = service.UserRepository.FindByID(uint(userIDInt))
	if err != nil {
		return nil, err
	}

	return user, nil
}

// RequestPasswordReset generates a token, stores it, and initiates email sending
func (service *AuthService) RequestPasswordReset(requestPasswordResetDTO dtos.RequestPasswordResetDTO) (string, error) {
	user, err := service.UserRepository.FindByEmail(requestPasswordResetDTO.Email)
	if err != nil {
		return "", errors.New("user not found")
	}

	// generate token
	token, err := utils.GenerateSecureToken()
	if err != nil {
		return "", err
	}

	log.Println("Generated unhashed token:", token)

	hashedToken, err := utils.Hash(token)
	if err != nil {
		return "", err
	}

	log.Println("hashed token:", hashedToken)

	expiry := time.Now().Add(15 * time.Minute)
	err = service.UserRepository.StorePasswordResetToken(user.ID, hashedToken, expiry)
	if err != nil {
		return "", err
	}

	// TODO: Return the plain unhashed token to be sent via email (or notification)
	return token, nil
}

// verify token and update database
// TODO: might need to refactor this
func (service *AuthService) ConfirmPasswordReset(resetPasswordDTO dtos.ConfirmPasswordResetDTO) error {
	user, err := service.UserRepository.FindByEmail(resetPasswordDTO.Email)
	if err != nil {
		return errors.New("invalid email")
	}

	// check if token is valid
	if user.PasswordResetExpiry.Before(time.Now()) {
		return errors.New("password reset token has expired")
	}

	err = utils.CheckHash(user.ResetPasswordToken, resetPasswordDTO.Token)
	if err != nil {
		return errors.New("invalid password reset token")
	}

	// Valid token, update password
	hashedPassword, err := utils.Hash(resetPasswordDTO.NewPassword)
	if err != nil {
		return err
	}

	err = service.UserRepository.UpdatePassword(user.ID, hashedPassword)
	if err != nil {
		return err
	}

	err = service.UserRepository.ClearPasswordResetToken(user.ID)
	if err != nil {
		return err
	}

	return nil
}

func (service *AuthService) ChangePassword(ctx context.Context, sessionID string, changePasswordDTO dtos.ChangePasswordDTO) error {
	userID, err := service.SessionService.GetSessionBySessionId(ctx, sessionID)
	if err != nil {
		return errors.New("session not found")
	}

	userIDInt, err := strconv.ParseUint(userID, 10, 32) // parse to uint
	if err != nil {
		return err
	}

	user, err := service.UserRepository.FindByID(uint(userIDInt))
	if err != nil {
		return err
	}

	err = utils.CheckHash(user.Password, changePasswordDTO.OldPassword)
	if err != nil {
		return errors.New("initial password does not match")
	}

	hashedPassword, err := utils.Hash(changePasswordDTO.NewPassword)
	if err != nil {
		return err
	}

	return service.UserRepository.ChangePassword(user.ID, hashedPassword)
}

// POST auth/refresh
// POST auth/password/reset/request
// POST auth/password/reset/confirm
// GET auth/me
// GET auth/verify
// POST auth/role/update ( dev only )
