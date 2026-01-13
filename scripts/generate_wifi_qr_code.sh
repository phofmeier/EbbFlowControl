#!/bin/bash

# Script to generate WiFi QR code using qrencode
# Usage: ./generate_wifi_qr_code.sh <SSID> <PASSWORD> <IP ADDRESS> <PORT> [SECURITY_TYPE]
# SECURITY_TYPE defaults to WPA2

if [ $# -lt 2 ]; then
    echo "Usage: $0 <SSID> <PASSWORD> <IP ADDRESS> <PORT> [SECURITY_TYPE]"
    echo "SECURITY_TYPE: WPA, WPA2, WEP, or nopass (default: WPA2)"
    exit 1
fi

SSID="$1"
PASSWORD="$2"
IP_ADDRESS="${3}"
PORT="${4}"
SECURITY="${5:-WPA2}"

# Validate security type
case "$SECURITY" in
    WPA|WPA2|WEP)
        ;;
    nopass)
        SECURITY=""
        ;;
    *)
        echo "Invalid security type. Use WPA, WPA2, WEP, or nopass"
        exit 1
        ;;
esac

# Generate the WiFi QR code string
if [ -z "$SECURITY" ]; then
    QR_STRING="WIFI:S:$SSID;;"
else
    QR_STRING="WIFI:S:$SSID;T:$SECURITY;P:$PASSWORD;;"
fi

echo "Generating QR code for WiFi network: $SSID"
echo "QR String: $QR_STRING"

# Generate QR code and save as PNG
OUTPUT_FILE="${SSID}_wifi_qr.png"
qrencode -o "$OUTPUT_FILE" "$QR_STRING"

if [ $? -eq 0 ]; then
    echo "QR code generated successfully: $OUTPUT_FILE"
else
    echo "Failed to generate QR code"
    exit 1
fi

# Generate connection URL QR code
CONNECTION_URL="http://${IP_ADDRESS}:${PORT}/"
URL_QR_FILE="${SSID}_connection_url_qr.png"
echo "Generating QR code for connection URL: $CONNECTION_URL"
qrencode -o "$URL_QR_FILE" "$CONNECTION_URL"

if [ $? -eq 0 ]; then
    echo "Connection URL QR code generated successfully: $URL_QR_FILE"
else
    echo "Failed to generate connection URL QR code"
    exit 1
fi
