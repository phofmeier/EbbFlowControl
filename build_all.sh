#!/bin/bash

# Build script for two apps

set -e

echo "Building Application"
idf.py @profiles/app build || { echo "Application build failed"; exit -1; }

echo "Building Factory"
idf.py @profiles/factory build || { echo "Factory build failed"; exit -1; }

echo "Build finished.."
exit 0
