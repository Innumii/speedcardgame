output "ecr_repository_url" {
  description = "Auth ECR repository URL"
  value       = module.service.ecr_repository_url
}

output "ecs_cluster_name" {
  description = "Auth ECS cluster name"
  value       = module.service.ecs_cluster_name
}

output "ecs_service_name" {
  description = "Auth ECS service name"
  value       = module.service.ecs_service_name
}
