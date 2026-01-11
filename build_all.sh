#!/bin/bash

# Build script for two apps

set -e

echo "Building Application"
idf.py @profiles/app build || { echo "Application build failed"; exit -1; }

echo "Building Factory"
idf.py @profiles/factory build || { echo "Factory build failed"; exit -1; }

cmake --build build_factory --target generate_wifi_qr || { echo "QR code generation failed"; exit -1; }

echo "Build finished.."
exit 0
