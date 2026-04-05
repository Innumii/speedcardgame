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

  default_tags {
    tags = {
      environment = var.environment
      managed-by  = "terraform"
    }
  }
}

locals {
  route53_zone_name = "${trimsuffix(var.base_domain, ".")}."
  api_domain_name   = "api.${trimsuffix(var.base_domain, ".")}"
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

data "aws_route53_zone" "primary" {
  name         = local.route53_zone_name
  private_zone = false
}

resource "aws_security_group" "alb" {
  name        = "${var.alb_name}-sg"
  description = "Security group for shared ALB"
  vpc_id      = data.aws_vpc.default.id

  ingress {
    from_port   = 443
    to_port     = 443
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

resource "aws_lb" "shared" {
  name               = var.alb_name
  internal           = false
  load_balancer_type = "application"
  security_groups    = [aws_security_group.alb.id]
  subnets            = data.aws_subnets.default.ids
}

resource "aws_lb_target_group" "auth" {
  name        = var.auth_target_group_name
  port        = 8080
  protocol    = "HTTPS"
  vpc_id      = data.aws_vpc.default.id
  target_type = "ip"

  health_check {
    protocol = "HTTPS"
    path     = "/health"
    matcher  = "200-299"
  }
}

resource "aws_lb_target_group" "cards" {
  name        = var.cards_target_group_name
  port        = 8080
  protocol    = "HTTPS"
  vpc_id      = data.aws_vpc.default.id
  target_type = "ip"

  health_check {
    protocol = "HTTPS"
    path     = "/cards/health"
    matcher  = "200-299"
  }
}

resource "aws_lb_listener" "https" {
  count             = var.enable_https_listener ? 1 : 0
  load_balancer_arn = aws_lb.shared.arn
  port              = 443
  protocol          = "HTTPS"
  ssl_policy        = "ELBSecurityPolicy-TLS13-1-2-2021-06"
  certificate_arn   = var.acm_certificate_arn

  default_action {
    type = "fixed-response"

    fixed_response {
      content_type = "application/json"
      message_body = "{\"message\":\"Not Found\"}"
      status_code  = "404"
    }
  }
}

resource "aws_lb_listener_rule" "auth" {
  listener_arn = aws_lb_listener.https[0].arn
  priority     = 100

  action {
    type             = "forward"
    target_group_arn = aws_lb_target_group.auth.arn
  }

  condition {
    path_pattern {
      values = ["/auth/*"]
    }
  }
}

resource "aws_lb_listener_rule" "cards" {
  listener_arn = aws_lb_listener.https[0].arn
  priority     = 200

  action {
    type             = "forward"
    target_group_arn = aws_lb_target_group.cards.arn
  }

  condition {
    path_pattern {
      values = ["/cards/*"]
    }
  }
}

resource "aws_route53_record" "api" {
  zone_id = data.aws_route53_zone.primary.zone_id
  name    = local.api_domain_name
  type    = "A"

  alias {
    evaluate_target_health = true
    name                   = aws_lb.shared.dns_name
    zone_id                = aws_lb.shared.zone_id
  }
}