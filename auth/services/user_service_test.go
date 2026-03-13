package services

import (
	"context"
	"errors"
	"net/http"
	"net/http/httptest"
	"strings"
	"testing"
	"time"

	"github.com/Ryanljk/speedcardgame/auth/dtos"
	"github.com/Ryanljk/speedcardgame/auth/models"
)

// fakeServerHostPort splits "http://127.0.0.1:PORT" into host and port
// so t.Setenv can feed them to ResolveCardsServiceBaseURL.
func fakeServerHostPort(rawURL string) (host, port string) {
	rawURL = strings.TrimPrefix(rawURL, "http://")
	parts := strings.SplitN(rawURL, ":", 2)
	return parts[0], parts[1]
}

// ── Register ───────────────────────────────────────────────────────────────────

func TestRegister_Success(t *testing.T) {
	fakeServer := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		w.WriteHeader(http.StatusCreated)
	}))
	defer fakeServer.Close()

	host, port := fakeServerHostPort(fakeServer.URL)
	t.Setenv("CARDS_SERVICE_HOST", host)
	t.Setenv("CARDS_SERVICE_PORT", port)

	repo := &MockUserRepository{
		FindByEmailFn: func(email string) (*models.User, error) {
			return nil, errors.New("not found")
		},
		CreateFn: func(user *models.User) error {
			user.ID = 1
			return nil
		},
		DeleteFn: func(userID uint) error { return nil },
	}

	svc, _ := newTestServices(t, repo)
	dto := dtos.RegisterDTO{Name: "Alice", Email: "alice@example.com", Password: "secret123"}

	user, err := svc.Register(dto)
	if err != nil {
		t.Fatalf("expected no error, got: %v", err)
	}
	if user.Email != dto.Email {
		t.Errorf("expected email %q, got %q", dto.Email, user.Email)
	}
}

func TestRegister_EmailAlreadyTaken(t *testing.T) {
	repo := &MockUserRepository{
		FindByEmailFn: func(email string) (*models.User, error) {
			return &models.User{Email: email}, nil
		},
	}

	svc, _ := newTestServices(t, repo)
	dto := dtos.RegisterDTO{Name: "Bob", Email: "bob@example.com", Password: "secret123"}

	_, err := svc.Register(dto)
	if err == nil || err.Error() != "email already taken" {
		t.Errorf("expected 'email already taken', got: %v", err)
	}
}

func TestRegister_InventoryFailure_RollsBackUser(t *testing.T) {
	fakeServer := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		w.WriteHeader(http.StatusInternalServerError)
	}))
	defer fakeServer.Close()

	host, port := fakeServerHostPort(fakeServer.URL)
	t.Setenv("CARDS_SERVICE_HOST", host)
	t.Setenv("CARDS_SERVICE_PORT", port)

	deleted := false
	repo := &MockUserRepository{
		FindByEmailFn: func(email string) (*models.User, error) {
			return nil, errors.New("not found")
		},
		CreateFn: func(user *models.User) error {
			user.ID = 99
			return nil
		},
		DeleteFn: func(userID uint) error {
			deleted = true
			return nil
		},
	}

	svc, _ := newTestServices(t, repo)
	dto := dtos.RegisterDTO{Name: "Carol", Email: "carol@example.com", Password: "secret123"}

	_, err := svc.Register(dto)
	if err == nil {
		t.Fatal("expected error from inventory failure, got nil")
	}
	if !deleted {
		t.Error("expected user to be deleted on inventory failure (rollback)")
	}
}

// ── Login ──────────────────────────────────────────────────────────────────────

func TestLogin_Success(t *testing.T) {
	plain := "mypassword"
	hashed := hashedPassword(t, plain)

	repo := &MockUserRepository{
		FindByEmailFn: func(email string) (*models.User, error) {
			return &models.User{ID: 1, Email: email, Password: hashed}, nil
		},
	}

	svc, _ := newTestServices(t, repo)
	dto := dtos.LoginDTO{Email: "alice@example.com", Password: plain}

	user, sessionID, err := svc.Login(context.Background(), dto)
	if err != nil {
		t.Fatalf("expected no error, got: %v", err)
	}
	if user == nil {
		t.Fatal("expected user, got nil")
	}
	if sessionID == "" {
		t.Error("expected a session ID, got empty string")
	}
}

func TestLogin_UserNotFound(t *testing.T) {
	repo := &MockUserRepository{
		FindByEmailFn: func(email string) (*models.User, error) {
			return nil, errors.New("not found")
		},
	}

	svc, _ := newTestServices(t, repo)
	dto := dtos.LoginDTO{Email: "ghost@example.com", Password: "whatever"}

	_, _, err := svc.Login(context.Background(), dto)
	if err == nil || err.Error() != "user not found" {
		t.Errorf("expected 'user not found', got: %v", err)
	}
}

