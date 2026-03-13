package services_test

import (
	"testing"

	"github.com/Ryanljk/speedcardgame/auth/services"
	"github.com/Ryanljk/speedcardgame/auth/utils"
	"github.com/alicebob/miniredis/v2"
	"github.com/redis/go-redis/v9"
)

func newTestRedis(t *testing.T) (*miniredis.Miniredis, *redis.Client) {
	t.Helper()
	mr, err := miniredis.Run()
	if err != nil {
		t.Fatalf("failed to start miniredis: %v", err)
	}
	t.Cleanup(mr.Close)
	client := redis.NewClient(&redis.Options{Addr: mr.Addr()})
	return mr, client
}

func newTestServices(t *testing.T, repo *MockUserRepository) (*services.AuthService, *services.SessionService) {
	t.Helper()
	_, redisClient := newTestRedis(t)
	sessionSvc := services.NewSessionService(redisClient)
	authSvc := services.NewAuthService(repo, sessionSvc)
	return authSvc, sessionSvc
}

func hashedPassword(t *testing.T, plain string) string {
	t.Helper()
	h, err := utils.Hash(plain)
	if err != nil {
		t.Fatalf("failed to hash password: %v", err)
	}
	return h
}
