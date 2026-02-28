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

data "terraform_remote_state" "data" {
  count   = var.use_managed_data_stack ? 1 : 0
  backend = "local"
  config = {
    path = var.data_stack_state_path
  }
}

locals {
  data_outputs      = var.use_managed_data_stack ? data.terraform_remote_state.data[0].outputs : {}
  postgres_host     = coalesce(var.postgres_host, try(local.data_outputs.auth_postgres_endpoint, null))
  postgres_user     = coalesce(var.postgres_user, try(local.data_outputs.auth_postgres_user, null))
  postgres_password = coalesce(var.postgres_password, try(local.data_outputs.auth_postgres_password, null))
  postgres_db       = coalesce(var.postgres_db, try(local.data_outputs.auth_postgres_db, null))
  postgres_port     = coalesce(var.postgres_port, try(local.data_outputs.auth_postgres_port, null))
  redis_host        = coalesce(var.redis_host, try(local.data_outputs.auth_redis_endpoint, null))
  redis_port        = coalesce(var.redis_port, try(local.data_outputs.auth_redis_port, null))
}

module "service" {
  source = "../../modules/ecs-public-service"

  # GitHub Container Registry Config
  github_username     = var.github_username
  github_repo_name    = var.github_repo_name
  ghcr_pat_secret_arn = var.ghcr_pat_secret_arn

  aws_region     = var.aws_region
  service_name   = var.service_name
  vpc_id         = data.aws_vpc.default.id
  subnet_ids     = data.aws_subnets.default.ids
  container_port = 8080
  image_tag      = var.image_tag
  image_repo     = var.image_repo
  cpu            = var.cpu
  memory         = var.memory
  desired_count  = 1

  environment = {
    POSTGRES_HOST      = local.postgres_host
    POSTGRES_USER      = local.postgres_user
    POSTGRES_PASSWORD  = local.postgres_password
    POSTGRES_DB        = local.postgres_db
    POSTGRES_PORT      = tostring(local.postgres_port)
    POSTGRES_SSLMODE   = var.postgres_sslmode
    POSTGRES_TIMEZONE  = var.postgres_timezone
    REDIS_HOST         = local.redis_host
    REDIS_PORT         = tostring(local.redis_port)
    CARDS_SERVICE_HOST = var.cards_service_host
    CARDS_SERVICE_PORT = tostring(var.cards_service_port)
  }
}