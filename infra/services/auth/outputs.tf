output "ecs_cluster_name" {
  description = "Auth ECS cluster name"
  value       = module.service.cluster_id
}

output "service_name" {
  description = "Auth ECS service name"
  value       = module.service.service_name
}