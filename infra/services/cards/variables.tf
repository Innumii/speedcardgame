variable "aws_region" {
  description = "AWS region"
  default     = "ap-southeast-1"
  type        = string
}

variable "service_name" {
  description = "Cards ECS/ECR service name"
  type        = string
  default     = "speedcardgame-cards"
}

variable "image_tag" {
  description = "Cards image tag in ECR"
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

variable "use_managed_data_stack" {
  description = "When true, read DATABASE_URL from infra/data stack remote state."
  type        = bool
  default     = true
}

variable "data_stack_state_path" {
  description = "Path to infra/data terraform state file for auto wiring."
  type        = string
  default     = "../../data/terraform.tfstate"
}

variable "database_url" {
  description = "Cards DATABASE_URL reachable from cards task"
  type        = string
  default     = null
}

variable "database_url_secret_arn" {
  description = "Secrets Manager ARN containing DATABASE_URL JSON key for cards runtime"
  type        = string
  default     = null
}
