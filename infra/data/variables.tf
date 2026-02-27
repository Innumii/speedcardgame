variable "aws_region" {
  description = "AWS region"
  type        = string
  default     = "us-east-1"
}

variable "auth_postgres_identifier" {
  description = "RDS identifier for auth database"
  type        = string
  default     = "speedcardgame-auth-db"
}

variable "auth_postgres_db" {
  description = "Auth postgres DB name"
  type        = string
  default     = "authdb"
}

variable "auth_postgres_user" {
  description = "Auth postgres username"
  type        = string
  default     = "authuser"
}

variable "auth_postgres_password" {
  description = "Auth postgres password"
  type        = string
  sensitive   = true
}

variable "auth_postgres_port" {
  description = "Auth postgres port"
  type        = number
  default     = 5432
}

variable "auth_postgres_instance_class" {
  description = "Auth postgres instance class"
  type        = string
  default     = "db.t4g.micro"
}

variable "auth_postgres_allocated_storage" {
  description = "Auth postgres storage in GB"
  type        = number
  default     = 20
}

variable "cards_postgres_identifier" {
  description = "RDS identifier for cards database"
  type        = string
  default     = "speedcardgame-cards-db"
}

variable "cards_postgres_db" {
  description = "Cards postgres DB name"
  type        = string
  default     = "cardsdb"
}

variable "cards_postgres_user" {
  description = "Cards postgres username"
  type        = string
  default     = "cardsuser"
}

variable "cards_postgres_password" {
  description = "Cards postgres password"
  type        = string
  sensitive   = true
}

variable "cards_postgres_port" {
  description = "Cards postgres port"
  type        = number
  default     = 5432
}

variable "cards_postgres_instance_class" {
  description = "Cards postgres instance class"
  type        = string
  default     = "db.t4g.micro"
}

variable "cards_postgres_allocated_storage" {
  description = "Cards postgres storage in GB"
  type        = number
  default     = 20
}

variable "auth_redis_cluster_id" {
  description = "ElastiCache cluster id for auth"
  type        = string
  default     = "speedcardgame-auth-redis"
}

variable "auth_redis_port" {
  description = "Redis port"
  type        = number
  default     = 6379
}

variable "auth_redis_node_type" {
  description = "Redis node type"
  type        = string
  default     = "cache.t4g.micro"
}

variable "auth_redis_engine_version" {
  description = "Redis engine version"
  type        = string
  default     = "7.1"
}

variable "auth_redis_parameter_group_name" {
  description = "Redis parameter group name"
  type        = string
  default     = "default.redis7"
}
