variable "aws_region" {
  description = "AWS region"
  type        = string
}

variable "service_name" {
  description = "Service name used for ECS, ECR, and related resources"
  type        = string
}

variable "vpc_id" {
  description = "VPC ID for ECS networking"
  type        = string
}

variable "subnet_ids" {
  description = "Subnet IDs for ECS service ENIs"
  type        = list(string)
}

variable "container_port" {
  description = "Container and host port for the service"
  type        = number
}

variable "environment" {
  description = "Environment variables for the container"
  type        = map(string)
  default     = {}
}

variable "image_tag" {
  description = "Container image tag to deploy from ECR"
  type        = string
  default     = "latest"
}

variable "cpu" {
  description = "Fargate task CPU units"
  type        = number
  default     = 256
}

variable "memory" {
  description = "Fargate task memory (MiB)"
  type        = number
  default     = 512
}

variable "desired_count" {
  description = "Number of ECS tasks"
  type        = number
  default     = 1
}

variable "assign_public_ip" {
  description = "Assign public IP to task ENI"
  type        = bool
  default     = true
}
