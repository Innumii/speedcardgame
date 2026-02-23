package dtos

type RegisterDTO struct {
	Name     string `json:"name" binding:"required,min=3,max=50" validate:"regex=^[a-zA-Z ]*$"`
	Email    string `json:"email" binding:"required,email"`
	Password string `json:"password" binding:"required,min=8"`
}

type LoginDTO struct {
	Email    string `json:"email" binding:"required"`
	Password string `json:"password" binding:"required"`
}

type RequestPasswordResetDTO struct {
	Email string `json:"email" binding:"required"`
}

type ConfirmPasswordResetDTO struct {
	Email       string `json:"email" binding:"required"`
	Token       string `json:"token" binding:"required"`
	NewPassword string `json:"newPassword" binding:"required,min=8"`
}

type ChangePasswordDTO struct {
	OldPassword string `json:"oldPassword" binding:"required"`
	NewPassword string `json:"newPassword" binding:"required,min=8"`
}
