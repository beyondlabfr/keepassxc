#!/usr/bin/env bash
#
# Régénère l'icône desktop PNG à partir du SVG, puis minifie/crush les assets.
#
# Usage (depuis n'importe où) :
#   ./scripts/minify-icons.sh
#
set -euo pipefail

NC='\033[0m'
YELLOW='\033[0;33m'

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
SHARE_DIR="${ROOT}/share"

SVG="${SHARE_DIR}/icons/application/scalable/apps/keepassxc.svg"
PNG="${SHARE_DIR}/icons/application/256x256/apps/keepassxc.png"

cd "${SHARE_DIR}"

# Build desktop icon
echo "Creating desktop icon PNG..."
if command -v inkscape &> /dev/null; then
  # Inkscape 1.x:
  #   inkscape --export-type=png --export-filename=out.png -w 256 -h 256 in.svg
  # Inkscape 0.x:
  #   inkscape -z -e out.png -w 256 -h 256 in.svg
  if inkscape --version 2>/dev/null | grep -qE 'Inkscape 1\.'; then
    inkscape --export-type=png \
      --export-filename="${PNG}" \
      -w 256 -h 256 \
      "${SVG}"
  else
    inkscape -z -w 256 -h 256 \
      "${SVG}" \
      -e "${PNG}"
  fi
else
  echo -e "${YELLOW}Could not find inkscape; keepassxc.png not built!${NC}"
fi

# Minify SVG's
echo "Minifying SVG's..."
minify -o icons/badges --match=.svg icons/badges
minify -o icons/database --match=.svg icons/database

# Crush PNG's
echo "Crushing PNG's..."
find . -iname '*.png' -exec pngcrush -ow -brute {} \;

