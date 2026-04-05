//go:build integration

package integration_test

import (
	"context"
	"testing"
	"time"

	"github.com/FYL-Studios/speedcardgame/auth/dtos"
)

// ════════════════════════════════════════════════════════════════
// REGISTER
// ════════════════════════════════════════════════════════════════

func TestIntegration_Register_Success(t *testing.T) {
	deps := setupAuthTest(t)

	dto := dtos.RegisterDTO{Name: "Alice", Email: "alice@example.com", Password: "secret123"}
	user, err := deps.authSvc.Register(dto)
	if err != nil {
		t.Fatalf("expected no error, got: %v", err)
	}
	if user.ID == 0 {
		t.Error("expected user to have an ID after registration")
	}
	if user.Email != "alice@example.com" {
		t.Errorf("expected email 'alice@example.com', got %q", user.Email)
	}

	dbUser, err := deps.userRepo.FindByEmail("alice@example.com")
	if err != nil {
		t.Fatalf("user not found in DB: %v", err)
	}
	if dbUser.Name != "Alice" {
		t.Errorf("expected name 'Alice' in DB, got %q", dbUser.Name)
	}
	if dbUser.Password == "secret123" {
		t.Error("expected password to be hashed, got plaintext")
	}
}

func TestIntegration_Register_DuplicateEmail(t *testing.T) {
	deps := setupAuthTest(t)

	dto := dtos.RegisterDTO{Name: "Alice", Email: "alice@example.com", Password: "secret123"}
	deps.authSvc.Register(dto)

	_, err := deps.authSvc.Register(dto)
	if err == nil || err.Error() != "email already taken" {
		t.Errorf("expected 'email already taken', got: %v", err)
	}
}

// ════════════════════════════════════════════════════════════════
// LOGIN
// ════════════════════════════════════════════════════════════════

func TestIntegration_Login_Success(t *testing.T) {
	deps := setupAuthTest(t)
	ctx := context.Background()

	deps.authSvc.Register(dtos.RegisterDTO{Name: "Bob", Email: "bob@example.com", Password: "mypassword"})

	user, sessionID, err := deps.authSvc.Login(ctx, dtos.LoginDTO{Email: "bob@example.com", Password: "mypassword"})
	if err != nil {
		t.Fatalf("expected no error, got: %v", err)
	}
	if user == nil {
		t.Fatal("expected user, got nil")
	}
	if sessionID == "" {
		t.Error("expected session ID, got empty string")
	}

	userID, err := deps.sessionSvc.GetSessionBySessionId(ctx, sessionID)
	if err != nil {
		t.Fatalf("session not found in Redis: %v", err)
	}
	if userID == "" {
		t.Error("expected userID in Redis session")
	}
}

func TestIntegration_Login_WrongPassword(t *testing.T) {
	deps := setupAuthTest(t)
	ctx := context.Background()

	deps.authSvc.Register(dtos.RegisterDTO{Name: "Carol", Email: "carol@example.com", Password: "correctpass"})

	_, _, err := deps.authSvc.Login(ctx, dtos.LoginDTO{Email: "carol@example.com", Password: "wrongpass"})
	if err == nil || err.Error() != "invalid password" {
		t.Errorf("expected 'invalid password', got: %v", err)
	}
}

func TestIntegration_Login_UserNotFound(t *testing.T) {
	deps := setupAuthTest(t)
	ctx := context.Background()

	_, _, err := deps.authSvc.Login(ctx, dtos.LoginDTO{Email: "ghost@example.com", Password: "anything"})
	if err == nil || err.Error() != "user not found" {
		t.Errorf("expected 'user not found', got: %v", err)
	}
}

func TestIntegration_Login_ReplacesExistingSession(t *testing.T) {
	deps := setupAuthTest(t)
	ctx := context.Background()

	deps.authSvc.Register(dtos.RegisterDTO{Name: "Dave", Email: "dave@example.com", Password: "pass123"})

	_, firstSession, _ := deps.authSvc.Login(ctx, dtos.LoginDTO{Email: "dave@example.com", Password: "pass123"})
	_, secondSession, _ := deps.authSvc.Login(ctx, dtos.LoginDTO{Email: "dave@example.com", Password: "pass123"})

	if firstSession == secondSession {
		t.Error("expected new session ID on second login")
	}

	_, err := deps.sessionSvc.GetSessionBySessionId(ctx, firstSession)
	if err == nil {
		t.Error("expected old session to be invalidated")
	}
}

// ════════════════════════════════════════════════════════════════
// LOGOUT
// ════════════════════════════════════════════════════════════════

