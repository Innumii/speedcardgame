terraform {
  required_version = ">= 1.5.0"

  required_providers {
    aws = {
      source  = "hashicorp/aws"
      version = "~> 5.0"
    }
  }

  backend "local" {
    path = "terraform.tfstate"
  }
}

provider "aws" {
  region = var.aws_region
}

data "aws_vpc" "default" {
  default = true
}

data "aws_subnets" "default" {
  filter {
    name   = "vpc-id"
    values = [data.aws_vpc.default.id]
  }
}

module "auth_postgres" {
  source = "../modules/rds-postgres"

  identifier        = var.auth_postgres_identifier
  vpc_id            = data.aws_vpc.default.id
  subnet_ids        = data.aws_subnets.default.ids
  allowed_cidr      = data.aws_vpc.default.cidr_block
  db_name           = var.auth_postgres_db
  username          = var.auth_postgres_user
  password          = var.auth_postgres_password
  port              = var.auth_postgres_port
  instance_class    = var.auth_postgres_instance_class
  allocated_storage = var.auth_postgres_allocated_storage
}

module "cards_postgres" {
  source = "../modules/rds-postgres"

  identifier        = var.cards_postgres_identifier
  vpc_id            = data.aws_vpc.default.id
  subnet_ids        = data.aws_subnets.default.ids
  allowed_cidr      = data.aws_vpc.default.cidr_block
  db_name           = var.cards_postgres_db
  username          = var.cards_postgres_user
  password          = var.cards_postgres_password
  port              = var.cards_postgres_port
  instance_class    = var.cards_postgres_instance_class
  allocated_storage = var.cards_postgres_allocated_storage
}

module "auth_redis" {
  source = "../modules/elasticache-redis"

  cluster_id           = var.auth_redis_cluster_id
  vpc_id               = data.aws_vpc.default.id
  subnet_ids           = data.aws_subnets.default.ids
  allowed_cidr         = data.aws_vpc.default.cidr_block
  port                 = var.auth_redis_port
  node_type            = var.auth_redis_node_type
  engine_version       = var.auth_redis_engine_version
  parameter_group_name = var.auth_redis_parameter_group_name
}
