package services_test

import (
	"context"
	"errors"
	"testing"
	"time"

	"github.com/FYL-Studios/speedcardgame/auth/dtos"
	"github.com/FYL-Studios/speedcardgame/auth/models"
	"github.com/FYL-Studios/speedcardgame/auth/services"
	"github.com/alicebob/miniredis/v2"
	"github.com/redis/go-redis/v9"
)

// ── Register ───────────────────────────────────────────────────────────────────

func TestRegister_Success(t *testing.T) {
	fakeServer, host, port := newFakeInventoryServer(t, true)
	defer fakeServer.Close()
	t.Setenv("CARDS_SERVICE_HOST", host)
	t.Setenv("CARDS_SERVICE_PORT", port)

	repo := &MockUserRepository{
		FindByEmailFn: func(email string) (*models.User, error) { return nil, errors.New("not found") },
		CreateFn:      func(user *models.User) error { user.ID = 1; return nil },
		DeleteFn:      func(userID uint) error { return nil },
	}
	svc, _ := newTestServices(t, repo)
	user, err := svc.Register(dtos.RegisterDTO{Name: "Alice", Email: "alice@example.com", Password: "secret123"})
	if err != nil {
		t.Fatalf("expected no error, got: %v", err)
	}
	if user.Email != "alice@example.com" {
		t.Errorf("expected email 'alice@example.com', got %q", user.Email)
	}
}

func TestRegister_EmailAlreadyTaken(t *testing.T) {
	repo := &MockUserRepository{
		FindByEmailFn: func(email string) (*models.User, error) { return &models.User{Email: email}, nil },
	}
	svc, _ := newTestServices(t, repo)
	_, err := svc.Register(dtos.RegisterDTO{Name: "Bob", Email: "bob@example.com", Password: "secret123"})
	if err == nil || err.Error() != "email already taken" {
		t.Errorf("expected 'email already taken', got: %v", err)
	}
}

func TestRegister_InventoryFailure_RollsBackUser(t *testing.T) {
	fakeServer, host, port := newFakeInventoryServer(t, false)
	defer fakeServer.Close()
	t.Setenv("CARDS_SERVICE_HOST", host)
	t.Setenv("CARDS_SERVICE_PORT", port)

	deleted := false
	repo := &MockUserRepository{
		FindByEmailFn: func(email string) (*models.User, error) { return nil, errors.New("not found") },
		CreateFn:      func(user *models.User) error { user.ID = 99; return nil },
		DeleteFn:      func(userID uint) error { deleted = true; return nil },
	}
	svc, _ := newTestServices(t, repo)
	_, err := svc.Register(dtos.RegisterDTO{Name: "Carol", Email: "carol@example.com", Password: "secret123"})
	if err == nil {
		t.Fatal("expected error from inventory failure, got nil")
	}
	if !deleted {
		t.Error("expected user to be deleted on inventory failure (rollback)")
	}
}

func TestRegister_InventoryFailure_RollbackAlsoFails(t *testing.T) {
	fakeServer, host, port := newFakeInventoryServer(t, false)
	defer fakeServer.Close()
	t.Setenv("CARDS_SERVICE_HOST", host)
	t.Setenv("CARDS_SERVICE_PORT", port)

	repo := &MockUserRepository{
		FindByEmailFn: func(email string) (*models.User, error) { return nil, errors.New("not found") },
		CreateFn:      func(user *models.User) error { user.ID = 99; return nil },
		DeleteFn:      func(userID uint) error { return errors.New("delete failed") },
	}
	svc, _ := newTestServices(t, repo)
	_, err := svc.Register(dtos.RegisterDTO{Name: "Eve", Email: "eve@example.com", Password: "secret123"})
	if err == nil {
		t.Fatal("expected error when both inventory and rollback fail")
	}
}

