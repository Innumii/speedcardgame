# Use an official Go image for building the application
FROM golang:1.24-alpine

# Set the current working directory inside the container
WORKDIR /app

# Copy the Go module files and download dependencies
COPY go.mod go.sum ./
RUN go mod download

# Copy the rest of the application code
COPY . .

# Build the Go application
RUN go build -o main .

# Expose the application port (this should match the port in your .env file)
EXPOSE 8080

# Command to run the application
CMD ["./main"]
