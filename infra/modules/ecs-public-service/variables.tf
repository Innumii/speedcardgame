variable "service_name" {
  description = "Name of the ECS service"
  type        = string
}

variable "github_username" {
  description = "GitHub username or organization"
  type        = string
}

variable "github_repo_name" {
  description = "The repository name on GitHub"
  type        = string
}

variable "ghcr_pat_secret_arn" {
  description = "The ARN of the AWS Secret containing the GHCR Personal Access Token"
  type        = string
}

variable "aws_region" {
  description = "AWS region"
  default     = "ap-southeast-1"
  type        = string
}

variable "vpc_id" {
  description = "VPC ID"
  type        = string
}

variable "subnet_ids" {
  description = "List of subnet IDs"
  type        = list(string)
}

variable "container_port" {
  description = "Container port"
  type        = number
}

variable "cpu" {
  description = "Fargate CPU units"
  type        = string
}

variable "memory" {
  description = "Fargate memory"
  type        = string
}

variable "desired_count" {
  description = "Number of tasks"
  type        = number
}

variable "image_tag" {
  description = "Docker image tag from CI"
  type        = string
}

variable "image_repo" {
  description = "Docker image repository (e.g., 'name/service')"
  type        = string
}

variable "environment" {
  description = "Environment variables"
  type        = map(string)
  default     = {}
}

variable "assign_public_ip" {
  description = "Assign public IP"
  type        = bool
  default     = false
}

variable "target_group_arn" {
  description = "ALB target group ARN"
  type        = string
  default     = null
}

variable "alb_security_group_id" {
  description = "ALB security group ID"
  type        = string
  default     = null
}