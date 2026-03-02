terraform {
  required_version = ">= 1.5.0"

  required_providers {
    aws = {
      source  = "hashicorp/aws"
      version = "~> 5.0"
    }
    random = {
      source  = "hashicorp/random"
      version = "~> 3.0"
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

# Route tables in the VPC (used for S3 gateway endpoint)
data "aws_route_tables" "vpc" {
  filter {
    name   = "vpc-id"
    values = [data.aws_vpc.default.id]
  }
}

# Security group for interface endpoints (allow HTTPS egress)
resource "aws_security_group" "ecr_endpoint_sg" {
  name   = "ecr-endpoint-sg"
  vpc_id = data.aws_vpc.default.id

  # Allow tasks in the VPC to connect to the interface endpoints on HTTPS
  ingress {
    from_port   = 443
    to_port     = 443
    protocol    = "tcp"
    cidr_blocks = [data.aws_vpc.default.cidr_block]
  }

  egress {
    from_port   = 443
    to_port     = 443
    protocol    = "tcp"
    cidr_blocks = ["0.0.0.0/0"]
  }

  tags = {
    Name = "ecr-endpoint-sg"
  }
}

# Interface endpoints for ECR (API + DKR) so tasks in private subnets can pull images without NAT
resource "aws_vpc_endpoint" "ecr_api" {
  vpc_id              = data.aws_vpc.default.id
  service_name        = "com.amazonaws.${var.aws_region}.ecr.api"
  vpc_endpoint_type   = "Interface"
  subnet_ids          = data.aws_subnets.default.ids
  security_group_ids  = [aws_security_group.ecr_endpoint_sg.id]
  private_dns_enabled = true

  tags = {
    Name = "ecr-api-endpoint"
  }
}

resource "aws_vpc_endpoint" "ecr_dkr" {
  vpc_id              = data.aws_vpc.default.id
  service_name        = "com.amazonaws.${var.aws_region}.ecr.dkr"
  vpc_endpoint_type   = "Interface"
  subnet_ids          = data.aws_subnets.default.ids
  security_group_ids  = [aws_security_group.ecr_endpoint_sg.id]
  private_dns_enabled = true

  tags = {
    Name = "ecr-dkr-endpoint"
  }
}

# Gateway endpoint for S3 (used by ECR for image layers)
resource "aws_vpc_endpoint" "s3" {
  vpc_id            = data.aws_vpc.default.id
  service_name      = "com.amazonaws.${var.aws_region}.s3"
  vpc_endpoint_type = "Gateway"
  route_table_ids   = data.aws_route_tables.vpc.ids
}


# Dedicated security groups for services
resource "aws_security_group" "auth_service" {
  name   = "auth-service-sg"
  vpc_id = data.aws_vpc.default.id
  tags   = { Name = "auth-service-sg" }
}

resource "aws_security_group" "cards_service" {
  name   = "cards-service-sg"
  vpc_id = data.aws_vpc.default.id
  tags   = { Name = "cards-service-sg" }
}

# Dedicated security groups for databases
resource "aws_security_group" "auth_db" {
  name   = "auth-db-sg"
  vpc_id = data.aws_vpc.default.id
  tags   = { Name = "auth-db-sg" }
}

resource "aws_security_group" "cards_db" {
  name   = "cards-db-sg"
  vpc_id = data.aws_vpc.default.id
  tags   = { Name = "cards-db-sg" }
}

# Allow ECS tasks in this VPC to access each DB port
resource "aws_security_group_rule" "allow_auth_service" {
  type              = "ingress"
  from_port         = var.auth_postgres_port
  to_port           = var.auth_postgres_port
  protocol          = "tcp"
  security_group_id = aws_security_group.auth_db.id
  cidr_blocks       = [data.aws_vpc.default.cidr_block]
}

resource "aws_security_group_rule" "allow_cards_service" {
  type              = "ingress"
  from_port         = var.cards_postgres_port
  to_port           = var.cards_postgres_port
  protocol          = "tcp"
  security_group_id = aws_security_group.cards_db.id
  cidr_blocks       = [data.aws_vpc.default.cidr_block]
}

resource "random_password" "auth_postgres" {
  count   = var.auth_postgres_password == null ? 1 : 0
  length  = 24
  special = true
}

resource "random_password" "cards_postgres" {
  count   = var.cards_postgres_password == null ? 1 : 0
  length  = 24
  special = true
}

locals {
  auth_postgres_password_resolved  = coalesce(var.auth_postgres_password, try(random_password.auth_postgres[0].result, null))
  cards_postgres_password_resolved = coalesce(var.cards_postgres_password, try(random_password.cards_postgres[0].result, null))
}

module "auth_postgres" {
  source                 = "../modules/rds-postgres"
  identifier             = var.auth_postgres_identifier
  vpc_id                 = data.aws_vpc.default.id
  subnet_ids             = data.aws_subnets.default.ids
  db_name                = var.auth_postgres_db
  username               = var.auth_postgres_user
  password               = local.auth_postgres_password_resolved
  port                   = var.auth_postgres_port
  instance_class         = var.auth_postgres_instance_class
  allocated_storage      = var.auth_postgres_allocated_storage
  vpc_security_group_ids = [aws_security_group.auth_db.id]
}

module "cards_postgres" {
  source                 = "../modules/rds-postgres"
  identifier             = var.cards_postgres_identifier
  vpc_id                 = data.aws_vpc.default.id
  subnet_ids             = data.aws_subnets.default.ids
  db_name                = var.cards_postgres_db
  username               = var.cards_postgres_user
  password               = local.cards_postgres_password_resolved
  port                   = var.cards_postgres_port
  instance_class         = var.cards_postgres_instance_class
  allocated_storage      = var.cards_postgres_allocated_storage
  vpc_security_group_ids = [aws_security_group.cards_db.id]
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

# 3. VPC ENDPOINTS (The Fix for Connection Issues)
# Endpoint for Secrets Manager (to get GHCR Token)
resource "aws_vpc_endpoint" "secretsmanager" {
  vpc_id              = data.aws_vpc.default.id
  service_name        = "com.amazonaws.${var.aws_region}.secretsmanager"
  vpc_endpoint_type   = "Interface"
  private_dns_enabled = true
  subnet_ids          = data.aws_subnets.default.ids
  security_group_ids  = [aws_security_group.ecr_endpoint_sg.id]
}

# Endpoint for CloudWatch Logs (to fix the Logger Args error)
resource "aws_vpc_endpoint" "logs" {
  vpc_id              = data.aws_vpc.default.id
  service_name        = "com.amazonaws.${var.aws_region}.logs"
  vpc_endpoint_type   = "Interface"
  private_dns_enabled = true
  subnet_ids          = data.aws_subnets.default.ids
  security_group_ids  = [aws_security_group.ecr_endpoint_sg.id]
}

resource "null_resource" "skip_secrets_manager" {
  count = var.skip_secrets_manager ? 1 : 0
}

locals {
  cards_database_url = "postgres://${var.cards_postgres_user}:${local.cards_postgres_password_resolved}@${module.cards_postgres.endpoint}:${module.cards_postgres.port}/${var.cards_postgres_db}?sslmode=disable"
}

# Secrets
module "secrets" {
  source                   = "../modules/secrets"
  github_username          = var.github_username
  github_token             = var.github_token
  existing_ghcr_secret_arn = var.existing_ghcr_secret_arn
  auth_postgres_password   = local.auth_postgres_password_resolved
  cards_database_url       = local.cards_database_url
  count                    = var.skip_secrets_manager ? 0 : 1
}
