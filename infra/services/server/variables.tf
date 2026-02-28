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
