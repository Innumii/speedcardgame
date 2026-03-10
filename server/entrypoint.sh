#!/bin/sh
set -e

# Create the certs directory
mkdir -p /tmp/certs
printf "%s" "$TLS_CERT" > /tmp/certs/server.crt
printf "%s" "$TLS_KEY" > /tmp/certs/server.key

# Secure the files
chmod 600 /tmp/certs/server.*

# Execute the original server binary
exec /app/server "$@"