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
}

variable "route53_zone_name" {
  description = "Public Route53 zone name (must end with a trailing dot)"
  type        = string
  default     = "myapp.com."
}

variable "api_domain_name" {
  description = "Public API FQDN to point to the ALB"
  type        = string
  default     = "api.myapp.com"
}
