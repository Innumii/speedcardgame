package services_test

import (
	"context"
	"testing"

	"github.com/FYL-Studios/speedcardgame/auth/services"
)

func TestCreateSession_StoresAndReturnsSessionID(t *testing.T) {
	_, redisClient := newTestRedis(t)
	svc := services.NewSessionService(redisClient)

	sessionID, err := svc.CreateSession(context.Background(), "user-1")
	if err != nil {
		t.Fatalf("expected no error, got: %v", err)
	}
	if sessionID == "" {
		t.Error("expected a session ID, got empty string")
	}
}

func TestCreateSession_RejectsExistingSession(t *testing.T) {
	_, redisClient := newTestRedis(t)
	svc := services.NewSessionService(redisClient)
	ctx := context.Background()

	firstID, err := svc.CreateSession(ctx, "user-1")
	if err != nil {
		t.Fatalf("expected no error on first session create, got: %v", err)
	}

	secondID, err := svc.CreateSession(ctx, "user-1")
	if err == nil {
		t.Fatal("expected error on second session create, got nil")
	}
	if err.Error() != "you are already logged in" {
		t.Fatalf("expected 'you are already logged in', got: %v", err)
	}
	if secondID != "" {
		t.Errorf("expected empty session ID on rejection, got %q", secondID)
	}

	// Existing session must remain valid.
	_, err = svc.GetSessionBySessionId(ctx, firstID)
	if err != nil {
		t.Fatalf("expected original session to remain valid, got: %v", err)
	}
}

func TestCreateSession_SetFails(t *testing.T) {
	mr, redisClient := newTestRedis(t)
	svc := services.NewSessionService(redisClient)
	mr.Close()

	_, err := svc.CreateSession(context.Background(), "user-1")
	if err == nil {
		t.Error("expected error when Redis Set fails, got nil")
	}
}

func TestGetSessionBySessionId_Success(t *testing.T) {
	_, redisClient := newTestRedis(t)
	svc := services.NewSessionService(redisClient)
	ctx := context.Background()

	sessionID, _ := svc.CreateSession(ctx, "user-42")
	userID, err := svc.GetSessionBySessionId(ctx, sessionID)
	if err != nil {
		t.Fatalf("expected no error, got: %v", err)
	}
	if userID != "user-42" {
		t.Errorf("expected userID 'user-42', got %q", userID)
	}
}

func TestGetSessionBySessionId_NotFound(t *testing.T) {
	_, redisClient := newTestRedis(t)
	svc := services.NewSessionService(redisClient)

	_, err := svc.GetSessionBySessionId(context.Background(), "nonexistent")
	if err == nil {
		t.Error("expected error for missing session, got nil")
	}
}

func TestGetSessionByUserId_Success(t *testing.T) {
	_, redisClient := newTestRedis(t)
	svc := services.NewSessionService(redisClient)
	ctx := context.Background()

	sessionID, _ := svc.CreateSession(ctx, "user-7")
	gotSessionID, err := svc.GetSessionByUserId(ctx, "user-7")
	if err != nil {
		t.Fatalf("expected no error, got: %v", err)
	}
	if gotSessionID != sessionID {
		t.Errorf("expected session ID %q, got %q", sessionID, gotSessionID)
	}
}

func TestDeleteSession_RemovesBothMappings(t *testing.T) {
	_, redisClient := newTestRedis(t)
	svc := services.NewSessionService(redisClient)
	ctx := context.Background()

	sessionID, _ := svc.CreateSession(ctx, "user-5")
	if err := svc.DeleteSession(ctx, sessionID); err != nil {
		t.Fatalf("expected no error on delete, got: %v", err)
	}
	if _, err := svc.GetSessionBySessionId(ctx, sessionID); err == nil {
		t.Error("expected session->userID mapping to be deleted")
	}
	if _, err := svc.GetSessionByUserId(ctx, "user-5"); err == nil {
		t.Error("expected userID->sessionID mapping to be deleted")
	}
}

func TestDeleteSession_NonexistentIsNoop(t *testing.T) {
	_, redisClient := newTestRedis(t)
	svc := services.NewSessionService(redisClient)

	err := svc.DeleteSession(context.Background(), "ghost-session")
	if err != nil {
		t.Errorf("expected no error deleting nonexistent session, got: %v", err)
	}
}

func TestDeleteSession_DelFails(t *testing.T) {
	mr, redisClient := newTestRedis(t)
	svc := services.NewSessionService(redisClient)
	ctx := context.Background()

	sessionID, _ := svc.CreateSession(ctx, "user-1")
	mr.Close()

	err := svc.DeleteSession(ctx, sessionID)
	if err == nil {
		t.Error("expected error when Redis Del fails, got nil")
	}
}
