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