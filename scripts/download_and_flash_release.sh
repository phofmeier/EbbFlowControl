#!/bin/bash

# Script to download factory app from GitHub releases and flash to ESP32
# Usage: ./download_and_flash_release.sh [OPTIONS] [VERSION]
# Options:
#   -p, --port PORT         Serial port (default: /dev/ttyUSB0)
#   -r, --hard-reset        Perform hard reset and erase NVS partition
#   -f, --force-download    Force download even if version already exists
#   -h, --help              Show this help
# If VERSION is not specified, uses the latest tag

set -e

REPO="phofmeier/EbbFlowControl"
DEFAULT_PORT="/dev/ttyUSB0"

# Parse arguments
PORT="$DEFAULT_PORT"
HARD_RESET=false
FORCE_DOWNLOAD=false
DOWNLOAD_DIR=$(pwd)/downloads
VERSION=""

while [[ $# -gt 0 ]]; do
    case $1 in
        -p|--port)
            PORT="$2"
            shift 2
            ;;
        -r|--hard-reset)
            HARD_RESET=true
            shift
            ;;
        -f|--force-download)
            FORCE_DOWNLOAD=true
            shift
            ;;
        -h|--help)
            echo "Usage: $0 [OPTIONS] [VERSION]"
            echo "Download and flash factory app from GitHub releases"
            echo ""
            echo "Options:"
            echo "  -p, --port PORT         Serial port (default: $DEFAULT_PORT)"
            echo "  -r, --hard-reset        Perform hard reset and erase NVS partition"
            echo "  -f, --force-download    Force download even if version already exists"
            echo "  -h, --help              Show this help"
            echo ""
            echo "If VERSION is not specified, uses the latest tag"
            exit 0
            ;;
        *)
            if [ -z "$VERSION" ]; then
                VERSION="$1"
            else
                echo "Unknown option: $1"
                echo "Use -h for help"
                exit 1
            fi
            shift
            ;;
    esac
done

# Get version if not specified
if [ -z "$VERSION" ]; then
    echo "No version specified, getting latest tag..."
    VERSION=$(git describe --tags --abbrev=0 2>/dev/null || echo "latest")
    if [ "$VERSION" = "latest" ]; then
        echo "Could not determine latest tag from local git, using 'latest'"
        # get latest version from GitHub API
        VERSION=$(curl -s "https://api.github.com/repos/$REPO/releases/latest" | grep -oP '"tag_name": "\K(.*)(?=")')
        if [ -z "$VERSION" ]; then
            echo "Failed to get latest version from GitHub API"
            exit 1
        fi
    fi
fi

# remove leading 'v' if present
VERSION="${VERSION#v}"

echo "Using version: $VERSION"
echo "Serial port: $PORT"

EXTRACT_DIR="$DOWNLOAD_DIR/extracted_$VERSION"
# Check if version already exists (unless force download is enabled)
if [ "$FORCE_DOWNLOAD" = false ] && [ -d "$EXTRACT_DIR" ]; then
    echo "Version $VERSION already downloaded. Use --force-download to re-download."
    echo "Proceeding with existing download..."
else
    ZIP_FILE="FactoryBuildFiles.zip"
    DOWNLOAD_URL="https://github.com/$REPO/releases/download/$VERSION/$ZIP_FILE"

    echo "Downloading from: $DOWNLOAD_URL"

    # Create downloads directory
    mkdir -p "$DOWNLOAD_DIR"

    ZIP_PATH="$DOWNLOAD_DIR/$ZIP_FILE"

    echo "Downloading $ZIP_FILE..."
    if ! curl -L -o "$ZIP_PATH" "$DOWNLOAD_URL"; then
        echo "Failed to download $ZIP_FILE from $DOWNLOAD_URL"
        exit 1
    fi

    echo "Download complete."

    # Extract zip
    echo "Extracting $ZIP_FILE..."
    mkdir -p "$EXTRACT_DIR"
    # Unzip always overwrites, no need to clean
    if ! unzip -o -q "$ZIP_PATH" -d "$EXTRACT_DIR"; then
        echo "Failed to extract $ZIP_FILE"
        exit 1
    fi

    echo "Extracted successfully."
fi

# Flash all components
echo "Flashing to ESP32..."

cd "$EXTRACT_DIR/build_factory" || exit 1

if [ "$HARD_RESET" = true ]; then
    echo "Performing hard reset and erasing NVS partition..."
    python -m esptool --chip esp32 --port "$PORT" erase_flash || {
        echo "Failed to erase flash!"
        # move back to original directory
        cd - >/dev/null 2>&1
        exit 1
    }
    echo "Flash erased."
fi

python -m esptool --chip esp32 --port "$PORT" -b 460800 --before default_reset --after hard_reset write_flash @"flash_project_args" || {
    echo "Flashing failed!"
    # move back to original directory
    cd - >/dev/null 2>&1
    exit 1
}
echo "Flash complete!"

echo "Factory app $VERSION flashed successfully to $PORT."
# move back to original directory
cd - >/dev/null 2>&1
exit 0
