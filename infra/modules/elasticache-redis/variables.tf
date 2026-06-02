variable "cluster_id" {
  description = "ElastiCache cluster id"
  type        = string
}

variable "vpc_id" {
  description = "VPC ID"
  type        = string
}

variable "subnet_ids" {
  description = "Subnet IDs for redis subnet group"
  type        = list(string)
}

variable "allowed_cidr" {
  description = "CIDR allowed to connect to redis"
  type        = string
}

variable "port" {
  description = "Redis port"
  type        = number
  default     = 6379
}

variable "node_type" {
  description = "Redis node type"
  type        = string
  default     = "cache.t4g.micro"
}

variable "engine_version" {
  description = "Redis engine version"
  type        = string
  default     = "7.1"
}

variable "parameter_group_name" {
  description = "Redis parameter group"
  type        = string
  default     = "default.redis7"
}