func TestLogin_WrongPassword(t *testing.T) {
	hashed := hashedPassword(t, "correctpassword")

	repo := &MockUserRepository{
		FindByEmailFn: func(email string) (*models.User, error) {
			return &models.User{ID: 2, Email: email, Password: hashed}, nil
		},
	}

	svc, _ := newTestServices(t, repo)
	dto := dtos.LoginDTO{Email: "alice@example.com", Password: "wrongpassword"}

	_, _, err := svc.Login(context.Background(), dto)
	if err == nil || err.Error() != "invalid password" {
		t.Errorf("expected 'invalid password', got: %v", err)
	}
}

// ── Logout ─────────────────────────────────────────────────────────────────────

func TestLogout_Success(t *testing.T) {
	_, redisClient := newTestRedis(t)
	sessionSvc := NewSessionService(redisClient)
	authSvc := NewAuthService(&MockUserRepository{}, sessionSvc)

	ctx := context.Background()
	sessionID, _ := sessionSvc.CreateSession(ctx, "42")

	err := authSvc.Logout(ctx, sessionID)
	if err != nil {
		t.Fatalf("expected no error on logout, got: %v", err)
	}

	_, err = sessionSvc.GetSessionBySessionId(ctx, sessionID)
	if err == nil {
		t.Error("expected session to be deleted after logout")
	}
}

func TestLogout_SessionNotFound(t *testing.T) {
	svc, _ := newTestServices(t, &MockUserRepository{})

	err := svc.Logout(context.Background(), "nonexistent-session-id")
	if err == nil {
		t.Error("expected error for nonexistent session, got nil")
	}
}

// ── GetMe ──────────────────────────────────────────────────────────────────────

func TestGetMe_Success(t *testing.T) {
	_, redisClient := newTestRedis(t)
	sessionSvc := NewSessionService(redisClient)

	repo := &MockUserRepository{
		FindByIDFn: func(userID uint) (*models.User, error) {
			return &models.User{ID: userID, Name: "Alice", Email: "alice@example.com"}, nil
		},
	}
	authSvc := NewAuthService(repo, sessionSvc)

	ctx := context.Background()
	sessionID, _ := sessionSvc.CreateSession(ctx, "1")

	user, err := authSvc.GetMe(ctx, sessionID)
	if err != nil {
		t.Fatalf("expected no error, got: %v", err)
	}
	if user.ID != 1 {
		t.Errorf("expected user ID 1, got %d", user.ID)
	}
}

func TestGetMe_InvalidSession(t *testing.T) {
	svc, _ := newTestServices(t, &MockUserRepository{})

	_, err := svc.GetMe(context.Background(), "bad-session")
	if err == nil {
		t.Error("expected error for invalid session, got nil")
	}
}

// ── RequestPasswordReset ───────────────────────────────────────────────────────

func TestRequestPasswordReset_Success(t *testing.T) {
	repo := &MockUserRepository{
		FindByEmailFn: func(email string) (*models.User, error) {
			return &models.User{ID: 1, Email: email}, nil
		},
		StorePasswordResetTokenFn: func(userID uint, tokenHash string, expiry time.Time) error {
			return nil
		},
	}

	svc, _ := newTestServices(t, repo)
	dto := dtos.RequestPasswordResetDTO{Email: "alice@example.com"}

	token, err := svc.RequestPasswordReset(dto)
	if err != nil {
		t.Fatalf("expected no error, got: %v", err)
	}
	if token == "" {
		t.Error("expected a reset token, got empty string")
	}
}

func TestRequestPasswordReset_UserNotFound(t *testing.T) {
	repo := &MockUserRepository{
		FindByEmailFn: func(email string) (*models.User, error) {
			return nil, errors.New("not found")
		},
	}

	svc, _ := newTestServices(t, repo)
	dto := dtos.RequestPasswordResetDTO{Email: "ghost@example.com"}

	_, err := svc.RequestPasswordReset(dto)
	if err == nil || err.Error() != "user not found" {
		t.Errorf("expected 'user not found', got: %v", err)
	}
}

// ── ConfirmPasswordReset ───────────────────────────────────────────────────────

