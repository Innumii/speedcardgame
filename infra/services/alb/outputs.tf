output "alb_arn" {
  description = "Shared ALB ARN"
  value       = aws_lb.shared.arn
}

output "alb_dns_name" {
  description = "Shared ALB DNS name"
  value       = aws_lb.shared.dns_name
}

output "alb_zone_id" {
  description = "Shared ALB hosted zone ID"
  value       = aws_lb.shared.zone_id
}

output "alb_security_group_id" {
  description = "Security group ID attached to shared ALB"
  value       = aws_security_group.alb.id
}

output "auth_target_group_arn" {
  description = "Target group ARN for auth service"
  value       = aws_lb_target_group.auth.arn
}

output "cards_target_group_arn" {
  description = "Target group ARN for cards service"
  value       = aws_lb_target_group.cards.arn
}

output "api_domain_name" {
  description = "Route53 endpoint for API"
  value       = trimsuffix(aws_route53_record.api.fqdn, ".")
}

output "api_base_url" {
  description = "Public API base URL"
  value       = "${var.enable_https_listener ? "https" : "http"}://${trimsuffix(aws_route53_record.api.fqdn, ".")}"
}

