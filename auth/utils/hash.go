package utils

import (
	"crypto/rand"
	"encoding/hex"
	"golang.org/x/crypto/bcrypt"
)

// Hash hashes the password/string using bcrypt
func Hash(password string) (string, error) {
	hash, err := bcrypt.GenerateFromPassword([]byte(password), bcrypt.DefaultCost)
	return string(hash), err
}

// CheckHash compares a hashed string (like a password or token) with a plain string
func CheckHash(hashedString, plainString string) error {
	// CompareHashAndPassword returns nil on success, otherwise an error
	err := bcrypt.CompareHashAndPassword([]byte(hashedString), []byte(plainString))
	return err
}

// GenerateSecureToken generates a secure random token
func GenerateSecureToken() (string, error) {
	token := make([]byte, 32)
	_, err := rand.Read(token)
	if err != nil {
		return "", err
	}
	return hex.EncodeToString(token), nil
}