func TestConfirmPasswordReset_Success(t *testing.T) {
	plainToken := "resettoken123"
	hashedToken := hashedPassword(t, plainToken)

	repo := &MockUserRepository{
		FindByEmailFn: func(email string) (*models.User, error) {
			return &models.User{
				ID:                  1,
				Email:               email,
				ResetPasswordToken:  hashedToken,
				PasswordResetExpiry: time.Now().Add(10 * time.Minute),
			}, nil
		},
		UpdatePasswordFn:          func(userID uint, passwordHash string) error { return nil },
		ClearPasswordResetTokenFn: func(userID uint) error { return nil },
	}

	svc, _ := newTestServices(t, repo)
	dto := dtos.ConfirmPasswordResetDTO{
		Email:       "alice@example.com",
		Token:       plainToken,
		NewPassword: "newpassword123",
	}

	err := svc.ConfirmPasswordReset(dto)
	if err != nil {
		t.Fatalf("expected no error, got: %v", err)
	}
}

func TestConfirmPasswordReset_ExpiredToken(t *testing.T) {
	repo := &MockUserRepository{
		FindByEmailFn: func(email string) (*models.User, error) {
			return &models.User{
				ID:                  1,
				ResetPasswordToken:  "sometoken",
				PasswordResetExpiry: time.Now().Add(-1 * time.Minute),
			}, nil
		},
	}

	svc, _ := newTestServices(t, repo)
	dto := dtos.ConfirmPasswordResetDTO{Email: "alice@example.com", Token: "sometoken", NewPassword: "newpass123"}

	err := svc.ConfirmPasswordReset(dto)
	if err == nil || err.Error() != "password reset token has expired" {
		t.Errorf("expected 'password reset token has expired', got: %v", err)
	}
}

func TestConfirmPasswordReset_InvalidToken(t *testing.T) {
	hashedToken := hashedPassword(t, "correcttoken")

	repo := &MockUserRepository{
		FindByEmailFn: func(email string) (*models.User, error) {
			return &models.User{
				ID:                  1,
				ResetPasswordToken:  hashedToken,
				PasswordResetExpiry: time.Now().Add(10 * time.Minute),
			}, nil
		},
	}

	svc, _ := newTestServices(t, repo)
	dto := dtos.ConfirmPasswordResetDTO{Email: "alice@example.com", Token: "wrongtoken", NewPassword: "newpass123"}

	err := svc.ConfirmPasswordReset(dto)
	if err == nil || err.Error() != "invalid password reset token" {
		t.Errorf("expected 'invalid password reset token', got: %v", err)
	}
}

// ── ChangePassword ─────────────────────────────────────────────────────────────

func TestChangePassword_Success(t *testing.T) {
	oldPlain := "oldpassword"
	oldHashed := hashedPassword(t, oldPlain)

	_, redisClient := newTestRedis(t)
	sessionSvc := NewSessionService(redisClient)
	repo := &MockUserRepository{
		FindByIDFn: func(userID uint) (*models.User, error) {
			return &models.User{ID: userID, Password: oldHashed}, nil
		},
		ChangePasswordFn: func(userID uint, newPassword string) error { return nil },
	}
	authSvc := NewAuthService(repo, sessionSvc)

	ctx := context.Background()
	sessionID, _ := sessionSvc.CreateSession(ctx, "1")

	dto := dtos.ChangePasswordDTO{OldPassword: oldPlain, NewPassword: "newpassword123"}
	err := authSvc.ChangePassword(ctx, sessionID, dto)
	if err != nil {
		t.Fatalf("expected no error, got: %v", err)
	}
}

func TestChangePassword_WrongOldPassword(t *testing.T) {
	oldHashed := hashedPassword(t, "correctold")

	_, redisClient := newTestRedis(t)
	sessionSvc := NewSessionService(redisClient)
	repo := &MockUserRepository{
		FindByIDFn: func(userID uint) (*models.User, error) {
			return &models.User{ID: userID, Password: oldHashed}, nil
		},
	}
	authSvc := NewAuthService(repo, sessionSvc)

	ctx := context.Background()
	sessionID, _ := sessionSvc.CreateSession(ctx, "1")

	dto := dtos.ChangePasswordDTO{OldPassword: "wrongold", NewPassword: "newpassword123"}
	err := authSvc.ChangePassword(ctx, sessionID, dto)
	if err == nil || err.Error() != "initial password does not match" {
		t.Errorf("expected 'initial password does not match', got: %v", err)
	}
}

func TestChangePassword_SessionNotFound(t *testing.T) {
	svc, _ := newTestServices(t, &MockUserRepository{})

	dto := dtos.ChangePasswordDTO{OldPassword: "old", NewPassword: "new"}
	err := svc.ChangePassword(context.Background(), "bad-session", dto)
	if err == nil || err.Error() != "session not found" {
		t.Errorf("expected 'session not found', got: %v", err)
	}
}
