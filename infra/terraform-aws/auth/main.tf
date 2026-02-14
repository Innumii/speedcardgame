terraform {
  required_version = ">= 1.5.0"
  required_providers {
    aws = {
      source  = "hashicorp/aws"
      version = "~> 5.0"
    }
  }
}

provider "aws" {
  region = var.aws_region
}

data "aws_caller_identity" "current" {}

data "aws_availability_zones" "available" {
  state = "available"
}

data "aws_ami" "amazon_linux_2" {
  most_recent = true
  owners      = ["amazon"]

  filter {
    name   = "name"
    values = ["amzn2-ami-hvm-*-x86_64-gp2"]
  }

  filter {
    name   = "virtualization-type"
    values = ["hvm"]
  }
}

locals {
  project_root = abspath("${path.module}/../../..")
  auth_dir     = "${local.project_root}/auth"
  ecr_registry = "${data.aws_caller_identity.current.account_id}.dkr.ecr.${var.aws_region}.amazonaws.com"
}

resource "aws_vpc" "main" {
  cidr_block           = var.vpc_cidr
  enable_dns_hostnames = true
  enable_dns_support   = true

  tags = merge(var.tags, { Name = "${var.project_name}-vpc" })
}

resource "aws_internet_gateway" "main" {
  vpc_id = aws_vpc.main.id

  tags = merge(var.tags, { Name = "${var.project_name}-igw" })
}

resource "aws_subnet" "public" {
  count                   = 2
  vpc_id                  = aws_vpc.main.id
  cidr_block              = cidrsubnet(var.vpc_cidr, 8, count.index)
  availability_zone       = data.aws_availability_zones.available.names[count.index]
  map_public_ip_on_launch = true

  tags = merge(var.tags, { Name = "${var.project_name}-public-${count.index + 1}" })
}

resource "aws_route_table" "public" {
  vpc_id = aws_vpc.main.id

  route {
    cidr_block = "0.0.0.0/0"
    gateway_id = aws_internet_gateway.main.id
  }

  tags = merge(var.tags, { Name = "${var.project_name}-public-rt" })
}

resource "aws_route_table_association" "public" {
  count          = length(aws_subnet.public)
  subnet_id      = aws_subnet.public[count.index].id
  route_table_id = aws_route_table.public.id
}

resource "aws_security_group" "alb" {
  name        = "${var.project_name}-alb-sg"
  description = "ALB security group"
  vpc_id      = aws_vpc.main.id

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

  tags = merge(var.tags, { Name = "${var.project_name}-alb-sg" })
}

resource "aws_security_group" "instance" {
  name        = "${var.project_name}-instance-sg"
  description = "EC2 security group"
  vpc_id      = aws_vpc.main.id

  ingress {
    from_port   = 22
    to_port     = 22
    protocol    = "tcp"
    cidr_blocks = [var.ssh_cidr]
  }

  ingress {
    from_port       = var.auth_app_port
    to_port         = var.auth_app_port
    protocol        = "tcp"
    security_groups = [aws_security_group.alb.id]
  }

  egress {
    from_port   = 0
    to_port     = 0
    protocol    = "-1"
    cidr_blocks = ["0.0.0.0/0"]
  }

  tags = merge(var.tags, { Name = "${var.project_name}-instance-sg" })
}

resource "aws_iam_role" "ec2" {
  name = "${var.project_name}-ec2-role"

  assume_role_policy = jsonencode({
    Version = "2012-10-17"
    Statement = [
      {
        Effect = "Allow"
        Principal = { Service = "ec2.amazonaws.com" }
        Action = "sts:AssumeRole"
      }
    ]
  })
}

resource "aws_iam_role_policy_attachment" "ecr_readonly" {
  role       = aws_iam_role.ec2.name
  policy_arn = "arn:aws:iam::aws:policy/AmazonEC2ContainerRegistryReadOnly"
}

resource "aws_iam_instance_profile" "ec2" {
  name = "${var.project_name}-ec2-profile"
  role = aws_iam_role.ec2.name
}

resource "aws_ecr_repository" "auth" {
  name                 = "${var.project_name}-auth"
  image_tag_mutability = "MUTABLE"
}

resource "aws_instance" "auth" {
  ami                    = data.aws_ami.amazon_linux_2.id
  instance_type          = var.instance_type
  subnet_id              = aws_subnet.public[0].id
  vpc_security_group_ids = [aws_security_group.instance.id]
  key_name               = var.key_name
  iam_instance_profile   = aws_iam_instance_profile.ec2.name

  user_data = templatefile("${path.module}/user_data.sh.tftpl", {
    region             = var.aws_region
    account_id         = data.aws_caller_identity.current.account_id
    auth_image         = "${aws_ecr_repository.auth.repository_url}:${var.image_tag}"
    auth_app_port      = var.auth_app_port
    auth_postgres_user = var.auth_postgres_user
    auth_postgres_pass = var.auth_postgres_password
    auth_postgres_db   = var.auth_postgres_db
    auth_postgres_port = var.auth_postgres_port
    auth_postgres_ssl  = var.auth_postgres_sslmode
    auth_postgres_tz   = var.auth_postgres_timezone
    auth_redis_port    = var.auth_redis_port
  })

  tags = merge(var.tags, { Name = "${var.project_name}-ec2" })
}

resource "aws_lb" "public" {
  name               = "${var.project_name}-alb"
  internal           = false
  load_balancer_type = "application"
  security_groups    = [aws_security_group.alb.id]
  subnets            = aws_subnet.public[*].id

  tags = merge(var.tags, { Name = "${var.project_name}-alb" })
}

resource "aws_lb_target_group" "auth" {
  name        = "${var.project_name}-auth-tg"
  port        = var.auth_app_port
  protocol    = "HTTP"
  vpc_id      = aws_vpc.main.id
  target_type = "instance"

  health_check {
    path = var.auth_healthcheck_path
  }
}

resource "aws_lb_target_group_attachment" "auth" {
  target_group_arn = aws_lb_target_group.auth.arn
  target_id        = aws_instance.auth.id
  port             = var.auth_app_port
}

resource "aws_lb_listener" "http" {
  load_balancer_arn = aws_lb.public.arn
  port              = 80
  protocol          = "HTTP"

  default_action {
    type             = "forward"
    target_group_arn = aws_lb_target_group.auth.arn
  }
}

resource "null_resource" "build_auth_image" {
  count = var.enable_local_image_build ? 1 : 0

  triggers = {
    image_tag = var.image_tag
    repo_url  = aws_ecr_repository.auth.repository_url
  }

  provisioner "local-exec" {
    interpreter = ["PowerShell", "-Command"]
    command     = "${join("; ", [
      "$ErrorActionPreference = 'Stop'",
      "aws ecr get-login-password --region ${var.aws_region} | docker login --username AWS --password-stdin ${local.ecr_registry}",
      "docker build -t ${aws_ecr_repository.auth.repository_url}:${var.image_tag} ${local.auth_dir}",
      "docker push ${aws_ecr_repository.auth.repository_url}:${var.image_tag}"
    ])}"
  }
}