func TestRegisterDevUser_Success(t *testing.T) {
	fakeServer, host, port := newFakeInventoryServer(t, true)
	defer fakeServer.Close()
	t.Setenv("CARDS_SERVICE_HOST", host)
	t.Setenv("CARDS_SERVICE_PORT", port)

	repo := &MockUserRepository{
		FindByEmailFn: func(email string) (*models.User, error) { return nil, errors.New("not found") },
		CreateFn:      func(user *models.User) error { user.ID = 1; return nil },
		DeleteFn:      func(userID uint) error { return nil },
	}
	svc, _ := newTestServices(t, repo)
	user, err := svc.RegisterDevUser(dtos.RegisterDTO{Name: "Dev", Email: "dev@example.com", Password: "secret123"})
	if err != nil {
		t.Fatalf("expected no error, got: %v", err)
	}
	if user.Email != "dev@example.com" {
		t.Errorf("expected email 'dev@example.com', got %q", user.Email)
	}
}

// ── Login ──────────────────────────────────────────────────────────────────────

func TestLogin_Success(t *testing.T) {
	hashed := hashedPassword(t, "mypassword")
	repo := &MockUserRepository{
		FindByEmailFn: func(email string) (*models.User, error) {
			return &models.User{ID: 1, Email: email, Password: hashed}, nil
		},
	}
	svc, _ := newTestServices(t, repo)
	user, sessionID, err := svc.Login(context.Background(), dtos.LoginDTO{Email: "alice@example.com", Password: "mypassword"})
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
		FindByEmailFn: func(email string) (*models.User, error) { return nil, errors.New("not found") },
	}
	svc, _ := newTestServices(t, repo)
	_, _, err := svc.Login(context.Background(), dtos.LoginDTO{Email: "ghost@example.com", Password: "whatever"})
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
	_, _, err := svc.Login(context.Background(), dtos.LoginDTO{Email: "alice@example.com", Password: "wrongpassword"})
	if err == nil || err.Error() != "invalid password" {
		t.Errorf("expected 'invalid password', got: %v", err)
	}
}

func TestLogin_SessionCreationFails(t *testing.T) {
	hashed := hashedPassword(t, "mypassword")
	mr, _ := miniredis.Run()
	client := redis.NewClient(&redis.Options{Addr: mr.Addr()})
	mr.Close()

	sessionSvc := services.NewSessionService(client)
	repo := &MockUserRepository{
		FindByEmailFn: func(email string) (*models.User, error) {
			return &models.User{ID: 1, Email: email, Password: hashed}, nil
		},
	}
	authSvc := services.NewAuthService(repo, sessionSvc)
	_, _, err := authSvc.Login(context.Background(), dtos.LoginDTO{Email: "alice@example.com", Password: "mypassword"})
	if err == nil {
		t.Error("expected error when session creation fails, got nil")
	}
}

// ── Logout ─────────────────────────────────────────────────────────────────────

