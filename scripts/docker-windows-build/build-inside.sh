#!/usr/bin/env bash
# Appelé dans le conteneur : compile KeePassXC pour Windows (MinGW) avec vcpkg.
set -euo pipefail

SRC="${1:-/src}"
if [[ ! -f "${SRC}/CMakeLists.txt" ]]; then
    echo "Erreur: ${SRC} ne contient pas CMakeLists.txt (montez la racine du dépôt sur /src)." >&2
    exit 1
fi
cd "$SRC"

export VCPKG_ROOT="${VCPKG_ROOT:-/opt/vcpkg}"
TRIPLET="${VCPKG_TRIPLET:-x64-mingw-static}"
export VCPKG_DEFAULT_TRIPLET="${TRIPLET}"

TOOLCHAIN="${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
BUILD_DIR="${BUILD_DIR:-build-windows-docker}"

echo "==> Répertoire source: ${SRC}"
echo "==> Triplet vcpkg: ${TRIPLET}"
echo "==> Dossier de build: ${BUILD_DIR}"
echo "==> Première compilation: prévoir longtemps (Qt + dépendances via vcpkg) et beaucoup d'espace disque."

cmake -S . -B "${BUILD_DIR}" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    "-DCMAKE_TOOLCHAIN_FILE=${TOOLCHAIN}" \
    "-DVCPKG_TARGET_TRIPLET=${TRIPLET}" \
    -DWITH_TESTS=OFF \
    -DWITH_GUI_TESTS=OFF \
    -DWITH_XC_DOCS=OFF \
    -DKEEPASSXC_BUILD_TYPE=Release \
    -DWITH_XC_ALL=ON

cmake --build "${BUILD_DIR}" --parallel "$(nproc)"

echo ""
echo "==> Recherche des binaires Windows générés :"
find "${BUILD_DIR}" -maxdepth 5 \( -name 'KeePassXC.exe' -o -name 'keepassxc.exe' -o -name '*.exe' \) 2>/dev/null | head -20 || true

OUT="${OUTPUT_DIR:-}"
if [[ -n "${OUT}" && -d "${OUT}" ]]; then
    echo ""
    echo "==> Copie des .exe vers ${OUT}"
    find "${BUILD_DIR}" -name '*.exe' -exec cp -v {} "${OUT}/" \;
fi

echo ""
echo "Terminé. Les artefacts sont sous ${SRC}/${BUILD_DIR} (voir src/ pour l'exécutable principal)."
