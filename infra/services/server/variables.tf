variable "aws_region" {
  description = "AWS region"
  type        = string
  default     = "us-east-1"
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
