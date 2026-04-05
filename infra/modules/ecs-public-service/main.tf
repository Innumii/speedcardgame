terraform {
  required_providers {
    aws = {
      source = "hashicorp/aws"
    }
  }
}

############################
# CloudWatch Logs
############################

resource "aws_cloudwatch_log_group" "this" {
  name              = "/ecs/${var.service_name}"
  retention_in_days = 14
}

############################
# IAM Roles
############################

data "aws_iam_policy_document" "ecs_task_assume_role" {
  statement {
    actions = ["sts:AssumeRole"]

    principals {
      type        = "Service"
      identifiers = ["ecs-tasks.amazonaws.com"]
    }
  }
}

resource "aws_iam_role" "task_execution" {
  name               = "${var.service_name}-task-exec-role"
  assume_role_policy = data.aws_iam_policy_document.ecs_task_assume_role.json
}

resource "aws_iam_role_policy_attachment" "task_execution_policy" {
  role       = aws_iam_role.task_execution.name
  policy_arn = "arn:aws:iam::aws:policy/service-role/AmazonECSTaskExecutionRolePolicy"
}

resource "aws_iam_role_policy" "read_ghcr_secret" {
  name = "${var.service_name}-ghcr-secret-policy"
  role = aws_iam_role.task_execution.id

  policy = jsonencode({
    Version = "2012-10-17"
    Statement = [
      {
        Action   = ["secretsmanager:GetSecretValue"]
        Effect   = "Allow"
        Resource = distinct(concat([var.ghcr_pat_secret_arn], tolist(var.task_secret_arns)))
      }
    ]
  })
}

############################
# ECS Cluster
############################

resource "aws_ecs_cluster" "this" {
  name = "${var.service_name}-cluster"
}

############################
# Security Group
############################

resource "aws_security_group" "this" {
  name        = "${var.service_name}-sg"
  description = "Security group for ${var.service_name}"
  vpc_id      = var.vpc_id

  ingress {
    from_port       = var.container_port
    to_port         = var.container_port
    protocol        = "tcp"
    security_groups = var.alb_security_group_id != null ? [var.alb_security_group_id] : null
    cidr_blocks     = var.alb_security_group_id == null ? ["0.0.0.0/0"] : null
  }

  egress {
    from_port   = 0
    to_port     = 0
    protocol    = "-1"
    cidr_blocks = ["0.0.0.0/0"]
  }
}

############################
# ECS Task Definition
############################

resource "aws_ecs_task_definition" "this" {
  family                   = var.service_name
  requires_compatibilities = ["FARGATE"]
  network_mode             = "awsvpc"
  cpu                      = var.cpu
  memory                   = var.memory
  execution_role_arn       = aws_iam_role.task_execution.arn
  task_role_arn            = aws_iam_role.task_execution.arn

  container_definitions = jsonencode([
    merge({
      name      = var.service_name
      image     = "ghcr.io/${var.image_repo}:${var.image_tag}"
      essential = true

      # This block allows ECS to log into GitHub using your PAT
      repositoryCredentials = {
        credentialsParameter = var.ghcr_pat_secret_arn
      }

      portMappings = [
        {
          containerPort = var.container_port
          hostPort      = var.container_port
          protocol      = "tcp"
        }
      ]

      environment = [
        for key, value in var.environment : {
          name  = key
          value = tostring(value)
        }
      ]

      secrets = [
        for key, value in var.secrets : {
          name      = key
          valueFrom = value
        }
      ]

      logConfiguration = {
        logDriver = "awslogs"
        options = {
          awslogs-group         = aws_cloudwatch_log_group.this.name
          awslogs-region        = var.aws_region
          awslogs-stream-prefix = "ecs"
        }
      }
      },
      var.container_entrypoint != null ? {
        entryPoint = var.container_entrypoint
      } : {},
      var.container_command != null ? {
        command = var.container_command
      } : {}
    )
  ])
}

############################
# ECS Service
############################

resource "aws_ecs_service" "this" {
  name            = var.service_name
  cluster         = aws_ecs_cluster.this.id
  task_definition = aws_ecs_task_definition.this.arn
  launch_type     = "FARGATE"
  desired_count   = var.desired_count

  deployment_minimum_healthy_percent = 50
  deployment_maximum_percent         = 200

  network_configuration {
    subnets          = var.subnet_ids
    security_groups  = [aws_security_group.this.id]
    assign_public_ip = var.assign_public_ip
  }

  dynamic "load_balancer" {
    for_each = var.target_group_arn != null ? [1] : []
    content {
      target_group_arn = var.target_group_arn
      container_name   = var.service_name
      container_port   = var.container_port
    }
  }

  lifecycle {
    ignore_changes = [
      desired_count
    ]
  }
}