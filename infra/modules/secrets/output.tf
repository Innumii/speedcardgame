output "ghcr_secret_arn" {
  value = coalesce(var.existing_ghcr_secret_arn, try(aws_secretsmanager_secret.ghcr_creds[0].arn, null))
}

output "auth_runtime_secret_arn" {
  value = aws_secretsmanager_secret.auth_runtime.arn
}

output "cards_runtime_secret_arn" {
  value = aws_secretsmanager_secret.cards_runtime.arn
}
