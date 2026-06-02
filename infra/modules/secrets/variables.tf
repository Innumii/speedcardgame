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

variable "cards_service_base_url" {
  description = "Auth CARDS_SERVICE_BASE_URL to store for runtime injection"
  type        = string
  sensitive   = true
  default     = null
}

variable "stripe_secret_key" {
  description = "Stripe Secret API Key for payment processing"
  type        = string
  sensitive   = true
  default     = null
}

variable "stripe_webhook_secret" {
  description = "Stripe Webhook Secret for validating webhook signatures"
  type        = string
  sensitive   = true
  default     = null
}

variable "tls_key" {
  description = "TLS certificate and private key in PEM format for HTTPS support, stored as a single string with certificate followed by private key"
  type        = string
  sensitive   = true
  default     = null
}

variable "tls_cert" {
  description = "TLS certificate in PEM format for HTTPS support"
  type        = string
  sensitive   = true
  default     = null
}


