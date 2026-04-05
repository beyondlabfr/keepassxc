#!/usr/bin/env bash
# Construit KeePassXC et génère un paquet .deb (Debian / Ubuntu).
#
# Usage :
#   ./scripts/build-debian-package.sh
#   INSTALL_DEPS=1 ./scripts/build-debian-package.sh    # installe les deps de build (apt)
#
# Variables utiles :
#   BUILD_DIR          Répertoire de compilation (défaut : <racine>/build-deb)
#   CMAKE_GENERATOR    ex. Ninja (défaut : déduit)
#   DEB_PACKAGE_NAME   Nom du paquet (défaut : keepassxc)
#   DEB_REVISION       Révision Debian (défaut : 1)
#   DEB_VERSION        Surcharge la version du .deb (sinon dérivée de CMakeLists.txt + git)
#   EXTRA_CMAKE_ARGS   Arguments CMake additionnels (chaîne)
#   JOBS               Parallélisme make/ninja (défaut : nproc)
#   WITH_XC_DOCS       1 pour activer la doc (nécessite asciidoctor), 0 sinon (défaut : 0)
#   SKIP_BUILD         1 pour ne réinstaller que le .deb depuis un build existant
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-${ROOT}/build-deb}"
STAGING="${STAGING:-${BUILD_DIR}/staging}"
PREFIX="${CMAKE_INSTALL_PREFIX:-/usr}"
JOBS="${JOBS:-$(nproc 2>/dev/null || echo 4)}"
DEB_PACKAGE_NAME="${DEB_PACKAGE_NAME:-keepassxc}"
DEB_REVISION="${DEB_REVISION:-1}"
WITH_XC_DOCS="${WITH_XC_DOCS:-0}"
SKIP_BUILD="${SKIP_BUILD:-0}"
INSTALL_DEPS="${INSTALL_DEPS:-0}"

die() { echo "Erreur: $*" >&2; exit 1; }

[[ "$(uname -s)" == "Linux" ]] || die "Ce script est prévu pour Linux (Debian/Ubuntu)."

command -v cmake >/dev/null || die "cmake est requis."
command -v dpkg-deb >/dev/null || die "dpkg-deb est requis (paquet dpkg-dev)."

extract_cmake_string_var() {
  local var="$1"
  grep -E "^set\\(${var} \"" "${ROOT}/CMakeLists.txt" | head -1 | sed -E 's/^set\([^ ]+ "([^"]*)".*/\1/'
}

V_MAJOR="$(extract_cmake_string_var KEEPASSXC_VERSION_MAJOR)"
V_MINOR="$(extract_cmake_string_var KEEPASSXC_VERSION_MINOR)"
V_PATCH="$(extract_cmake_string_var KEEPASSXC_VERSION_PATCH)"
BASE_VERSION="${V_MAJOR}.${V_MINOR}.${V_PATCH}"
[[ -n "${V_MAJOR}" ]] || die "Impossible de lire la version dans CMakeLists.txt."

if [[ -n "${DEB_VERSION:-}" ]]; then
  PKG_VERSION="${DEB_VERSION}"
else
  PKG_VERSION="${BASE_VERSION}-${DEB_REVISION}"
  if [[ -d "${ROOT}/.git" ]]; then
    GIT_SHORT="$(git -C "${ROOT}" rev-parse --short HEAD 2>/dev/null || true)"
    [[ -n "${GIT_SHORT}" ]] && PKG_VERSION="${BASE_VERSION}-${DEB_REVISION}~git${GIT_SHORT}"
  fi
fi

# Normalise pour policy Debian (pas d'underscore en upstream version problématique)
PKG_VERSION="${PKG_VERSION//_/-}"

DEB_OUT="${BUILD_DIR}/${DEB_PACKAGE_NAME}_${PKG_VERSION}_$(dpkg --print-architecture).deb"

if [[ "${INSTALL_DEPS}" == "1" ]]; then
  echo "==> Installation des dépendances de build (apt)…"
  sudo apt-get update
  sudo apt-get install -y --no-install-recommends \
    build-essential cmake pkg-config \
    libbotan-2-dev libargon2-dev libminizip-dev zlib1g-dev \
    libqrencode-dev libreadline-dev \
    qtbase5-dev qttools5-dev qttools5-dev-tools libqt5svg5-dev \
    libqt5x11extras5-dev libqt5x11extras5 \
    libxi-dev libxtst-dev \
    libssl-dev \
    libpcsclite-dev \
    dpkg-dev fakeroot file \
    ninja-build
  if [[ "${WITH_XC_DOCS}" == "1" ]]; then
    sudo apt-get install -y --no-install-recommends asciidoctor
  fi
fi

if ! command -v ninja >/dev/null 2>&1; then
  CMAKE_GENERATOR="${CMAKE_GENERATOR:-Unix Makefiles}"
else
  CMAKE_GENERATOR="${CMAKE_GENERATOR:-Ninja}"
fi

