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

# Set default environment variables
ENV POSTGRES_HOST=host.docker.internal
ENV REDIS_HOST=host.docker.internal

# Build the Go application
RUN go build -o main -buildvcs=false .

# Bootstrap TLS cert/key files from secret env vars when file mounts are unavailable.
COPY entrypoint.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh

# Expose the port the app runs on (if needed)
EXPOSE 8080

# Command to run the executable
ENTRYPOINT ["/entrypoint.sh"]
