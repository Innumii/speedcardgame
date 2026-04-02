// POST auth/register
// POST auth/login
// POST auth/logout
// POST auth/refresh
// POST auth/password/reset/request
// POST auth/password/reset/confirm
// GET auth/me
// GET auth/verify
// POST auth/role/update ( dev only )

package controllers

import (
	"context"
	"net/http"
	"strings"

	"github.com/Ryanljk/speedcardgame/auth/dtos"
	"github.com/Ryanljk/speedcardgame/auth/services"

	"github.com/gin-gonic/gin"
)

type AuthController struct {
	AuthService *services.AuthService
}

func sessionIDFromRequest(c *gin.Context) (string, bool) {
	sessionID, err := c.Cookie("session_id")
	if err == nil && strings.TrimSpace(sessionID) != "" {
		return sessionID, true
	}

	headerSessionID := strings.TrimSpace(c.GetHeader("X-Session-ID"))
	if headerSessionID != "" {
		return headerSessionID, true
	}

	return "", false
}

// Constructor function
// Accepts an instance of authService and returns a new authController
func NewAuthController(authService *services.AuthService) *AuthController {
	return &AuthController{AuthService: authService}
}

// @Summary Register user
// @Tags users
// @Accept  json
// @Produce  json
// @Param registerDTO body dtos.RegisterDTO true "Registration details"
// @Success 201
// @Failure 400
// @Router /register [post]
// c *gin.Context is the context that is used to access the current request and response parameters
func (controller *AuthController) Register(c *gin.Context) {
	var registerDTO dtos.RegisterDTO

	// If the request body does not match the RegisterDTO struct, return an error
	if err := c.ShouldBindJSON(&registerDTO); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	// Call the Register method from the authService
	// AuthService is a pointer to the authService object that was passed in when the controller was created
	user, err := controller.AuthService.Register(registerDTO)
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	c.JSON(http.StatusCreated, gin.H{"message": "user created successfully", "user": user})
}

// @Summary Login user
// @Tags users
// @Accept  json
// @Produce  json
// @Param loginDTO body dtos.LoginDTO true "Login details"
// @Success 200
// @Failure 401
// @Router /login [post]
func (controller *AuthController) Login(c *gin.Context) {
	var loginDTO dtos.LoginDTO

	if err := c.ShouldBindJSON(&loginDTO); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	user, sessionID, err := controller.AuthService.Login(context.Background(), loginDTO)
	if err != nil {
		if err.Error() == "you are already logged in" {
			c.JSON(http.StatusConflict, gin.H{"error": err.Error()})
			return
		}
		c.JSON(http.StatusUnauthorized, gin.H{"error": err.Error()})
		return
	}

	// Set session ID as a cookie
	c.SetSameSite(http.SameSiteLaxMode)
	c.SetCookie("session_id", sessionID, 3600*24, "/", "", true, true)

	c.JSON(http.StatusOK, gin.H{"message": "login successful", "user": user, "session_id": sessionID})
}

// @Summary Logout user
// @Tags users
// @Accept  json
// @Produce  json
// @Success 200
// @Failure 401
// @Failure 500
// @Router /logout [post]
func (controller *AuthController) Logout(c *gin.Context) {
	sessionID, ok := sessionIDFromRequest(c)
	if !ok {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "No session ID provided"})
		return
	}

	// Call the AuthService to log out and delete the session
	err := controller.AuthService.Logout(context.Background(), sessionID)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to log out"})
		return
	}

	// Clear the session cookie
	c.SetSameSite(http.SameSiteLaxMode)
	c.SetCookie("session_id", "", -1, "/", "", true, true)

	// Respond with success
	c.JSON(http.StatusOK, gin.H{"message": "Logged out successfully"})
}

// @Summary Get logged in user info using session id cookie
// @Tags users
// @Accept  json
// @Produce  json
// @Success 200 {object} models.User
// @Failure 401
// @Router /me [get]
func (controller *AuthController) GetMe(c *gin.Context) {
	sessionID, ok := sessionIDFromRequest(c)
	if !ok {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "No session ID provided"})
		return
	}

	// Call the AuthService to get the user details
	user, err := controller.AuthService.GetMe(context.Background(), sessionID)
	if err != nil {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "Invalid session ID"})
		return
	}

	c.JSON(http.StatusOK, gin.H{"user": user})
}

// @Summary Reset password request
// @Tags users
// @Accept  json
// @Produce  json
// @Param requestPasswordResetDTO body dtos.RequestPasswordResetDTO true "Request password reset"
// @Success 200
// @Failure 400
// @Failure 500
// @Router /reset-password [post]
func (controller *AuthController) RequestPasswordReset(c *gin.Context) {
	var requestPasswordResetDTO dtos.RequestPasswordResetDTO

	if err := c.ShouldBindJSON(&requestPasswordResetDTO); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	token, err := controller.AuthService.RequestPasswordReset(requestPasswordResetDTO)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to request password reset"})
		return
	}

	c.JSON(http.StatusOK, gin.H{"message": "Password reset requested successfully", "token": token})
}

// @Summary Reset password request
// @Tags users
// @Accept  json
// @Produce  json
// @Param confirmPasswordResetDTO body dtos.ConfirmPasswordResetDTO true "Confirm password reset"
// @Success 200
// @Failure 400
// @Failure 500
// @Router /reset-password/confirm [post]
func (controller *AuthController) ConfirmPasswordReset(c *gin.Context) {
	var confirmPasswordResetDTO dtos.ConfirmPasswordResetDTO

	if err := c.ShouldBindJSON(&confirmPasswordResetDTO); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	err := controller.AuthService.ConfirmPasswordReset(confirmPasswordResetDTO)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "Failed to confirm password reset"})
		return
	}

	c.JSON(http.StatusOK, gin.H{"message": "Password reset confirmed successfully"})
}

// @Summary Change password with old password
// @Tags users
// @Accept  json
// @Produce  json
// @Param changePasswordDto body dtos.ChangePasswordDTO true "Change password"
// @Success 200
// @Failure 400
// @Failure 401
// @Failure 500
// @Router /change-password [patch]
func (controller *AuthController) ChangePassword(c *gin.Context) {
	var changePasswordDto dtos.ChangePasswordDTO

	if err := c.ShouldBindJSON(&changePasswordDto); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}

	// Retrieve session ID from the cookie
	sessionID, err := c.Cookie("session_id")
	if err != nil {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "No session ID provided"})
		return
	}

	err = controller.AuthService.ChangePassword(context.Background(), sessionID, changePasswordDto)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": err.Error()})
		return
	}

	c.JSON(http.StatusOK, gin.H{"message": "Password changed successfully"})
}

// TODO: Implement the following routes
// GET auth/verify -> to verify email
// POST auth/refresh -> to refresh the session ( optional )
// POST auth/role/update ( dev only )
