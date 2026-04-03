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
  use_alb_remote_state = var.use_managed_alb_stack && fileexists(var.alb_stack_state_path)
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

locals {
  alb_outputs                  = local.use_alb_remote_state ? data.terraform_remote_state.alb[0].outputs : {}
  route53_zone_name            = "${trimsuffix(var.base_domain, ".")}."
  game_domain_name             = "game.${trimsuffix(var.base_domain, ".")}"
  cards_service_base_url_local = var.cards_service_base_url != null ? var.cards_service_base_url : try(local.alb_outputs.api_base_url, null)
  cards_service_host = var.cards_service_host != null ? var.cards_service_host : (
    local.cards_service_base_url_local != null ? regexreplace(local.cards_service_base_url_local, "^https?://", "") : try(local.alb_outputs.api_domain_name, null)
  )
  cards_service_port = var.cards_service_port != null ? var.cards_service_port : (
    local.cards_service_base_url_local != null ? (
      startswith(local.cards_service_base_url_local, "https://") ? 443 : 80
    ) : 443
  )
}

data "aws_route53_zone" "primary" {
  name         = local.route53_zone_name
  private_zone = false
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

resource "aws_route53_record" "game" {
  zone_id = data.aws_route53_zone.primary.zone_id
  name    = local.game_domain_name
  type    = "A"

  alias {
    evaluate_target_health = true
    name                   = aws_lb.this.dns_name
    zone_id                = aws_lb.this.zone_id
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

  environment = merge(
    local.cards_service_host != null ? {
      USE_AWS_SERVICES       = "true"
      TLS_VERIFY_CERTS       = "true"
      CARDS_SERVICE_HOST     = local.cards_service_host
      CARDS_SERVICE_PORT     = tostring(local.cards_service_port)
      AWS_CARDS_SERVICE_HOST = local.cards_service_host
      AWS_CARDS_SERVICE_PORT = tostring(local.cards_service_port)
    } : {},
    {}
  )
}
