//go:build integration

package integration_test

import (
	"context"
	"fmt"
	"os"
	"testing"

	"github.com/joho/godotenv"
	"github.com/Ryanljk/speedcardgame/auth/models"
	"github.com/Ryanljk/speedcardgame/auth/repositories"
	"github.com/Ryanljk/speedcardgame/auth/services"
	"github.com/redis/go-redis/v9"
	"gorm.io/driver/postgres"
	"gorm.io/gorm"
	"gorm.io/gorm/logger"
)

func init() {
	_ = godotenv.Load("../../.env")
}

type testDeps struct {
	db         *gorm.DB
	redis      *redis.Client
	authSvc    *services.AuthService
	sessionSvc *services.SessionService
	userRepo   repositories.UserRepository
}

func setupAuthTest(t *testing.T) *testDeps {
	t.Helper()

	db, err := gorm.Open(postgres.Open(buildAuthDSN()), &gorm.Config{
		Logger: logger.Default.LogMode(logger.Silent),
	})
	if err != nil {
		t.Fatalf("failed to connect to auth DB: %v\nMake sure docker-compose is running", err)
	}
	if err := db.AutoMigrate(&models.User{}); err != nil {
		t.Fatalf("failed to migrate: %v", err)
	}

	redisAddr := fmt.Sprintf("localhost:%s",
		getEnvOrDefault("REDIS_PORT", "6379"),
	)
	redisClient := redis.NewClient(&redis.Options{Addr: redisAddr})
	if err := redisClient.Ping(context.Background()).Err(); err != nil {
		t.Fatalf("failed to connect to Redis: %v\nMake sure docker-compose is running", err)
	}

	userRepo := repositories.NewGormUserRepository(db)
	sessionSvc := services.NewSessionService(redisClient)
	authSvc := services.NewAuthService(userRepo, sessionSvc)

	deps := &testDeps{
		db:         db,
		redis:      redisClient,
		authSvc:    authSvc,
		sessionSvc: sessionSvc,
		userRepo:   userRepo,
	}

	t.Cleanup(func() {
		cleanAuthDB(t, db)
		redisClient.FlushAll(context.Background())
		redisClient.Close()
	})

	return deps
}

func cleanAuthDB(t *testing.T, db *gorm.DB) {
	t.Helper()
	db.Exec("DELETE FROM users")
}

func buildAuthDSN() string {
	user := getEnvOrDefault("POSTGRES_USER", "postgres")
	pass := getEnvOrDefault("POSTGRES_PASSWORD", "postgres")
	dbname := getEnvOrDefault("POSTGRES_DB", "auth")
	port := getEnvOrDefault("POSTGRES_PORT", "5432")
	sslmode := getEnvOrDefault("POSTGRES_SSLMODE", "disable")
	return fmt.Sprintf("host=localhost user=%s password=%s dbname=%s port=%s sslmode=%s TimeZone=UTC",
		user, pass, dbname, port, sslmode)
}

func getEnvOrDefault(key, fallback string) string {
	if v := os.Getenv(key); v != "" {
		return v
	}
	return fallback
}
