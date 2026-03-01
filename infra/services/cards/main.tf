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
  data_outputs            = var.use_managed_data_stack ? data.terraform_remote_state.data[0].outputs : {}
  database_url            = coalesce(var.database_url, try(local.data_outputs.cards_database_url, null))
  database_url_secret_arn = coalesce(var.database_url_secret_arn, try(local.data_outputs.cards_runtime_secret_arn, null))
}

module "service" {
  source = "../../modules/ecs-public-service"

  aws_region     = var.aws_region
  service_name   = var.service_name
  vpc_id         = data.aws_vpc.default.id
  subnet_ids     = data.aws_subnets.default.ids
  container_port = 8080
  image_tag      = var.image_tag
  cpu            = var.cpu
  memory         = var.memory
  desired_count  = 1

  task_secret_arns = local.database_url_secret_arn != null ? toset([local.database_url_secret_arn]) : toset([])

  secrets = local.database_url_secret_arn != null ? {
    DATABASE_URL = "${local.database_url_secret_arn}:DATABASE_URL::"
  } : {}

  environment = merge(
    {
      PORT = "8080"
    },
    local.database_url_secret_arn == null && local.database_url != null ? {
      DATABASE_URL = local.database_url
    } : {}
  )
}
