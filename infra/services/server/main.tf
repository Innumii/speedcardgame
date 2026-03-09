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

data "aws_secretsmanager_secret" "game_tls" {
  name = "game-server-tls"
}

resource "aws_lb" "this" {
  name               = "${var.service_name}-nlb"
  internal           = false
  load_balancer_type = "network"
  subnets            = data.aws_subnets.default.ids
}

resource "aws_lb_target_group" "this" {
  name        = "${var.service_name}-tg"
  port        = 4000
  protocol    = "TCP"
  vpc_id      = data.aws_vpc.default.id
  target_type = "ip"
}

resource "aws_lb_listener" "tcp" {
  load_balancer_arn = aws_lb.this.arn
  port              = 4000
  protocol          = "TCP"

  default_action {
    type             = "forward"
    target_group_arn = aws_lb_target_group.this.arn
  }
}

module "service" {
  source = "../../modules/ecs-public-service"

  github_username     = var.github_username
  github_repo_name    = var.github_repo_name
  ghcr_pat_secret_arn = var.ghcr_pat_secret_arn

  aws_region       = var.aws_region
  service_name     = var.service_name
  vpc_id           = data.aws_vpc.default.id
  subnet_ids       = data.aws_subnets.default.ids
  container_port   = 4000
  image_tag        = var.image_tag
  image_repo       = var.image_repo
  cpu              = var.cpu
  memory           = var.memory
  assign_public_ip = var.assign_public_ip
  desired_count    = 1
  target_group_arn = aws_lb_target_group.this.arn

  secrets = {
    TLS_CERT = "${data.aws_secretsmanager_secret.game_tls.arn}:cert::"
    TLS_KEY  = "${data.aws_secretsmanager_secret.game_tls.arn}:key::"
  }

  task_secret_arns = [
    data.aws_secretsmanager_secret.game_tls.arn
  ]
}
