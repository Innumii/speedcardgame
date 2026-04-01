variable "aws_region" {
  description = "AWS region"
  type        = string
  default     = "ap-southeast-1"
}

variable "alb_name" {
  description = "Name of the shared ALB"
  type        = string
  default     = "speedcardgame-api-alb"
}

variable "auth_target_group_name" {
  description = "Target group name for auth service"
  type        = string
  default     = "speedcard-auth-tg"
}

variable "cards_target_group_name" {
  description = "Target group name for cards service"
  type        = string
  default     = "speedcard-cards-tg"
}

variable "acm_certificate_arn" {
  description = "ACM certificate ARN for HTTPS listener"
  type        = string
  default     = null
}

variable "base_domain" {
  description = "Base public domain used for DNS records and endpoints"
  type        = string
  sensitive   = true
}

variable "enable_https_listener" {
  description = "Enable HTTPS listener and redirect HTTP to HTTPS"
  type        = bool
  default     = true
}

variable "environment" {
  description = "Environment tag value"
  type        = string
  default     = "dev"
}
