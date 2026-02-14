variable "project_name" {
  description = "Project name prefix for AWS resources"
  type        = string
  default     = "speedcardgame-server"
}

variable "aws_region" {
  description = "AWS region to deploy into"
  type        = string
  default     = "us-east-1"
}

variable "vpc_cidr" {
  description = "CIDR block for the VPC"
  type        = string
  default     = "10.40.0.0/16"
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

variable "server_port" {
  description = "Host port for game server"
  type        = number
  default     = 4000
}

variable "server_healthcheck_path" {
  description = "ALB health check path for game server"
  type        = string
  default     = "/"
}

variable "tags" {
  description = "Tags to apply to AWS resources"
  type        = map(string)
  default     = {}
}
