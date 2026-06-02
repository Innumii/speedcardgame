output "auth_postgres_endpoint" {
  description = "Auth postgres endpoint"
  value       = module.auth_postgres.endpoint
}

output "auth_postgres_port" {
  description = "Auth postgres port"
  value       = module.auth_postgres.port
}

output "auth_postgres_user" {
  description = "Auth postgres username"
  value       = module.auth_postgres.username
}

output "auth_postgres_password" {
  description = "Auth postgres password"
  value       = module.auth_postgres.password
  sensitive   = true
}

output "auth_postgres_db" {
  description = "Auth postgres database"
  value       = module.auth_postgres.db_name
}

output "cards_postgres_endpoint" {
  description = "Cards postgres endpoint"
  value       = module.cards_postgres.endpoint
}

output "cards_postgres_port" {
  description = "Cards postgres port"
  value       = module.cards_postgres.port
}

output "cards_postgres_user" {
  description = "Cards postgres username"
  value       = module.cards_postgres.username
}

output "cards_postgres_password" {
  description = "Cards postgres password"
  value       = module.cards_postgres.password
  sensitive   = true
}

output "cards_postgres_db" {
  description = "Cards postgres database"
  value       = module.cards_postgres.db_name
}

output "ghcr_pat_secret_arn" {
  description = "Secrets Manager ARN containing GHCR credentials"
  value       = var.skip_secrets_manager ? null : module.secrets[0].ghcr_secret_arn
}

output "auth_postgres_runtime_secret_arn" {
  description = "Secrets Manager ARN containing auth runtime secret values"
  value       = var.skip_secrets_manager ? null : module.secrets[0].auth_runtime_secret_arn
}

output "cards_runtime_secret_arn" {
  description = "Secrets Manager ARN containing cards runtime secret values"
  value       = var.skip_secrets_manager ? null : module.secrets[0].cards_runtime_secret_arn
}

output "auth_redis_endpoint" {
  description = "Auth redis endpoint"
  value       = module.auth_redis.endpoint
}

output "auth_redis_port" {
  description = "Auth redis port"
  value       = module.auth_redis.port
}

