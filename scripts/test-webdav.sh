#!/usr/bin/env bash
# Runs tests/TestWebDav against a throwaway Apache WebDAV server (Docker, httpd:2.4).
#
# Usage : scripts/test-webdav.sh [commande...]   (défaut : build/tests/testwebdav)
#   La commande reçoit KPXC_TEST_WEBDAV_URL, _USER et _PASSWORD dans son environnement, par exemple
#   pour un binaire compilé dans un conteneur :
#   scripts/test-webdav.sh docker run --rm --network host -e KPXC_TEST_WEBDAV_URL -e KPXC_TEST_WEBDAV_USER \
#       -e KPXC_TEST_WEBDAV_PASSWORD -e QT_QPA_PLATFORM -v "$PWD/build:/build" image /build/tests/testwebdav
# Variables : WEBDAV_PORT (défaut 8099)
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PORT="${WEBDAV_PORT:-8099}"
NAME="kpxc-webdav-test-$$"
USER_NAME="kpxc"
PASSWORD="$(head -c 12 /dev/urandom | base64 | tr -dc 'A-Za-z0-9')"

if [[ $# -eq 0 ]]; then
    set -- "${ROOT}/build/tests/testwebdav"
fi

cleanup() { docker rm -f "${NAME}" >/dev/null 2>&1 || true; }
trap cleanup EXIT

docker run -d --name "${NAME}" -p "127.0.0.1:${PORT}:80" \
    -v "${ROOT}/tests/webdav/httpd.conf:/usr/local/apache2/conf/httpd.conf:ro" \
    httpd:2.4 sh -c "
        mkdir -p /srv/webdav/dav /srv/webdav/noetag /srv/webdav/nomove /srv/webdav/gzip /usr/local/apache2/var &&
        chown -R daemon:daemon /srv/webdav /usr/local/apache2/var &&
        htpasswd -bc /usr/local/apache2/conf/htpasswd '${USER_NAME}' '${PASSWORD}' &&
        exec httpd-foreground" >/dev/null

for _ in $(seq 1 30); do
    curl -s -o /dev/null "http://127.0.0.1:${PORT}/" && break
    sleep 0.5
done

export KPXC_TEST_WEBDAV_URL="http://127.0.0.1:${PORT}/"
export KPXC_TEST_WEBDAV_USER="${USER_NAME}"
export KPXC_TEST_WEBDAV_PASSWORD="${PASSWORD}"
export QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-offscreen}"
"$@"
