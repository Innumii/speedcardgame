output "ecr_repository_url" {
  description = "Server container image reference"
  value       = "ghcr.io/${var.image_repo}:${var.image_tag}"
}

output "ecs_cluster_name" {
  description = "Server ECS cluster name"
  value       = module.service.cluster_id
}

output "ecs_service_name" {
  description = "Server ECS service name"
  value       = module.service.service_name
}

output "nlb_dns_name" {
  description = "Server NLB DNS name"
  value       = aws_lb.this.dns_name
}

output "game_server_host" {
  description = "Stable game server hostname for clients (Route53 record when configured, otherwise NLB DNS)"
  value       = local.game_server_host_for_client
}

output "game_server_port" {
  description = "Raw TCP/TLS game server port"
  value       = var.game_port
}