func TestIntegration_Logout_Success(t *testing.T) {
	deps := setupAuthTest(t)
	ctx := context.Background()

	deps.authSvc.Register(dtos.RegisterDTO{Name: "Eve", Email: "eve@example.com", Password: "pass123"})
	_, sessionID, _ := deps.authSvc.Login(ctx, dtos.LoginDTO{Email: "eve@example.com", Password: "pass123"})

	err := deps.authSvc.Logout(ctx, sessionID)
	if err != nil {
		t.Fatalf("expected no error on logout, got: %v", err)
	}

	_, err = deps.sessionSvc.GetSessionBySessionId(ctx, sessionID)
	if err == nil {
		t.Error("expected session deleted from Redis after logout")
	}
}

func TestIntegration_Logout_InvalidSession(t *testing.T) {
	deps := setupAuthTest(t)
	ctx := context.Background()

	err := deps.authSvc.Logout(ctx, "nonexistent-session")
	if err == nil {
		t.Error("expected error for nonexistent session")
	}
}

// ════════════════════════════════════════════════════════════════
// GET ME
// ════════════════════════════════════════════════════════════════

func TestIntegration_GetMe_Success(t *testing.T) {
	deps := setupAuthTest(t)
	ctx := context.Background()

	deps.authSvc.Register(dtos.RegisterDTO{Name: "Frank", Email: "frank@example.com", Password: "pass123"})
	_, sessionID, _ := deps.authSvc.Login(ctx, dtos.LoginDTO{Email: "frank@example.com", Password: "pass123"})

	user, err := deps.authSvc.GetMe(ctx, sessionID)
	if err != nil {
		t.Fatalf("expected no error, got: %v", err)
	}
	if user.Email != "frank@example.com" {
		t.Errorf("expected 'frank@example.com', got %q", user.Email)
	}
	if user.Name != "Frank" {
		t.Errorf("expected 'Frank', got %q", user.Name)
	}
}

func TestIntegration_GetMe_InvalidSession(t *testing.T) {
	deps := setupAuthTest(t)
	ctx := context.Background()

	_, err := deps.authSvc.GetMe(ctx, "bad-session-id")
	if err == nil {
		t.Error("expected error for invalid session")
	}
}

// ════════════════════════════════════════════════════════════════
// PASSWORD RESET
// ════════════════════════════════════════════════════════════════

func TestIntegration_RequestPasswordReset_Success(t *testing.T) {
	deps := setupAuthTest(t)

	deps.authSvc.Register(dtos.RegisterDTO{Name: "Grace", Email: "grace@example.com", Password: "pass123"})

	token, err := deps.authSvc.RequestPasswordReset(dtos.RequestPasswordResetDTO{Email: "grace@example.com"})
	if err != nil {
		t.Fatalf("expected no error, got: %v", err)
	}
	if token == "" {
		t.Error("expected a reset token, got empty string")
	}

	user, _ := deps.userRepo.FindByEmail("grace@example.com")
	if user.ResetPasswordToken == "" {
		t.Error("expected hashed token stored in DB")
	}
	if user.PasswordResetExpiry.Before(time.Now()) {
		t.Error("expected expiry to be in the future")
	}
}

func TestIntegration_ConfirmPasswordReset_Success(t *testing.T) {
	deps := setupAuthTest(t)
	ctx := context.Background()

	deps.authSvc.Register(dtos.RegisterDTO{Name: "Hank", Email: "hank@example.com", Password: "oldpass123"})

	token, err := deps.authSvc.RequestPasswordReset(dtos.RequestPasswordResetDTO{Email: "hank@example.com"})
	if err != nil {
		t.Fatalf("failed to request reset: %v", err)
	}

	err = deps.authSvc.ConfirmPasswordReset(dtos.ConfirmPasswordResetDTO{
		Email:       "hank@example.com",
		Token:       token,
		NewPassword: "newpass456",
	})
	if err != nil {
		t.Fatalf("expected no error, got: %v", err)
	}

	// Old password should no longer work
	_, _, err = deps.authSvc.Login(ctx, dtos.LoginDTO{Email: "hank@example.com", Password: "oldpass123"})
	if err == nil {
		t.Error("expected old password to be rejected after reset")
	}

	// New password should work
	_, sessionID, err := deps.authSvc.Login(ctx, dtos.LoginDTO{Email: "hank@example.com", Password: "newpass456"})
	if err != nil {
		t.Fatalf("expected login with new password to succeed, got: %v", err)
	}
	if sessionID == "" {
		t.Error("expected session ID after login with new password")
	}
}

func TestIntegration_ConfirmPasswordReset_ExpiredToken(t *testing.T) {
	deps := setupAuthTest(t)

	deps.authSvc.Register(dtos.RegisterDTO{Name: "Iris", Email: "iris@example.com", Password: "pass123"})
	token, _ := deps.authSvc.RequestPasswordReset(dtos.RequestPasswordResetDTO{Email: "iris@example.com"})

	deps.db.Exec("UPDATE users SET password_reset_expiry = ? WHERE email = ?", time.Now().Add(-1*time.Hour), "iris@example.com")

	err := deps.authSvc.ConfirmPasswordReset(dtos.ConfirmPasswordResetDTO{
		Email: "iris@example.com", Token: token, NewPassword: "newpass456",
	})
	if err == nil || err.Error() != "password reset token has expired" {
		t.Errorf("expected 'password reset token has expired', got: %v", err)
	}
}

