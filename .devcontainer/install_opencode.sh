#!/usr/bin/env bash

set -euo pipefail

OPENCODE_VERSION="${OPENCODE_VERSION:?OPENCODE_VERSION must be set}"

echo "Installing OpenCode ${OPENCODE_VERSION}..."

curl -fsSL https://opencode.ai/v2/install \
  | bash -s -- \
      --version "${OPENCODE_VERSION}" \
      --no-modify-path

echo
echo "OpenCode:"
opencode --version