func TestLogout_Success(t *testing.T) {
	_, redisClient := newTestRedis(t)
	sessionSvc := services.NewSessionService(redisClient)
	authSvc := services.NewAuthService(&MockUserRepository{}, sessionSvc)

	ctx := context.Background()
	sessionID, _ := sessionSvc.CreateSession(ctx, "42")
	if err := authSvc.Logout(ctx, sessionID); err != nil {
		t.Fatalf("expected no error on logout, got: %v", err)
	}
	if _, err := sessionSvc.GetSessionBySessionId(ctx, sessionID); err == nil {
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

func TestLogout_DeleteSessionFails(t *testing.T) {
	mr, redisClient := newTestRedis(t)
	sessionSvc := services.NewSessionService(redisClient)
	authSvc := services.NewAuthService(&MockUserRepository{}, sessionSvc)

	ctx := context.Background()
	sessionID, _ := sessionSvc.CreateSession(ctx, "42")
	mr.Close()

	err := authSvc.Logout(ctx, sessionID)
	if err == nil {
		t.Error("expected error when DeleteSession fails, got nil")
	}
}

// ── GetMe ──────────────────────────────────────────────────────────────────────

func TestGetMe_Success(t *testing.T) {
	_, redisClient := newTestRedis(t)
	sessionSvc := services.NewSessionService(redisClient)
	repo := &MockUserRepository{
		FindByIDFn: func(userID uint) (*models.User, error) {
			return &models.User{ID: userID, Name: "Alice", Email: "alice@example.com"}, nil
		},
	}
	authSvc := services.NewAuthService(repo, sessionSvc)
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

func TestGetMe_InvalidUserIDInRedis(t *testing.T) {
	_, redisClient := newTestRedis(t)
	sessionSvc := services.NewSessionService(redisClient)
	authSvc := services.NewAuthService(&MockUserRepository{}, sessionSvc)

	ctx := context.Background()
	redisClient.Set(ctx, "session:bad-uid-session", "not-a-number", 0)
	_, err := authSvc.GetMe(ctx, "bad-uid-session")
	if err == nil {
		t.Error("expected error for non-numeric userID in redis, got nil")
	}
}

func TestGetMe_FindByIDFails(t *testing.T) {
	_, redisClient := newTestRedis(t)
	sessionSvc := services.NewSessionService(redisClient)
	repo := &MockUserRepository{
		FindByIDFn: func(userID uint) (*models.User, error) { return nil, errors.New("db error") },
	}
	authSvc := services.NewAuthService(repo, sessionSvc)
	ctx := context.Background()
	sessionID, _ := sessionSvc.CreateSession(ctx, "1")
	_, err := authSvc.GetMe(ctx, sessionID)
	if err == nil {
		t.Error("expected error when FindByID fails, got nil")
	}
}

// ── RequestPasswordReset ───────────────────────────────────────────────────────

func TestRequestPasswordReset_Success(t *testing.T) {
	repo := &MockUserRepository{
		FindByEmailFn:             func(email string) (*models.User, error) { return &models.User{ID: 1, Email: email}, nil },
		StorePasswordResetTokenFn: func(userID uint, tokenHash string, expiry time.Time) error { return nil },
	}
	svc, _ := newTestServices(t, repo)
	token, err := svc.RequestPasswordReset(dtos.RequestPasswordResetDTO{Email: "alice@example.com"})
	if err != nil {
		t.Fatalf("expected no error, got: %v", err)
	}
	if token == "" {
		t.Error("expected a reset token, got empty string")
	}
}

func TestRequestPasswordReset_UserNotFound(t *testing.T) {
	repo := &MockUserRepository{
		FindByEmailFn: func(email string) (*models.User, error) { return nil, errors.New("not found") },
	}
	svc, _ := newTestServices(t, repo)
	_, err := svc.RequestPasswordReset(dtos.RequestPasswordResetDTO{Email: "ghost@example.com"})
	if err == nil || err.Error() != "user not found" {
		t.Errorf("expected 'user not found', got: %v", err)
	}
}

func TestRequestPasswordReset_StoreFails(t *testing.T) {
	repo := &MockUserRepository{
		FindByEmailFn: func(email string) (*models.User, error) { return &models.User{ID: 1, Email: email}, nil },
		StorePasswordResetTokenFn: func(userID uint, tokenHash string, expiry time.Time) error {
			return errors.New("db error")
		},
	}
	svc, _ := newTestServices(t, repo)
	_, err := svc.RequestPasswordReset(dtos.RequestPasswordResetDTO{Email: "alice@example.com"})
	if err == nil {
		t.Error("expected error when StorePasswordResetToken fails, got nil")
	}
}

// ── ConfirmPasswordReset ───────────────────────────────────────────────────────

func TestConfirmPasswordReset_Success(t *testing.T) {
	plainToken := "resettoken123"
	hashedToken := hashedPassword(t, plainToken)
	repo := &MockUserRepository{
		FindByEmailFn: func(email string) (*models.User, error) {
			return &models.User{
				ID: 1, Email: email,
				ResetPasswordToken:  hashedToken,
				PasswordResetExpiry: time.Now().Add(10 * time.Minute),
			}, nil
		},
		UpdatePasswordFn:          func(userID uint, passwordHash string) error { return nil },
		ClearPasswordResetTokenFn: func(userID uint) error { return nil },
	}
	svc, _ := newTestServices(t, repo)
	err := svc.ConfirmPasswordReset(dtos.ConfirmPasswordResetDTO{
		Email: "alice@example.com", Token: plainToken, NewPassword: "newpassword123",
	})
	if err != nil {
		t.Fatalf("expected no error, got: %v", err)
	}
}

func TestConfirmPasswordReset_ExpiredToken(t *testing.T) {
	repo := &MockUserRepository{
		FindByEmailFn: func(email string) (*models.User, error) {
			return &models.User{
				ID: 1, ResetPasswordToken: "sometoken",
				PasswordResetExpiry: time.Now().Add(-1 * time.Minute),
			}, nil
		},
	}
	svc, _ := newTestServices(t, repo)
	err := svc.ConfirmPasswordReset(dtos.ConfirmPasswordResetDTO{
		Email: "alice@example.com", Token: "sometoken", NewPassword: "newpass123",
	})
	if err == nil || err.Error() != "password reset token has expired" {
		t.Errorf("expected 'password reset token has expired', got: %v", err)
	}
}

func TestConfirmPasswordReset_InvalidToken(t *testing.T) {
	hashedToken := hashedPassword(t, "correcttoken")
	repo := &MockUserRepository{
		FindByEmailFn: func(email string) (*models.User, error) {
			return &models.User{
				ID: 1, ResetPasswordToken: hashedToken,
				PasswordResetExpiry: time.Now().Add(10 * time.Minute),
			}, nil
		},
	}
	svc, _ := newTestServices(t, repo)
	err := svc.ConfirmPasswordReset(dtos.ConfirmPasswordResetDTO{
		Email: "alice@example.com", Token: "wrongtoken", NewPassword: "newpass123",
	})
	if err == nil || err.Error() != "invalid password reset token" {
		t.Errorf("expected 'invalid password reset token', got: %v", err)
	}
}

func TestConfirmPasswordReset_UpdatePasswordFails(t *testing.T) {
	plainToken := "resettoken123"
	hashedToken := hashedPassword(t, plainToken)
	repo := &MockUserRepository{
		FindByEmailFn: func(email string) (*models.User, error) {
			return &models.User{
				ID: 1, ResetPasswordToken: hashedToken,
				PasswordResetExpiry: time.Now().Add(10 * time.Minute),
			}, nil
		},
		UpdatePasswordFn: func(userID uint, passwordHash string) error { return errors.New("db error") },
	}
	svc, _ := newTestServices(t, repo)
	err := svc.ConfirmPasswordReset(dtos.ConfirmPasswordResetDTO{
		Email: "alice@example.com", Token: plainToken, NewPassword: "newpass123",
	})
	if err == nil {
		t.Error("expected error when UpdatePassword fails, got nil")
	}
}

func TestConfirmPasswordReset_ClearTokenFails(t *testing.T) {
	plainToken := "resettoken123"
	hashedToken := hashedPassword(t, plainToken)
	repo := &MockUserRepository{
		FindByEmailFn: func(email string) (*models.User, error) {
			return &models.User{
				ID: 1, ResetPasswordToken: hashedToken,
				PasswordResetExpiry: time.Now().Add(10 * time.Minute),
			}, nil
		},
		UpdatePasswordFn:          func(userID uint, passwordHash string) error { return nil },
		ClearPasswordResetTokenFn: func(userID uint) error { return errors.New("db error") },
	}
	svc, _ := newTestServices(t, repo)
	err := svc.ConfirmPasswordReset(dtos.ConfirmPasswordResetDTO{
		Email: "alice@example.com", Token: plainToken, NewPassword: "newpass123",
	})
	if err == nil {
		t.Error("expected error when ClearPasswordResetToken fails, got nil")
	}
}

// ── ChangePassword ─────────────────────────────────────────────────────────────

func TestChangePassword_Success(t *testing.T) {
	oldHashed := hashedPassword(t, "oldpassword")
	_, redisClient := newTestRedis(t)
	sessionSvc := services.NewSessionService(redisClient)
	repo := &MockUserRepository{
		FindByIDFn:       func(userID uint) (*models.User, error) { return &models.User{ID: userID, Password: oldHashed}, nil },
		ChangePasswordFn: func(userID uint, newPassword string) error { return nil },
	}
	authSvc := services.NewAuthService(repo, sessionSvc)
	ctx := context.Background()
	sessionID, _ := sessionSvc.CreateSession(ctx, "1")
	err := authSvc.ChangePassword(ctx, sessionID, dtos.ChangePasswordDTO{OldPassword: "oldpassword", NewPassword: "newpassword123"})
	if err != nil {
		t.Fatalf("expected no error, got: %v", err)
	}
}

func TestChangePassword_WrongOldPassword(t *testing.T) {
	oldHashed := hashedPassword(t, "correctold")
	_, redisClient := newTestRedis(t)
	sessionSvc := services.NewSessionService(redisClient)
	repo := &MockUserRepository{
		FindByIDFn: func(userID uint) (*models.User, error) { return &models.User{ID: userID, Password: oldHashed}, nil },
	}
	authSvc := services.NewAuthService(repo, sessionSvc)
	ctx := context.Background()
	sessionID, _ := sessionSvc.CreateSession(ctx, "1")
	err := authSvc.ChangePassword(ctx, sessionID, dtos.ChangePasswordDTO{OldPassword: "wrongold", NewPassword: "newpassword123"})
	if err == nil || err.Error() != "initial password does not match" {
		t.Errorf("expected 'initial password does not match', got: %v", err)
	}
}

func TestChangePassword_SessionNotFound(t *testing.T) {
	svc, _ := newTestServices(t, &MockUserRepository{})
	err := svc.ChangePassword(context.Background(), "bad-session", dtos.ChangePasswordDTO{OldPassword: "old", NewPassword: "new"})
	if err == nil || err.Error() != "session not found" {
		t.Errorf("expected 'session not found', got: %v", err)
	}
}

func TestChangePassword_InvalidUserIDInRedis(t *testing.T) {
	_, redisClient := newTestRedis(t)
	sessionSvc := services.NewSessionService(redisClient)
	authSvc := services.NewAuthService(&MockUserRepository{}, sessionSvc)
	ctx := context.Background()
	redisClient.Set(ctx, "session:bad-uid-session", "not-a-number", 0)
	err := authSvc.ChangePassword(ctx, "bad-uid-session", dtos.ChangePasswordDTO{OldPassword: "old", NewPassword: "new"})
	if err == nil {
		t.Error("expected error for non-numeric userID, got nil")
	}
}

func TestChangePassword_FindByIDFails(t *testing.T) {
	_, redisClient := newTestRedis(t)
	sessionSvc := services.NewSessionService(redisClient)
	repo := &MockUserRepository{
		FindByIDFn: func(userID uint) (*models.User, error) { return nil, errors.New("db error") },
	}
	authSvc := services.NewAuthService(repo, sessionSvc)
	ctx := context.Background()
	sessionID, _ := sessionSvc.CreateSession(ctx, "1")
	err := authSvc.ChangePassword(ctx, sessionID, dtos.ChangePasswordDTO{OldPassword: "old", NewPassword: "new"})
	if err == nil {
		t.Error("expected error when FindByID fails, got nil")
	}
}
