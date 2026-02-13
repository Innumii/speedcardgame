# Makefile does not work for Windows, use Git Bash or WSL
.PHONY: format

tools:
	go install github.com/swaggo/swag/cmd/swag@latest
	go install github.com/golangci/golangci-lint/cmd/golangci-lint@latest
	go install github.com/bokwoon95/wgo@latest

setup: tools
	go mod tidy

format:
	@gofmt -s -w .
	@golangci-lint run

drop:
	go run main.go db:drop

create:
	go run main.go db:create

run dev:
	swag init
	wgo run main.go

up:
	docker build -t auth-service .
	docker run -p 8080:8080 auth-service

down:
	docker stop $(docker ps -a -q)
	docker rm $(docker ps -a -q)
