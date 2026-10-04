#!/usr/bin/env bash

set -euo pipefail

echo "Installing pre-commit hooks..."

pre-commit install --install-hooks

echo "Dev container ready."
