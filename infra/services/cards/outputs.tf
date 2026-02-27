output "ecr_repository_url" {
  description = "Cards ECR repository URL"
  value       = module.service.ecr_repository_url
}

output "ecs_cluster_name" {
  description = "Cards ECS cluster name"
  value       = module.service.ecs_cluster_name
}

output "ecs_service_name" {
  description = "Cards ECS service name"
  value       = module.service.ecs_service_name
}
