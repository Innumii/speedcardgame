variable "aws_region" {
  description = "AWS region"
  default     = "ap-southeast-1"
  type        = string
}

variable "service_name" {
  description = "Server ECS/ECR service name"
  type        = string
  default     = "speedcardgame-server"
}

variable "image_tag" {
  description = "Server image tag in ECR"
  type        = string
  default     = "latest"
}

variable "image_repo" {
  description = "GHCR image repo path, e.g. owner/g4t2-server-service"
  type        = string
}

variable "github_username" {
  description = "GitHub username or organization"
  type        = string
}

variable "github_repo_name" {
  description = "GitHub repository name"
  type        = string
}

variable "ghcr_pat_secret_arn" {
  description = "Secrets Manager ARN with GHCR credentials"
  type        = string
}

variable "cpu" {
  description = "Fargate CPU units"
  type        = string
  default     = "256"
}

variable "memory" {
  description = "Fargate memory"
  type        = string
  default     = "512"
}

variable "assign_public_ip" {
  description = "Assign public IP to ECS tasks"
  type        = bool
  default     = true
}
