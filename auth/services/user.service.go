package services

import (
	"bytes"
	"context"
	"crypto/tls"
	"crypto/x509"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"log"
	"net/http"
	"os"
	"strconv"
	"strings"
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

type inventoryCreateRequest struct {
	Uid uint `json:"uid"`
}

func NewAuthService(userRepository repositories.UserRepository, sessionService *SessionService) *AuthService {
	return &AuthService{UserRepository: userRepository, SessionService: sessionService}
}

// Register a new user
func (service *AuthService) Register(registerDTO dtos.RegisterDTO) (*models.User, error) {
	return service.registerUser(registerDTO, true)
}

func (service *AuthService) RegisterDevUser(registerDTO dtos.RegisterDTO) (*models.User, error) {
	return service.registerUser(registerDTO, true)
}

func (service *AuthService) registerUser(registerDTO dtos.RegisterDTO, createInventory bool) (*models.User, error) {
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

	if createInventory {
		if err := service.createStarterInventory(user.ID); err != nil {
			deleteErr := service.UserRepository.Delete(user.ID)
			if deleteErr != nil {
				return nil, fmt.Errorf("inventory creation failed: %v; rollback failed: %w", err, deleteErr)
			}
			return nil, fmt.Errorf("inventory creation failed: %w", err)
		}
	}

	return &user, nil
}

func buildCardsTLSConfig() (*tls.Config, error) {
	verifyTLS := utils.GetEnvAsBool("TLS_VERIFY_CERTS", true)
	serverName := strings.TrimSpace(os.Getenv("CARDS_TLS_SERVER_NAME"))
	caCertPath := strings.TrimSpace(os.Getenv("CARDS_CA_CERT_PATH"))

	tlsConfig := &tls.Config{
		MinVersion:         tls.VersionTLS12,
		InsecureSkipVerify: !verifyTLS, //nolint:gosec // Explicit opt-out via TLS_VERIFY_CERTS for isolated local test environments only.
	}

	if !verifyTLS {
		return tlsConfig, nil
	}

	rootCAs, err := x509.SystemCertPool()
	if err != nil {
		return nil, fmt.Errorf("load system cert pool: %w", err)
	}
	if rootCAs == nil {
		rootCAs = x509.NewCertPool()
	}

	if caCertPath != "" {
		caPem, err := os.ReadFile(caCertPath)
		if err != nil {
			return nil, fmt.Errorf("read CARDS_CA_CERT_PATH %q: %w", caCertPath, err)
		}
		if ok := rootCAs.AppendCertsFromPEM(caPem); !ok {
			return nil, fmt.Errorf("CARDS_CA_CERT_PATH %q does not contain a valid PEM certificate", caCertPath)
		}
	}

	tlsConfig.RootCAs = rootCAs
	if serverName != "" {
		tlsConfig.ServerName = serverName
	}

	return tlsConfig, nil
}

func (service *AuthService) createStarterInventory(userID uint) error {
	baseURLs := utils.ResolveCardsServiceBaseURLs()
	if len(baseURLs) == 0 {
		baseURLs = []string{utils.ResolveCardsServiceBaseURL()}
	}

	payload, err := json.Marshal(inventoryCreateRequest{Uid: userID})
	if err != nil {
		return err
	}

	tlsConfig, err := buildCardsTLSConfig()
	if err != nil {
		return err
	}

	client := &http.Client{
		Timeout: 5 * time.Second,
		Transport: &http.Transport{
			TLSClientConfig: tlsConfig,
		},
	}

	inventoryPaths := []string{"/cards/inventories", "/cardbase/inventories"}
	deckPaths := []string{"/cards/decks/fill", "/cardbase/decks/fill"}

	var lastErr error
	for _, baseURL := range baseURLs {
		if err := service.postWithFallback(client, baseURL, inventoryPaths, payload, "cards service"); err != nil {
			lastErr = err
			continue
		}

		if err := service.postWithFallback(client, baseURL, deckPaths, payload, "deck fill"); err != nil {
			lastErr = err
			continue
		}

		return nil
	}

	if lastErr != nil {
		return lastErr
	}

	return fmt.Errorf("inventory creation failed: no cards service base URL candidates available")
}

func (service *AuthService) postWithFallback(client *http.Client, baseURL string, paths []string, payload []byte, operation string) error {
	var operationErr error
	for _, path := range paths {
		url := fmt.Sprintf("%s%s", baseURL, path)
		req, reqErr := http.NewRequest(http.MethodPost, url, bytes.NewReader(payload))
		if reqErr != nil {
			return reqErr
		}
		req.Header.Set("Content-Type", "application/json")

		resp, doErr := client.Do(req)
		if doErr != nil {
			return doErr
		}

		body, _ := io.ReadAll(resp.Body)
		resp.Body.Close()

		if resp.StatusCode == http.StatusOK || resp.StatusCode == http.StatusCreated {
			return nil
		}

		operationErr = fmt.Errorf("%s returned %d: %s", operation, resp.StatusCode, string(body))
		if resp.StatusCode != http.StatusNotFound {
			break
		}
	}

	return operationErr
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
