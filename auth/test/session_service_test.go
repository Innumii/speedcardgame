package services_test

import (
	"context"
	"testing"

	"github.com/Ryanljk/speedcardgame/auth/services"
)

func TestCreateSession_StoresAndReturnsSessionID(t *testing.T) {
	_, redisClient := newTestRedis(t)
	svc := services.NewSessionService(redisClient)
	ctx := context.Background()

	sessionID, err := svc.CreateSession(ctx, "user-1")
	if err != nil {
		t.Fatalf("expected no error, got: %v", err)
	}
	if sessionID == "" {
		t.Error("expected a session ID, got empty string")
	}
}

func TestCreateSession_ReplacesExistingSession(t *testing.T) {
	_, redisClient := newTestRedis(t)
	svc := services.NewSessionService(redisClient)
	ctx := context.Background()

	firstID, _ := svc.CreateSession(ctx, "user-1")
	secondID, err := svc.CreateSession(ctx, "user-1")
	if err != nil {
		t.Fatalf("expected no error on second session create, got: %v", err)
	}
	if firstID == secondID {
		t.Error("expected a new session ID to replace the old one")
	}

	// Old session should no longer resolve
	_, err = svc.GetSessionBySessionId(ctx, firstID)
	if err == nil {
		t.Error("expected old session to be invalidated")
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

	err := svc.DeleteSession(ctx, sessionID)
	if err != nil {
		t.Fatalf("expected no error on delete, got: %v", err)
	}

	// session -> userID mapping should be gone
	_, err = svc.GetSessionBySessionId(ctx, sessionID)
	if err == nil {
		t.Error("expected session->userID mapping to be deleted")
	}

	// userID -> sessionID mapping should be gone
	_, err = svc.GetSessionByUserId(ctx, "user-5")
	if err == nil {
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
