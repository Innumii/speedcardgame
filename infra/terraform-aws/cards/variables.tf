variable "project_name" {
  description = "Project name prefix for AWS resources"
  type        = string
  default     = "speedcardgame-cards"
}

variable "aws_region" {
  description = "AWS region to deploy into"
  type        = string
  default     = "us-east-1"
}

variable "vpc_cidr" {
  description = "CIDR block for the VPC"
  type        = string
  default     = "10.30.0.0/16"
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

variable "cards_app_port" {
  description = "Host port for cards service"
  type        = number
  default     = 8082
}

variable "cards_port" {
  description = "Container port for cards service"
  type        = number
  default     = 8082
}

variable "cards_postgres_user" {
  description = "Cards Postgres user"
  type        = string
  default     = "cardsuser"
}

variable "cards_postgres_password" {
  description = "Cards Postgres password"
  type        = string
  sensitive   = true
  default     = "cardspass"
}

variable "cards_postgres_db" {
  description = "Cards Postgres database"
  type        = string
  default     = "cardsdb"
}

variable "cards_database_url" {
  description = "Cards DATABASE_URL"
  type        = string
  default     = "postgres://cardsuser:cardspass@cards-db:5432/cardsdb?sslmode=disable"
}

variable "cards_healthcheck_path" {
  description = "ALB health check path for cards"
  type        = string
  default     = "/"
}

variable "tags" {
  description = "Tags to apply to AWS resources"
  type        = map(string)
  default     = {}
}
