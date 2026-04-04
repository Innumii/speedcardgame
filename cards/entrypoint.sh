#!/bin/sh
set -e

CERT_DIR=/app/certs
CRT_FILE="$CERT_DIR/server.crt"
KEY_FILE="$CERT_DIR/server.key"

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

write_secret_file_prefer_env() {
    file_path="$1"
    secret_value="$2"

    if [ -n "$secret_value" ]; then
        normalized_value="$(normalize_secret_value "$secret_value")"
        printf '%s\n' "$normalized_value" > "$file_path"
        return 0
    fi

    if [ -s "$file_path" ]; then
        return 0
    fi

    return 1
}

validate_tls_cert() {
    if [ ! -s "$CRT_FILE" ]; then
        echo "Error: TLS certificate file is missing or empty at $CRT_FILE"
        return 1
    fi

    if ! grep -Eq '^-----BEGIN CERTIFICATE-----$' "$CRT_FILE"; then
        echo "Error: $CRT_FILE is not a valid PEM certificate"
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
        echo "Error: $KEY_FILE is not a valid PEM private key"
        head -n 1 "$KEY_FILE" || true
        return 1
    fi
}

if ! write_secret_file_prefer_env "$CRT_FILE" "$TLS_CERT"; then
    if [ ! -s "$CRT_FILE" ]; then
        echo "Warning: TLS_CERT not set and $CRT_FILE does not exist."
    fi
fi

if ! write_secret_file_prefer_env "$KEY_FILE" "$TLS_KEY"; then
    if [ ! -s "$KEY_FILE" ]; then
        echo "Warning: TLS_KEY not set and $KEY_FILE does not exist."
    fi
fi

chmod 600 "$KEY_FILE" 2>/dev/null || true
chmod 644 "$CRT_FILE" 2>/dev/null || true

validate_tls_cert
validate_tls_key

exec /app/main
