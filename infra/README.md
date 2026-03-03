# Terraform deployment (AWS, shared ALB + Route53)

This folder contains **one shared data stack**, **one shared ALB stack**, and **three isolated AWS service stacks**:

- `infra/data`
- `infra/services/alb`

- `infra/services/auth`
- `infra/services/cards`
- `infra/services/server`

`infra/services/alb` provisions one internet-facing shared ALB, HTTPS listener, path rules, and Route53 alias:

- `/auth/*` -> auth target group
- `/cards/*` -> cards target group
- `api.myapp.com` -> ALB alias A record

## Common modules

- `infra/modules/ecs-public-service`: shared ECS/ECR/service logic
- `infra/modules/rds-postgres`: shared RDS PostgreSQL logic
- `infra/modules/elasticache-redis`: shared ElastiCache Redis logic

## Prerequisites

- Terraform `>= 1.5`
- AWS CLI authenticated to your target account
- Permissions for ECS, ECR, IAM, EC2 networking, and CloudWatch Logs

## 1) Deploy shared data infrastructure first

```bash
cd infra/data
cp terraform.tfvars.example terraform.tfvars
# optional: set auth_postgres_password and cards_postgres_password
terraform init
terraform plan
terraform apply
```

This provisions:

- Auth PostgreSQL (RDS)
- Cards PostgreSQL (RDS)
- Auth Redis (ElastiCache)
- Runtime secrets in AWS Secrets Manager for auth/cards

## 2) Deploy shared ALB + Route53 endpoint

```bash
cd infra/services/alb
cp terraform.tfvars.example terraform.tfvars
# set acm_certificate_arn and your Route53 zone/domain
terraform init
terraform plan
terraform apply
```

## 3) Deploy Auth service only

```bash
cd infra/services/auth
cp terraform.tfvars.example terraform.tfvars
# optional: override cards_service_base_url
terraform init
terraform plan
terraform apply
```

By default, auth reads DB/Redis values from `infra/data/terraform.tfstate`.
Auth `POSTGRES_PASSWORD` is injected from AWS Secrets Manager when available.

## 4) Deploy Cards service only

```bash
cd infra/services/cards
cp terraform.tfvars.example terraform.tfvars
# optional: override database_url manually
terraform init
terraform plan
terraform apply
```

By default, cards reads `DATABASE_URL` from `infra/data/terraform.tfstate`.
Cards `DATABASE_URL` is injected from AWS Secrets Manager when available.

## 5) Deploy Game Server only

```bash
cd infra/services/server
cp terraform.tfvars.example terraform.tfvars
terraform init
terraform plan
terraform apply
```

## Notes on wiring and overrides

- `use_managed_data_stack = true` (default) makes auth/cards auto-read from the shared data state.
- `use_managed_alb_stack = true` (default) makes auth/cards auto-read ALB target groups and ALB security group from `infra/services/alb/terraform.tfstate`.
- Set `use_managed_data_stack = false` to provide manual DB/Redis values directly in each service tfvars.
- Set `use_managed_alb_stack = false` to provide manual `target_group_arn` and `alb_security_group_id` in each service tfvars.
- If needed, change `data_stack_state_path` in service stacks to point at a different shared state file.

## Build and push images

After `terraform apply`, use each stack output `ecr_repository_url` to build and push your image.

Example (auth):

```bash
aws ecr get-login-password --region us-east-1 | docker login --username AWS --password-stdin <account>.dkr.ecr.us-east-1.amazonaws.com
docker build -t <ecr_repository_url>:latest auth
docker push <ecr_repository_url>:latest
```

Re-run `terraform apply` in the service stack if you need to force a redeploy.
