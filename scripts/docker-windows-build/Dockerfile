# Image pour produire un KeePassXC.exe (MinGW 64 bits) depuis Linux via vcpkg.
# Le build officiel des releases utilise MSVC sur Windows ; ici c'est une alternative
# « tout-en-un » pour Docker sur Linux (ou macOS avec Docker Desktop).
#
# Dépendances système pour la chaîne MinGW + outils dont certains ports vcpkg ont besoin.
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive
ENV LANG=C.UTF-8

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    ninja-build \
    git \
    curl \
    zip \
    unzip \
    tar \
    ca-certificates \
    pkg-config \
    python3 \
    flex \
    bison \
    gperf \
    perl \
    cmake \
    gcc-mingw-w64-x86-64 \
    g++-mingw-w64-x86-64 \
    mingw-w64-tools \
    && rm -rf /var/lib/apt/lists/*

# Même commit que .github/workflows/release.yml (vcpkg.json / baseline).
ARG VCPKG_COMMIT=66c0373dc7fca549e5803087b9487edfe3aca0a1
ENV VCPKG_ROOT=/opt/vcpkg

RUN git clone https://github.com/microsoft/vcpkg.git "${VCPKG_ROOT}" \
    && cd "${VCPKG_ROOT}" \
    && git checkout "${VCPKG_COMMIT}" \
    && ./bootstrap-vcpkg.sh

COPY build-inside.sh /usr/local/bin/build-inside.sh
RUN chmod +x /usr/local/bin/build-inside.sh

WORKDIR /src
ENTRYPOINT ["/usr/local/bin/build-inside.sh"]
