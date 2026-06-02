output "ecr_repository_url" {
  description = "Cards container image reference"
  value       = "ghcr.io/${var.auth_image_repo}:${var.image_tag}"
}

output "ecs_cluster_name" {
  description = "Auth ECS cluster name"
  value       = module.service.cluster_id
}

output "service_name" {
  description = "Auth ECS service name"
  value       = module.service.service_name
}

output "ecs_service_security_group_id" {
  description = "Auth ECS service security group ID"
  value       = module.service.security_group_id
}
