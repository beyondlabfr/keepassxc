#!/usr/bin/env bash
# Compile KeePassXC pour macOS et produit le .dmg (Qt6 et dépendances par vcpkg), avec les mêmes
# options que release-tool.py (build_macos) de l'upstream.
#
# Usage : scripts/build-macos.sh [arm64|x86_64]   (défaut : architecture de la machine)
#   Un Mac Intel peut viser arm64 (Apple Silicon) : vcpkg compile alors les outils de Qt pour la
#   machine (triplet hôte) et les bibliothèques pour la cible, soit deux fois Qt.
#
# Variables : VCPKG_ROOT (défaut ~/vcpkg), BUILD_DIR (défaut <racine>/build-macos-<arch>),
#             JOBS (défaut : nombre de cœurs), MACOS_TARGET (défaut 12, comme l'upstream)
#             CODESIGN_IDENTITY (défaut « - » : signature ad hoc, sans compte Apple ; sinon le nom
#             d'un certificat « Developer ID Application » du trousseau)
# Prérequis : Command Line Tools (Xcode complet inutile), cmake, ninja, pkg-config, autoconf,
#             automake, autoconf-archive, libtool ; asciidoctor pour la documentation hors ligne.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ARCH="${1:-$(uname -m)}"
VCPKG_ROOT="${VCPKG_ROOT:-${HOME}/vcpkg}"
BUILD_DIR="${BUILD_DIR:-${ROOT}/build-macos-${ARCH}}"
JOBS="${JOBS:-$(sysctl -n hw.ncpu)}"
MACOS_TARGET="${MACOS_TARGET:-12}"

die() { echo "Erreur : $*" >&2; exit 1; }

triplet() {
    case "$1" in
        arm64) echo "arm64-osx-dynamic-release" ;;
        x86_64) echo "x64-osx-dynamic-release" ;;
        *) die "architecture inconnue : $1 (arm64 ou x86_64)" ;;
    esac
}

[[ -f "${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" ]] || die "vcpkg introuvable dans ${VCPKG_ROOT}"
xcode-select -p >/dev/null 2>&1 || die "Command Line Tools absents (xcode-select --install)"

DOCS="-DKPXC_FEATURE_DOCS=OFF"
if command -v asciidoctor >/dev/null 2>&1; then
    DOCS="-DKPXC_FEATURE_DOCS=ON"
else
    echo "asciidoctor absent : compilation sans documentation hors ligne"
fi

echo "==> Configuration ${ARCH} (cible $(triplet "${ARCH}"), hôte $(triplet "$(uname -m)")) dans ${BUILD_DIR}"
cmake -S "${ROOT}" -B "${BUILD_DIR}" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE="${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" \
    -DWITH_BUILD_QT=ON \
    -DWITH_TESTS=OFF \
    -DVCPKG_TARGET_TRIPLET="$(triplet "${ARCH}")" \
    -DVCPKG_HOST_TRIPLET="$(triplet "$(uname -m)")" \
    -DVCPKG_INSTALL_OPTIONS=--clean-after-build \
    -DCMAKE_OSX_ARCHITECTURES="${ARCH}" \
    -DCMAKE_OSX_DEPLOYMENT_TARGET="${MACOS_TARGET}" \
    -DKEEPASSXC_BUILD_TYPE=Release \
    -DWITH_XC_CODESIGN_IDENTITY="${CODESIGN_IDENTITY:--}" \
    ${DOCS}

echo "==> Compilation (-j${JOBS})"
cmake --build "${BUILD_DIR}" --parallel "${JOBS}"

echo "==> Paquet .dmg"
(cd "${BUILD_DIR}" && cpack -G DragNDrop)
ls -lh "${BUILD_DIR}"/KeePassXC-*.dmg
