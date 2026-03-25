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

locals {
  use_alb_remote_state  = var.use_managed_alb_stack && fileexists(var.alb_stack_state_path)
  use_data_remote_state = var.use_managed_data_stack && fileexists(var.data_stack_state_path)
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

data "terraform_remote_state" "alb" {
  count   = local.use_alb_remote_state ? 1 : 0
  backend = "local"
  config = {
    path = var.alb_stack_state_path
  }
}

data "terraform_remote_state" "data" {
  count   = local.use_data_remote_state ? 1 : 0
  backend = "local"
  config = {
    path = var.data_stack_state_path
  }
}

locals {
  alb_outputs                  = local.use_alb_remote_state ? data.terraform_remote_state.alb[0].outputs : {}
  alb_security_group_id        = var.alb_security_group_id != null ? var.alb_security_group_id : try(local.alb_outputs.alb_security_group_id, null)
  cards_target_group_arn       = var.target_group_arn != null ? var.target_group_arn : try(local.alb_outputs.cards_target_group_arn, null)
  data_outputs                 = local.use_data_remote_state ? data.terraform_remote_state.data[0].outputs : {}
  postgres_host                = var.postgres_host != null ? var.postgres_host : try(local.data_outputs.cards_postgres_endpoint, null)
  postgres_user                = var.postgres_user != null ? var.postgres_user : try(local.data_outputs.cards_postgres_user, null)
  postgres_password            = var.postgres_password != null ? var.postgres_password : try(local.data_outputs.cards_postgres_password, null)
  postgres_password_secret_arn = var.postgres_password_secret_arn != null ? var.postgres_password_secret_arn : try(local.data_outputs.cards_runtime_secret_arn, null)
  postgres_db                  = var.postgres_db != null ? var.postgres_db : try(local.data_outputs.cards_postgres_db, null)
  postgres_port                = var.postgres_port != null ? var.postgres_port : try(local.data_outputs.cards_postgres_port, null)
  redis_host                   = var.redis_host != null ? var.redis_host : try(local.data_outputs.auth_redis_endpoint, null)
  redis_port                   = var.redis_port != null ? var.redis_port : try(local.data_outputs.auth_redis_port, null)
  cards_database_url           = "postgres://${local.postgres_user}:${local.postgres_password}@${local.postgres_host}:${local.postgres_port}/${local.postgres_db}?sslmode=${var.postgres_sslmode}"
}


module "service" {
  source = "../../modules/ecs-public-service"

  # GitHub Container Registry Config
  github_username     = var.github_username
  github_repo_name    = var.github_repo_name
  ghcr_pat_secret_arn = var.ghcr_pat_secret_arn

  aws_region            = var.aws_region
  service_name          = var.service_name
  vpc_id                = data.aws_vpc.default.id
  subnet_ids            = data.aws_subnets.default.ids
  container_port        = 8080
  image_tag             = var.image_tag
  image_repo            = var.image_repo
  cpu                   = var.cpu
  memory                = var.memory
  assign_public_ip      = var.assign_public_ip
  desired_count         = 1
  target_group_arn      = local.cards_target_group_arn
  alb_security_group_id = local.alb_security_group_id

  task_secret_arns = local.postgres_password_secret_arn != null ? toset([local.postgres_password_secret_arn]) : toset([])

  secrets = local.postgres_password_secret_arn != null ? {
    DATABASE_URL          = "${local.postgres_password_secret_arn}:DATABASE_URL::"
    STRIPE_SECRET_KEY     = "${local.postgres_password_secret_arn}:STRIPE_SECRET_KEY::"
    STRIPE_WEBHOOK_SECRET = "${local.postgres_password_secret_arn}:STRIPE_WEBHOOK_SECRET::"
  } : {}

  environment = merge(
    {
      PORT                     = tostring(8080)
      POSTGRES_HOST            = local.postgres_host
      POSTGRES_USER            = local.postgres_user
      POSTGRES_DB              = local.postgres_db
      POSTGRES_PORT            = tostring(local.postgres_port)
      POSTGRES_SSLMODE         = var.postgres_sslmode
      POSTGRES_TIMEZONE        = var.postgres_timezone
      DATABASE_URL             = local.cards_database_url
      REDIS_HOST               = local.redis_host
      REDIS_PORT               = tostring(local.redis_port)
      CARDS_SERVICE_HOST       = var.cards_service_host
      CARDS_SERVICE_PORT       = tostring(var.cards_service_port)
      DEBUG_LOG_ENABLED        = tostring(var.debug_log_enabled)
      HTTP_REQUEST_LOG_ENABLED = tostring(var.http_request_log_enabled)
    },
    {}
  )
}
