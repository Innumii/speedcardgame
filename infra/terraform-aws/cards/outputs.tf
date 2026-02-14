output "alb_dns_name" {
  description = "Public ALB DNS name"
  value       = aws_lb.public.dns_name
}

output "instance_public_ip" {
  description = "Public IP of the EC2 instance"
  value       = aws_instance.cards.public_ip
}

output "ecr_cards_repo" {
  description = "ECR repository URL for cards"
  value       = aws_ecr_repository.cards.repository_url
}
