output "ecr_repository_url" {
  description = "Server ECR repository URL"
  value       = module.service.ecr_repository_url
}

output "ecs_cluster_name" {
  description = "Server ECS cluster name"
  value       = module.service.cluster_id
}

output "ecs_service_name" {
  description = "Server ECS service name"
  value       = module.service.service_name
}