// ════════════════════════════════════════════════════════════════
// CHANGE PASSWORD
// ════════════════════════════════════════════════════════════════

func TestIntegration_ChangePassword_Success(t *testing.T) {
	deps := setupAuthTest(t)
	ctx := context.Background()

	deps.authSvc.Register(dtos.RegisterDTO{Name: "Jack", Email: "jack@example.com", Password: "oldpass123"})
	_, sessionID, _ := deps.authSvc.Login(ctx, dtos.LoginDTO{Email: "jack@example.com", Password: "oldpass123"})

	err := deps.authSvc.ChangePassword(ctx, sessionID, dtos.ChangePasswordDTO{
		OldPassword: "oldpass123",
		NewPassword: "newpass456",
	})
	if err != nil {
		t.Fatalf("expected no error, got: %v", err)
	}

	// Old password should fail
	_, _, err = deps.authSvc.Login(ctx, dtos.LoginDTO{Email: "jack@example.com", Password: "oldpass123"})
	if err == nil {
		t.Error("expected old password rejected after change")
	}

	// Logout first before testing new password
	deps.authSvc.Logout(ctx, sessionID)

	// New password should work
	_, _, err = deps.authSvc.Login(ctx, dtos.LoginDTO{Email: "jack@example.com", Password: "newpass456"})
	if err != nil {
		t.Fatalf("expected new password to work, got: %v", err)
	}
}

func TestIntegration_ChangePassword_WrongOldPassword(t *testing.T) {
	deps := setupAuthTest(t)
	ctx := context.Background()

	deps.authSvc.Register(dtos.RegisterDTO{Name: "Karen", Email: "karen@example.com", Password: "correctpass"})
	_, sessionID, _ := deps.authSvc.Login(ctx, dtos.LoginDTO{Email: "karen@example.com", Password: "correctpass"})

	err := deps.authSvc.ChangePassword(ctx, sessionID, dtos.ChangePasswordDTO{
		OldPassword: "wrongpass",
		NewPassword: "newpass456",
	})
	if err == nil || err.Error() != "initial password does not match" {
		t.Errorf("expected 'initial password does not match', got: %v", err)
	}
}

// ════════════════════════════════════════════════════════════════
// SESSION SERVICE
// ════════════════════════════════════════════════════════════════

func TestIntegration_Session_CreateAndRetrieve(t *testing.T) {
	deps := setupAuthTest(t)
	ctx := context.Background()

	sessionID, err := deps.sessionSvc.CreateSession(ctx, "user-42")
	if err != nil {
		t.Fatalf("expected no error, got: %v", err)
	}

	userID, err := deps.sessionSvc.GetSessionBySessionId(ctx, sessionID)
	if err != nil {
		t.Fatalf("expected session in Redis, got: %v", err)
	}
	if userID != "user-42" {
		t.Errorf("expected 'user-42', got %q", userID)
	}
}

func TestIntegration_Session_DeleteClearsBothMappings(t *testing.T) {
	deps := setupAuthTest(t)
	ctx := context.Background()

	sessionID, _ := deps.sessionSvc.CreateSession(ctx, "user-99")
	deps.sessionSvc.DeleteSession(ctx, sessionID)

	_, err := deps.sessionSvc.GetSessionBySessionId(ctx, sessionID)
	if err == nil {
		t.Error("expected session deleted from Redis")
	}

	_, err = deps.sessionSvc.GetSessionByUserId(ctx, "user-99")
	if err == nil {
		t.Error("expected user->session mapping deleted from Redis")
	}
}

func TestIntegration_Session_ExpiresAfterLogin(t *testing.T) {
	deps := setupAuthTest(t)
	ctx := context.Background()

	deps.authSvc.Register(dtos.RegisterDTO{Name: "Leo", Email: "leo@example.com", Password: "pass123"})
	_, session1, _ := deps.authSvc.Login(ctx, dtos.LoginDTO{Email: "leo@example.com", Password: "pass123"})
	_, session2, _ := deps.authSvc.Login(ctx, dtos.LoginDTO{Email: "leo@example.com", Password: "pass123"})

	if session1 == session2 {
		t.Error("expected different session IDs on each login")
	}

	// Check old session key directly in Redis
	_, err := deps.sessionSvc.GetSessionBySessionId(ctx, session1)
	if err == nil {
		t.Error("expected first session invalidated after second login")
	}
}
