#!/usr/bin/env bash
set -euo pipefail

# Ce script a été déplacé dans scripts/ pour pouvoir être lancé depuis n'importe où.
# Garde un wrapper ici pour compatibilité.
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
exec "${ROOT}/scripts/minify-icons.sh"
