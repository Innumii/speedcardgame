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

resource "aws_security_group" "alb" {
  name        = "${var.service_name}-alb-sg"
  description = "Security group for ${var.service_name} ALB"
  vpc_id      = data.aws_vpc.default.id

  ingress {
    from_port   = 80
    to_port     = 80
    protocol    = "tcp"
    cidr_blocks = ["0.0.0.0/0"]
  }

  egress {
    from_port   = 0
    to_port     = 0
    protocol    = "-1"
    cidr_blocks = ["0.0.0.0/0"]
  }
}

resource "aws_lb" "this" {
  name               = "${var.service_name}-alb"
  internal           = false
  load_balancer_type = "application"
  security_groups    = [aws_security_group.alb.id]
  subnets            = data.aws_subnets.default.ids
}

resource "aws_lb_target_group" "this" {
  name        = "${var.service_name}-tg"
  port        = 8080
  protocol    = "HTTP"
  vpc_id      = data.aws_vpc.default.id
  target_type = "ip"

  health_check {
    path    = "/"
    matcher = "200-499"
  }
}

resource "aws_lb_listener" "http" {
  load_balancer_arn = aws_lb.this.arn
  port              = 80
  protocol          = "HTTP"

  default_action {
    type             = "forward"
    target_group_arn = aws_lb_target_group.this.arn
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
  data_outputs                 = var.use_managed_data_stack ? data.terraform_remote_state.data[0].outputs : {}
  postgres_host                = var.postgres_host != null ? var.postgres_host : try(local.data_outputs.cards_postgres_endpoint, null)
  postgres_user                = var.postgres_user != null ? var.postgres_user : try(local.data_outputs.cards_postgres_user, null)
  postgres_password            = var.postgres_password != null ? var.postgres_password : try(local.data_outputs.cards_postgres_password, null)
  postgres_password_secret_arn = var.postgres_password_secret_arn != null ? var.postgres_password_secret_arn : try(local.data_outputs.cards_runtime_secret_arn, null)
  postgres_db                  = var.postgres_db != null ? var.postgres_db : try(local.data_outputs.cards_postgres_db, null)
  postgres_port                = var.postgres_port != null ? var.postgres_port : try(local.data_outputs.cards_postgres_port, null)
  redis_host                   = var.redis_host != null ? var.redis_host : try(local.data_outputs.auth_redis_endpoint, null)
  redis_port                   = var.redis_port != null ? var.redis_port : try(local.data_outputs.auth_redis_port, null)
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
  target_group_arn      = aws_lb_target_group.this.arn
  alb_security_group_id = aws_security_group.alb.id

  task_secret_arns = local.postgres_password_secret_arn != null ? toset([local.postgres_password_secret_arn]) : toset([])

  secrets = local.postgres_password_secret_arn != null ? {
    POSTGRES_PASSWORD = "${local.postgres_password_secret_arn}:POSTGRES_PASSWORD::"
  } : {}

  environment = merge(
    {
      POSTGRES_HOST      = local.postgres_host
      POSTGRES_USER      = local.postgres_user
      POSTGRES_DB        = local.postgres_db
      POSTGRES_PORT      = tostring(local.postgres_port)
      POSTGRES_SSLMODE   = var.postgres_sslmode
      POSTGRES_TIMEZONE  = var.postgres_timezone
      REDIS_HOST         = local.redis_host
      REDIS_PORT         = tostring(local.redis_port)
      CARDS_SERVICE_HOST = var.cards_service_host
      CARDS_SERVICE_PORT = tostring(var.cards_service_port)
    },
    local.postgres_password_secret_arn == null && local.postgres_password != null ? {
      POSTGRES_PASSWORD = local.postgres_password
    } : {}
  )
}