DOCS_FLAG="-DWITH_XC_DOCS=OFF"
[[ "${WITH_XC_DOCS}" == "1" ]] && DOCS_FLAG="-DWITH_XC_DOCS=ON"

if [[ "${SKIP_BUILD}" != "1" ]]; then
  echo "==> Configuration CMake (${CMAKE_GENERATOR}) dans ${BUILD_DIR}…"
  cmake -S "${ROOT}" -B "${BUILD_DIR}" -G "${CMAKE_GENERATOR}" \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCMAKE_INSTALL_PREFIX="${PREFIX}" \
    -DWITH_TESTS=OFF \
    -DWITH_GUI_TESTS=OFF \
    ${DOCS_FLAG} \
    ${EXTRA_CMAKE_ARGS:-}

  echo "==> Compilation (-j${JOBS})…"
  cmake --build "${BUILD_DIR}" -j"${JOBS}"
fi

echo "==> Installation dans le staging (${STAGING})…"
rm -rf "${STAGING}"
DESTDIR="${STAGING}" cmake --install "${BUILD_DIR}" --prefix "${PREFIX}"

mkdir -p "${STAGING}/DEBIAN"
INSTALLED_SIZE=0
shopt -s nullglob
for d in "${STAGING}"/*; do
  [[ "$(basename "$d")" == "DEBIAN" ]] && continue
  INSTALLED_SIZE=$((INSTALLED_SIZE + $(du -sk "$d" | cut -f1)))
done
shopt -u nullglob

# Fichiers ELF (exécutables et .so) pour dpkg-shlibdeps
SHLIB_BINS=()
while IFS= read -r -d '' f; do
  mt="$(file -b --mime-type "$f" 2>/dev/null || true)"
  if [[ "$mt" == "application/x-executable" || "$mt" == "application/x-pie-executable" || "$mt" == "application/x-sharedlib" ]]; then
    SHLIB_BINS+=("$f")
  fi
done < <(find "${STAGING}" -path "${STAGING}/DEBIAN" -prune -o -type f -print0 2>/dev/null)

DEPENDS_LINE=""
MULTIARCH="$(dpkg-architecture -qDEB_HOST_MULTIARCH 2>/dev/null || true)"
SUBST="${STAGING}/DEBIAN/substvars"
rm -f "${SUBST}"
if [[ ${#SHLIB_BINS[@]} -gt 0 ]] && command -v dpkg-shlibdeps >/dev/null 2>&1; then
  rel_bins=()
  for f in "${SHLIB_BINS[@]}"; do
    rel_bins+=("${f#"${STAGING}/"}")
  done
  set +e
  (
    cd "${STAGING}"
    if [[ -n "${MULTIARCH}" && -d "${PREFIX#/}/lib/${MULTIARCH}" ]]; then
      dpkg-shlibdeps -T"DEBIAN/substvars" -l"${PREFIX#/}/lib/${MULTIARCH}" "${rel_bins[@]}" 2>/dev/null
    else
      dpkg-shlibdeps -T"DEBIAN/substvars" "${rel_bins[@]}" 2>/dev/null
    fi
  )
  set -e
  if [[ -f "${SUBST}" ]]; then
    DEPENDS_LINE="$(grep -E '^shlibs:Depends=' "${SUBST}" | cut -d= -f2- || true)"
  fi
fi

if [[ -z "${DEPENDS_LINE}" ]]; then
  echo "Note: dpkg-shlibdeps a échoué ou est absent — utilisation d'une liste Depends générique."
  DEPENDS_LINE="libc6 (>= 2.31), libstdc++6 (>= 10), libgcc-s1, libqt5core5a | libqt5core5, libqt5gui5, libqt5widgets5, libqt5dbus5, libqt5network5, libqt5svg5, libqt5concurrent5, libqt5x11extras5, libbotan-2-19 | libbotan-2-18 | libbotan-2-17, libargon2-1, zlib1g, libqrencode4, libreadline8, libminizip1, libssl3 | libssl1.1, libx11-6, libxi6, libxtst6"
fi

cat > "${STAGING}/DEBIAN/control" <<EOF
Package: ${DEB_PACKAGE_NAME}
Version: ${PKG_VERSION}
Section: utils
Priority: optional
Architecture: $(dpkg --print-architecture)
Depends: ${DEPENDS_LINE}
Installed-Size: ${INSTALLED_SIZE}
Maintainer: Unofficial Build <root@localhost>
Homepage: https://keepassxc.org
Description: Gestionnaire de mots de passe KeePassXC (compilation locale)
 Paquet binaire produit par scripts/build-debian-package.sh à partir des sources.
 Basé sur KeePassXC ${BASE_VERSION}.
EOF

echo "==> Construction du paquet ${DEB_OUT}…"
if command -v fakeroot >/dev/null 2>&1; then
  fakeroot dpkg-deb --root-owner-group --build "${STAGING}" "${DEB_OUT}"
else
  dpkg-deb --root-owner-group --build "${STAGING}" "${DEB_OUT}"
fi

echo "==> Terminé : ${DEB_OUT}"
ls -lh "${DEB_OUT}"
