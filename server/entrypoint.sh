#!/bin/sh
set -e

# Create the certs directory
mkdir -p /certs

# Write TLS certs from environment variables
echo "$TLS_CERT" > /certs/server.crt
echo "$TLS_KEY"  > /certs/server.key

# Secure the files
chmod 600 /certs/server.*

# Execute the original server binary
exec /app/server "$@"