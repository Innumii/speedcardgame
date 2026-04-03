# Use the official Golang image as the base image
FROM golang:1.23-bookworm

# Set the Current Working Directory inside the container
WORKDIR /app

# Copy go.mod and go.sum files to the container
COPY go.mod go.sum ./

# Download all dependencies. Dependencies will be cached if the go.mod and go.sum files are not changed
RUN go mod download

# Copy the rest of the application code to the container
COPY . .

# Copy TLS certificates
COPY certs/ certs/

# Set default environment variables
ENV POSTGRES_HOST=host.docker.internal
ENV REDIS_HOST=host.docker.internal

# Build the Go application
RUN go build -o main -buildvcs=false .

# Expose the port the app runs on (if needed)
EXPOSE 8080

# Command to run the executable
CMD ["./main"]
