resource "aws_secretsmanager_secret" "ghcr_creds" {
  name        = "ghcr-auth-credentials" # Fixed name for easy look-up
  description = "Shared credentials for GHCR"
}

resource "aws_secretsmanager_secret_version" "ghcr_creds_version" {
  secret_id     = aws_secretsmanager_secret.ghcr_creds.id
  secret_string = jsonencode({
    username = var.github_username
    password = var.github_token
  })
}

# Output the ARN so you can copy it if needed
output "secret_arn" {
  value = aws_secretsmanager_secret.ghcr_creds.arn
}