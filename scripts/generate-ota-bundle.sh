#!/usr/bin/env bash
set -e

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CERT_DIR="$DIR/certs"
mkdir -p "$CERT_DIR"

if [ ! -f "$CERT_DIR/ca.key.pem" ]; then
    echo "=== Generating Self-Signed RAUC X.509 CA & Cert ==="
    openssl req -x509 -newkey rsa:4096 -nodes -keyout "$CERT_DIR/ca.key.pem" \
        -out "$CERT_DIR/ca.cert.pem" -subj "/O=IIoT Gateway/CN=RAUC CA" -days 3650
fi

echo "Certificates ready at: $CERT_DIR"
echo "To package a bundle, run: rauc bundle --cert=$CERT_DIR/ca.cert.pem --key=$CERT_DIR/ca.key.pem <input-dir> update-bundle.raucb"
