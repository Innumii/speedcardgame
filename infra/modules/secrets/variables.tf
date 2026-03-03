variable "aws_region" {
  description = "AWS region"
  default     = "ap-southeast-1"
  type        = string
}

variable "github_username" {
  description = "GitHub username or organization"
  type        = string
}

variable "github_token" {
  description = "GitHub Personal Access Token (PAT) with read:packages scope"
  type        = string
  sensitive   = true
}

variable "existing_ghcr_secret_arn" {
  description = "Existing GHCR secret ARN to reuse instead of creating ghcr-auth-credentials"
  type        = string
  default     = null
}

variable "auth_runtime_secret_name" {
  description = "Secret name for auth runtime values"
  type        = string
  default     = "speedcardgame-auth-runtime"
}

variable "cards_runtime_secret_name" {
  description = "Secret name for cards runtime values"
  type        = string
  default     = "speedcardgame-cards-runtime"
}

variable "auth_postgres_password" {
  description = "Auth postgres password to store for runtime injection"
  type        = string
  sensitive   = true
}

variable "cards_database_url" {
  description = "Cards DATABASE_URL to store for runtime injection"
  type        = string
  sensitive   = true
}

