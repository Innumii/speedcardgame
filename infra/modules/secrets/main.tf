resource "aws_secretsmanager_secret" "ghcr_creds" {
  count       = var.existing_ghcr_secret_arn == null ? 1 : 0
  name        = "ghcr-auth-credentials" # Fixed name for easy look-up
  description = "Shared credentials for GHCR"
}

resource "aws_secretsmanager_secret_version" "ghcr_creds_version" {
  count     = var.existing_ghcr_secret_arn == null ? 1 : 0
  secret_id = aws_secretsmanager_secret.ghcr_creds[0].id
  secret_string = jsonencode({
    username = var.github_username
    password = var.github_token
  })
}

resource "aws_secretsmanager_secret" "auth_runtime" {
  name        = var.auth_runtime_secret_name
  description = "Runtime secret values for auth service"
}

locals {
  auth_runtime_secret_values = merge(
    {
      POSTGRES_PASSWORD = var.auth_postgres_password
    },
    var.cards_service_base_url != null ? {
      CARDS_SERVICE_BASE_URL = var.cards_service_base_url
    } : {}
  )
}

resource "aws_secretsmanager_secret_version" "auth_runtime_version" {
  secret_id     = aws_secretsmanager_secret.auth_runtime.id
  secret_string = jsonencode(local.auth_runtime_secret_values)
}

resource "aws_secretsmanager_secret" "cards_runtime" {
  name        = var.cards_runtime_secret_name
  description = "Runtime secret values for cards service"
}

locals {
  cards_runtime_secret_values = merge(
    {
      DATABASE_URL = var.cards_database_url
    },
    var.stripe_secret_key != null ? {
      STRIPE_SECRET_KEY = var.stripe_secret_key
    } : {},
    var.stripe_webhook_secret != null ? {
      STRIPE_WEBHOOK_SECRET = var.stripe_webhook_secret
    } : {}
  )
}

resource "aws_secretsmanager_secret_version" "cards_runtime_version" {
  secret_id     = aws_secretsmanager_secret.cards_runtime.id
  secret_string = jsonencode(local.cards_runtime_secret_values)
}

# Output the ARN so you can copy it if needed
output "ghcr_secret_arn" {
  value = coalesce(var.existing_ghcr_secret_arn, try(aws_secretsmanager_secret.ghcr_creds[0].arn, null))
}

output "auth_runtime_secret_arn" {
  value = aws_secretsmanager_secret.auth_runtime.arn
}

output "cards_runtime_secret_arn" {
  value = aws_secretsmanager_secret.cards_runtime.arn
}