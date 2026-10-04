#!/usr/bin/env bash
set -euo pipefail

RULE_FILE="/etc/udev/rules.d/99-esp32.rules"

# Find an ESP32 serial device.
DEVICE=""

for dev in /dev/ttyUSB* /dev/ttyACM*; do
    [[ -e "$dev" ]] || continue

    if udevadm info --query=property --name="$dev" 2>/dev/null \
        | grep -Eq 'ID_VENDOR_ID=(10C4|1A86|303A)'; then
        DEVICE="$dev"
        break
    fi
done

if [[ -z "$DEVICE" ]]; then
    echo "No supported ESP32 USB serial device found."
    exit 1
fi

SERIAL=$(udevadm info --query=property --name="$DEVICE" \
    | awk -F= '$1 == "ID_SERIAL_SHORT" {print $2}')

if [[ -z "$SERIAL" ]]; then
    echo "Could not determine device serial number."
    exit 1
fi

VENDOR=$(udevadm info --query=property --name="$DEVICE" \
    | awk -F= '$1 == "ID_VENDOR_ID" {print $2}')

cat > /tmp/99-esp32.rules <<EOF
SUBSYSTEM=="tty", ATTRS{idVendor}=="$VENDOR", ENV{ID_SERIAL_SHORT}=="$SERIAL", SYMLINK+="esp32", GROUP="dialout", MODE="0660"
EOF

sudo cp /tmp/99-esp32.rules "$RULE_FILE"

sudo udevadm control --reload-rules
sudo udevadm trigger

echo "Created:"
echo "  $RULE_FILE"
echo
echo "ESP32:"
echo "  $DEVICE -> /dev/esp32"
