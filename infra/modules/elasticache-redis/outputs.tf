output "endpoint" {
  description = "Redis endpoint hostname"
  value       = aws_elasticache_cluster.this.cache_nodes[0].address
}

output "port" {
  description = "Redis port"
  value       = aws_elasticache_cluster.this.port
}

