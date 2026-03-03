output "ecr_repository_url" {
  description = "Cards container image reference"
  value       = "ghcr.io/${var.image_repo}:${var.image_tag}"
}

output "ecs_cluster_name" {
  description = "Cards ECS cluster name"
  value       = module.service.cluster_id
}

output "ecs_service_name" {
  description = "Cards ECS service name"
  value       = module.service.service_name
}

output "ecs_service_security_group_id" {
  description = "Cards ECS service security group ID"
  value       = module.service.security_group_id
}
