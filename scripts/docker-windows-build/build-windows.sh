#!/usr/bin/env bash
#
# Build KeePassXC pour Windows (.exe MinGW) dans Docker (Linux / macOS avec Docker).
#
# Usage (depuis la racine du dépôt KeePassXC) :
#   ./scripts/docker-windows-build/build-windows.sh
#
# Variables optionnelles :
#   IMAGE_NAME   — nom de l'image Docker (défaut : keepassxc-mingw-builder)
#   BUILD_DIR    — dossier de build dans le conteneur (défaut : build-windows-docker)
#   OUTPUT_DIR   — si défini, chemin hôte où copier tous les .exe à la fin (ex. ./out-windows)
#   VCPKG_TRIPLET — défaut : x64-mingw-static (essayez x64-mingw-dynamic si la liaison pose problème)
#
# Notes :
#   - Le workflow officiel des releases utilise MSVC sur Windows, pas MinGW.
#   - La première exécution télécharge et compile vcpkg (Qt, Botan, etc.) : très long.
#   - Prévoir plusieurs dizaines de Go d'espace disque libre pour le cache de build.
#

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

IMAGE_NAME="${IMAGE_NAME:-keepassxc-mingw-builder}"
BUILD_DIR="${BUILD_DIR:-build-windows-docker}"

echo "==> Construction de l'image Docker (${IMAGE_NAME})..."
docker build -t "${IMAGE_NAME}" "${SCRIPT_DIR}"

OUTPUT_MOUNT=()
if [[ -n "${OUTPUT_DIR:-}" ]]; then
    mkdir -p "${OUTPUT_DIR}"
    OUTPUT_MOUNT=( -e "OUTPUT_DIR=/out" -v "$(realpath "${OUTPUT_DIR}"):/out" )
fi

echo "==> Lancement du build (montage du dépôt en lecture-écriture pour build + vcpkg_installed)..."
docker run --rm \
    -e "BUILD_DIR=${BUILD_DIR}" \
    -e "VCPKG_TRIPLET=${VCPKG_TRIPLET:-x64-mingw-static}" \
    "${OUTPUT_MOUNT[@]}" \
    -v "${REPO_ROOT}:/src" \
    "${IMAGE_NAME}" \
    /src

echo ""
echo "==> Fait. Artefacts dans : ${REPO_ROOT}/${BUILD_DIR}"
