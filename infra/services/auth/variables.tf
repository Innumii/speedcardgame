variable "aws_region" {
  description = "AWS region"
  default     = "ap-southeast-1"
  type        = string
}

variable "service_name" {
  description = "Auth ECS service name"
  type        = string
  default     = "speedcardgame-auth"
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

variable "tls_secret_name" {
  description = "Secrets Manager secret containing TLS cert/key fields for service HTTPS startup"
  type        = string
  default     = "game-server-tls"
}
# -------------------------------------------

variable "image_tag" {
  description = "Image tag for the auth service image"
  type        = string
  default     = "latest"
}

variable "auth_image_repo" {
  description = "Auth-specific image repository fallback when using a shared tfvars file"
  type        = string
  default     = null
}

variable "auth_cpu" {
  description = "Fargate CPU units"
  type        = string
  default     = "256"
}

variable "auth_memory" {
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
  description = "Auth target group ARN override when not using remote state"
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

variable "cards_service_base_url_secret_arn" {
  description = "Secrets Manager ARN containing CARDS_SERVICE_BASE_URL JSON key for auth runtime"
  type        = string
  default     = null
}

variable "base_domain" {
  description = "Base public domain used for DNS records and endpoints"
  type        = string
  sensitive   = true
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

variable "use_aws_services" {
  description = "When true, enables AWS Secrets Manager integration and VPC endpoints for data services"
  type        = bool
  default     = true
}

variable "debug_log_enabled" {
  description = "Enable debug logging in auth service"
  type        = bool
  default     = false
}

variable "http_request_log_enabled" {
  description = "Enable HTTP request logging in auth service"
  type        = bool
  default     = true
}

variable "auth_desired_count" {
  description = "Number of desired auth service tasks"
  type        = number
  default     = 1
}

variable "internal_api_key" {
  description = "api key for JWT"
  type        = string
  sensitive   = true
}