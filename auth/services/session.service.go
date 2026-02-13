package services

import (
	"context"
	"fmt"
	"log"
	"time"

	"github.com/google/uuid"
	"github.com/redis/go-redis/v9"
)

/**
Sessions will be maintained for 24 hours
(Optional) Can issue refresh tokens for 1 week
*/

type SessionService struct {
	RedisClient *redis.Client
}

func NewSessionService(redisClient *redis.Client) *SessionService {
	return &SessionService{RedisClient: redisClient}
}

// CreateSession stores a new session in Redis
func (s *SessionService) CreateSession(ctx context.Context, userID string) (string, error) {
	// Check if there's already an active session for this user
	existingSessionID, err := s.RedisClient.Get(ctx, "userID:"+userID).Result()
	if err == nil {
		// Session exists, delete and create new session
		fmt.Println("Existing session found for user:", existingSessionID, ", deleting...")
		err = s.DeleteSession(ctx, existingSessionID)
		if err != nil {
			log.Printf("Failed to delete session: %v", err)
		}
	}

	// Generate a new session ID
	sessionID := uuid.NewString()

	// Store the session (mapping userID -> sessionID, and sessionID -> userID)
	err = s.RedisClient.Set(ctx, "userID:"+userID, sessionID, time.Hour*24).Err() // store session for 24 hours
	if err != nil {
		return "", err
	}

	// Store the reverse mapping sessionID -> userID
	err = s.RedisClient.Set(ctx, "session:"+sessionID, userID, time.Hour*24).Err() // store session for 24 hours
	if err != nil {
		return "", err
	}

	return sessionID, nil
}

// GetSession retrieves a session from Redis
func (s *SessionService) GetSessionBySessionId(ctx context.Context, sessionID string) (string, error) {
	userID, err := s.RedisClient.Get(ctx, "session:"+sessionID).Result()
	if err != nil {
		return "", err
	}
	return userID, nil
}

func (s *SessionService) GetSessionByUserId(ctx context.Context, userID string) (string, error) {
	userID, err := s.RedisClient.Get(ctx, "userID:"+userID).Result()
	if err != nil {
		return "", err
	}
	return userID, nil
}

// DeleteSession removes both the session-to-user and user-to-session mappings from Redis
func (s *SessionService) DeleteSession(ctx context.Context, sessionID string) error {
	// Retrieve the userID associated with the sessionID
	userID, err := s.RedisClient.Get(ctx, "session:"+sessionID).Result()
	if err != nil {
		if err == redis.Nil {
			// Session does not exist
			return nil
		}
		return err
	}

	// Delete the sessionID -> userID mapping
	if err := s.RedisClient.Del(ctx, "session:"+sessionID).Err(); err != nil {
		return err
	}

	// Delete the userID -> sessionID mapping
	if err := s.RedisClient.Del(ctx, "userID:"+userID).Err(); err != nil {
		return err
	}

	return nil
}
