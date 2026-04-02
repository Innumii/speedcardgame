# ---------- Build Stage ----------
FROM golang:1.23-bookworm AS builder

WORKDIR /app

# Copy go.mod and go.sum files to the container
COPY go.mod go.sum ./

# Download all dependencies. Dependencies will be cached if the go.mod and go.sum files are not changed
RUN go mod download

# Copy the rest of the application code to the container
COPY . .

# Build the Go application
RUN go build -o main -buildvcs=false .

# ---------- Runtime Stage ----------
FROM golang:1.23-bookworm AS runtime

WORKDIR /app

# Copy the compiled binary from the builder stage
COPY --from=builder /app/main .

# Copy TLS certificates into the runtime image so the service
# uses the shared CA-signed server certificate/key for HTTPS.
COPY --from=builder /app/certs ./certs

# Copy entrypoint for TLS secret materialization
COPY entrypoint.sh /entrypoint.sh
RUN sed -i 's/\r$//' /entrypoint.sh && chmod +x /entrypoint.sh

# Set default environment variables
ENV POSTGRES_HOST=host.docker.internal
ENV REDIS_HOST=host.docker.internal

# Expose the port the app runs on
EXPOSE 8080

ENTRYPOINT ["/entrypoint.sh"]
CMD []