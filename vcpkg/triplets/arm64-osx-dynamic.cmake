set(VCPKG_TARGET_ARCHITECTURE arm64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE dynamic)

set(VCPKG_CMAKE_SYSTEM_NAME Darwin)
set(VCPKG_OSX_ARCHITECTURES arm64)

set(VCPKG_OSX_DEPLOYMENT_TARGET 11.0)

# Qt checks the Xcode version, which fails with the Command Line Tools alone (no Xcode installed)
list(APPEND VCPKG_CMAKE_CONFIGURE_OPTIONS -DQT_NO_XCODE_MIN_VERSION_CHECK=ON)
