variable "project_name" {
  description = "Project name prefix for AWS resources"
  type        = string
  default     = "speedcardgame-auth"
}

variable "aws_region" {
  description = "AWS region to deploy into"
  type        = string
  default     = "us-east-1"
}

variable "vpc_cidr" {
  description = "CIDR block for the VPC"
  type        = string
  default     = "10.20.0.0/16"
}

variable "instance_type" {
  description = "EC2 instance type"
  type        = string
  default     = "t3.medium"
}

variable "key_name" {
  description = "EC2 key pair name for SSH"
  type        = string
  default     = null
}

variable "ssh_cidr" {
  description = "CIDR allowed to SSH to EC2"
  type        = string
  default     = "0.0.0.0/0"
}

variable "image_tag" {
  description = "Tag to push/use for ECR images"
  type        = string
  default     = "latest"
}

variable "enable_local_image_build" {
  description = "Build and push images from local Dockerfiles"
  type        = bool
  default     = true
}

variable "auth_app_port" {
  description = "Host port for auth service"
  type        = number
  default     = 8081
}

variable "auth_postgres_user" {
  description = "Auth Postgres user"
  type        = string
  default     = "authuser"
}

variable "auth_postgres_password" {
  description = "Auth Postgres password"
  type        = string
  sensitive   = true
  default     = "authpass"
}

variable "auth_postgres_db" {
  description = "Auth Postgres database"
  type        = string
  default     = "authdb"
}

variable "auth_postgres_port" {
  description = "Auth Postgres port"
  type        = number
  default     = 5432
}

variable "auth_postgres_sslmode" {
  description = "Auth Postgres SSL mode"
  type        = string
  default     = "disable"
}

variable "auth_postgres_timezone" {
  description = "Auth Postgres timezone"
  type        = string
  default     = "UTC"
}

variable "auth_redis_port" {
  description = "Auth Redis port"
  type        = number
  default     = 6379
}

variable "auth_healthcheck_path" {
  description = "ALB health check path for auth"
  type        = string
  default     = "/"
}

variable "tags" {
  description = "Tags to apply to AWS resources"
  type        = map(string)
  default     = {}
}
