#!/bin/bash

# This script creates a server certificate for an own secure https server.
# The certificate needs to be added to the Application to work properly.
# Set `CONFIG_OTA_USE_CERT_BUNDLE=n` and
#`CONFIG_OTA_FIRMWARE_UPGRADE_URL="https://192.168.2.106:8070/EbbFlowControl.bin` to use
# the local certificate. To change the path to the added certificate update the
# CMAKE file `components/ota_updater/CMakeLists.txt`

echo "Use the IP address of the server as the Common Name (CN) for the certificate."
echo "Add IP as first argument if you need to change it."
hostname=${1:-"$(hostname -I | awk '{print $1}')"}

echo "Using hostname/IP: $hostname"

openssl req -x509 -newkey rsa:2048 -keyout ca_key.pem -out ca_cert.pem -days 365 -nodes -subj /CN=$hostname
