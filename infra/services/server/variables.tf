variable "aws_region" {
  description = "AWS region"
  default     = "ap-southeast-1"
  type        = string
}

variable "game_port" {
  description = "Raw TCP/TLS game server port"
  type        = number
  default     = 4000
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

variable "desired_count" {
  description = "Desired ECS task count"
  type        = number
  default     = 1
}

variable "vpc_id" {
  description = "Optional VPC ID override. If null, default VPC is used"
  type        = string
  default     = null
}

variable "subnet_ids" {
  description = "Optional subnet IDs override. If empty, default VPC subnets are used"
  type        = list(string)
  default     = []
}

variable "tls_certificate_secret_arn" {
  description = "Secrets Manager ARN containing the TLS certificate PEM for the game server"
  type        = string
  default     = null
  sensitive   = true
}

variable "tls_private_key_secret_arn" {
  description = "Secrets Manager ARN containing the TLS private key PEM for the game server"
  type        = string
  default     = null
  sensitive   = true
}

variable "route53_zone_name" {
  description = "Optional Route53 hosted zone name (e.g. example.com) for a stable game endpoint"
  type        = string
  default     = null
}

variable "route53_record_name" {
  description = "Optional Route53 record name to create in route53_zone_name (e.g. game.example.com)"
  type        = string
  default     = null
}

variable "write_client_env_file" {
  description = "Whether to generate client/env/.env from Terraform outputs"
  type        = bool
  default     = true
}
