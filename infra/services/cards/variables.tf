variable "aws_region" {
  description = "AWS region"
  default     = "ap-southeast-1"
  type        = string
}

variable "service_name" {
  description = "Cards ECS service name"
  type        = string
  default     = "speedcardgame-cards"
}

# --- GitHub Container Registry Variables ---
variable "github_username" {
  description = "GitHub username or organization (e.g., 'my-org')"
  type        = string
}

variable "github_repo_name" {
  description = "The repository name on GitHub"
  type        = string
}

variable "ghcr_pat_secret_arn" {
  description = "The ARN of the AWS Secret containing the GHCR PAT (username/password)"
  type        = string
}
# -------------------------------------------

variable "image_tag" {
  description = "Image tag for the cards service image"
  type        = string
  default     = "latest"
}

variable "image_repo" {
  description = "Image repository for the cards service image"
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
  description = "Assign public IP to Fargate tasks (needed for ghcr.io pull when no NAT is present)"
  type        = bool
  default     = true
}

variable "use_managed_alb_stack" {
  description = "When true, read ALB target groups/security group/API URL from infra/services/alb remote state."
  type        = bool
  default     = true
}

variable "alb_stack_state_path" {
  description = "Path to infra/services/alb terraform state file for shared ALB wiring."
  type        = string
  default     = "../alb/terraform.tfstate"
}

variable "alb_security_group_id" {
  description = "Shared ALB security group ID override when not using remote state"
  type        = string
  default     = null
}

variable "target_group_arn" {
  description = "Cards target group ARN override when not using remote state"
  type        = string
  default     = null
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
  type    = string
  default = null
}

variable "postgres_user" {
  type    = string
  default = null
}

variable "postgres_password" {
  type      = string
  sensitive = true
  default   = null
}

variable "postgres_password_secret_arn" {
  description = "Secrets Manager ARN containing POSTGRES_PASSWORD JSON key for auth runtime"
  type        = string
  default     = null
}

variable "cards_runtime_secret_name" {
  description = "Cards runtime secret name for IAM permission and ECS secret fallback when ARN is not present in remote state"
  type        = string
  default     = "speedcardgame-cards-runtime"
}

variable "postgres_db" {
  type    = string
  default = null
}

variable "postgres_port" {
  type    = number
  default = null
}

variable "postgres_sslmode" {
  type    = string
  default = "disable"
}

variable "postgres_timezone" {
  type    = string
  default = "UTC"
}

variable "redis_host" {
  type    = string
  default = null
}

variable "redis_port" {
  type    = number
  default = null
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

variable "debug_log_enabled" {
  description = "Enable debug logging in cards service"
  type        = bool
  default     = false
}

variable "http_request_log_enabled" {
  description = "Enable HTTP request logging in cards service"
  type        = bool
  default     = true
}