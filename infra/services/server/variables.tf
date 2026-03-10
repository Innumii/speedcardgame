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

variable "use_managed_alb_stack" {
  description = "When true, read API host from infra/services/alb remote state."
  type        = bool
  default     = true
}

variable "alb_stack_state_path" {
  description = "Path to infra/services/alb terraform state file for API host wiring."
  type        = string
  default     = "../alb/terraform.tfstate"
}

variable "cards_service_host" {
  description = "Cards service host reachable from server task"
  type        = string
  default     = null
}

variable "cards_service_port" {
  description = "Cards service port reachable from server task"
  type        = number
  default     = null
}

variable "cards_service_base_url" {
  description = "Base URL for cards service (e.g. https://example.com)"
  type        = string
  default     = null
}
