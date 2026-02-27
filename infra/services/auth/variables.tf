variable "aws_region" {
  description = "AWS region"
  type        = string
  default     = "us-east-1"
}

variable "service_name" {
  description = "Auth ECS/ECR service name"
  type        = string
  default     = "speedcardgame-auth"
}

variable "image_tag" {
  description = "Auth image tag in ECR"
  type        = string
  default     = "latest"
}

variable "use_managed_data_stack" {
  description = "When true, read Postgres/Redis values from infra/data stack remote state."
  type        = bool
  default     = true
}

variable "data_stack_state_path" {
  description = "Path to infra/data terraform state file for auto wiring."
  type        = string
  default     = "../../data/terraform.tfstate"
}

variable "postgres_host" {
  description = "Postgres host reachable from auth task"
  type        = string
  default     = null
}

variable "postgres_user" {
  description = "Postgres user"
  type        = string
  default     = null
}

variable "postgres_password" {
  description = "Postgres password"
  type        = string
  sensitive   = true
  default     = null
}

variable "postgres_db" {
  description = "Postgres database name"
  type        = string
  default     = null
}

variable "postgres_port" {
  description = "Postgres port"
  type        = number
  default     = null
}

variable "postgres_sslmode" {
  description = "Postgres SSL mode"
  type        = string
  default     = "disable"
}

variable "postgres_timezone" {
  description = "Postgres timezone"
  type        = string
  default     = "UTC"
}

variable "redis_host" {
  description = "Redis host reachable from auth task"
  type        = string
  default     = null
}

variable "redis_port" {
  description = "Redis port"
  type        = number
  default     = null
}

variable "cards_service_host" {
  description = "Cards service host reachable from auth task"
  type        = string
}

variable "cards_service_port" {
  description = "Cards service port"
  type        = number
  default     = 8080
}
