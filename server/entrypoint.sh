#!/bin/sh
set -e

CERT_DIR=/certs
CRT_FILE="$CERT_DIR/server.crt"
KEY_FILE="$CERT_DIR/server.key"

# Create the certs directory if it doesn't exist
mkdir -p "$CERT_DIR"

# Only write cert and key if they don't already exist
if [ ! -f "$CRT_FILE" ] && [ ! -f "$KEY_FILE" ]; then
    if [ -n "$TLS_CERT" ] && [ -n "$TLS_KEY" ]; then
        echo "$TLS_CERT" > "$CRT_FILE"
        echo "$TLS_KEY"  > "$KEY_FILE"
        chmod 600 "$KEY_FILE"
        chmod 644 "$CRT_FILE"
    else
        echo "Warning: TLS_CERT or TLS_KEY not set. /certs will be empty."
    fi
else
    echo "TLS cert and key already exist, skipping environment variables."
fi

# Execute the original server binary
exec /app/server "$@"