#!/bin/sh
set -e

CERT_DIR=/app/certs
CRT_FILE="$CERT_DIR/server.crt"
KEY_FILE="$CERT_DIR/server.key"

# Create the certs directory if it doesn't exist
mkdir -p "$CERT_DIR"

normalize_secret_value() {
    value="$1"

    case "$value" in
        \"*\")
            value="${value#\"}"
            value="${value%\"}"
            ;;
        \'*\')
            value="${value#\'}"
            value="${value%\'}"
            ;;
    esac

    if printf '%s' "$value" | grep -q '\\n'; then
        value="$(printf '%b' "$value")"
    fi

    printf '%s' "$value"
}

write_secret_file_if_missing() {
    file_path="$1"
    secret_value="$2"

    if [ -s "$file_path" ]; then
        return 0
    fi

    if [ -z "$secret_value" ]; then
        return 1
    fi

    normalized_value="$(normalize_secret_value "$secret_value")"
    printf '%s\n' "$normalized_value" > "$file_path"
    return 0
}

generate_self_signed_cert_if_missing() {
    if [ -s "$CRT_FILE" ] && [ -s "$KEY_FILE" ]; then
        return 0
    fi

    if ! command -v openssl >/dev/null 2>&1; then
        echo "Error: openssl not found and TLS cert/key are missing."
        return 1
    fi

    echo "Info: generating self-signed TLS certificate for local runtime"
    openssl req -x509 -nodes -newkey rsa:2048 \
        -keyout "$KEY_FILE" \
        -out "$CRT_FILE" \
        -days 365 \
        -subj "/CN=localhost" >/dev/null 2>&1
}

validate_tls_cert() {
    if [ ! -s "$CRT_FILE" ]; then
        echo "Error: TLS certificate file is missing or empty at $CRT_FILE"
        return 1
    fi

    if ! grep -Eq '^-----BEGIN CERTIFICATE-----$' "$CRT_FILE"; then
        echo "Error: $CRT_FILE is not a valid PEM certificate (missing BEGIN CERTIFICATE header)"
        head -n 1 "$CRT_FILE" || true
        return 1
    fi
}

validate_tls_key() {
    if [ ! -s "$KEY_FILE" ]; then
        echo "Error: TLS private key file is missing or empty at $KEY_FILE"
        return 1
    fi

    if ! grep -Eq '^-----BEGIN (RSA )?PRIVATE KEY-----$' "$KEY_FILE"; then
        echo "Error: $KEY_FILE is not a valid PEM private key (missing BEGIN PRIVATE KEY header)"
        head -n 1 "$KEY_FILE" || true
        return 1
    fi
}

if ! write_secret_file_if_missing "$CRT_FILE" "$TLS_CERT" "TLS_CERT"; then
    if [ ! -s "$CRT_FILE" ]; then
        echo "Warning: TLS_CERT not set and $CRT_FILE does not exist."
    fi
fi

if ! write_secret_file_if_missing "$KEY_FILE" "$TLS_KEY" "TLS_KEY"; then
    if [ ! -s "$KEY_FILE" ]; then
        echo "Warning: TLS_KEY not set and $KEY_FILE does not exist."
    fi
fi

generate_self_signed_cert_if_missing

chmod 600 "$KEY_FILE" 2>/dev/null || true
chmod 644 "$CRT_FILE" 2>/dev/null || true

validate_tls_cert
validate_tls_key

# Execute the service binary
exec /app/main "$@"