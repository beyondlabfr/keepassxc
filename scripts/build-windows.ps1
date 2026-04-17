param(
    # Path to your vcpkg installation root (contains scripts/buildsystems/vcpkg.cmake)
    [string]$VcpkgRoot = "C:\vcpkg",

    # Build directory (relative to repo root unless absolute)
    [string]$BuildDir = "build",

    # Build type: Release / RelWithDebInfo / Debug
    [ValidateSet("Release", "RelWithDebInfo", "Debug")]
    [string]$BuildType = "Release",

    # KeePassXC build flavor (controls stability warnings in-app): Snapshot / PreRelease / Release
    [ValidateSet("Snapshot", "PreRelease", "Release")]
    [string]$KeePassXCBuildType = "Snapshot",

    # Convenience switch: sets -KeePassXCBuildType Release
    [switch]$ReleaseMode,

    # Ninja parallelism
    [int]$Jobs = 8,

    # If set, deletes the build directory first
    [switch]$Clean,

    # If set, runs CPack to create ZIP and MSI after build (requires WiX toolset for MSI)
    [switch]$Package,

    # Path to vcvars64.bat (adjust if your VS install differs)
    [string]$VcVars64 = "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
)

$ErrorActionPreference = "Stop"

if ($ReleaseMode) {
    $KeePassXCBuildType = "Release"
}

function Resolve-RepoRoot {
    $here = Split-Path -Parent $PSCommandPath
    return (Resolve-Path (Join-Path $here "..")).Path
}

$repoRoot = Resolve-RepoRoot
Set-Location $repoRoot

$toolchain = Join-Path $VcpkgRoot "scripts\buildsystems\vcpkg.cmake"
if (-not (Test-Path $toolchain)) {
    throw "vcpkg toolchain not found: $toolchain"
}
if (-not (Test-Path $VcVars64)) {
    throw "vcvars64.bat not found: $VcVars64"
}

$buildPath = if ([System.IO.Path]::IsPathRooted($BuildDir)) { $BuildDir } else { Join-Path $repoRoot $BuildDir }

if ($Clean -and (Test-Path $buildPath)) {
    Remove-Item -Recurse -Force $buildPath
}
New-Item -ItemType Directory -Force -Path $buildPath | Out-Null

$cmakeArgs = @(
    "-G", "Ninja",
    "-DWITH_XC_ALL=ON",
    "-DKEEPASSXC_BUILD_TYPE=$KeePassXCBuildType",
    "-DCMAKE_BUILD_TYPE=$BuildType",
    "-DCMAKE_TOOLCHAIN_FILE=$toolchain",
    ".."
)

Write-Host "Repo: $repoRoot"
Write-Host "Build: $buildPath"
Write-Host "Type: $BuildType"
Write-Host "KeePassXC build: $KeePassXCBuildType"
Write-Host "vcpkg: $VcpkgRoot"
Write-Host "Package: $Package"
Write-Host ""

# Remove prior packaging output to avoid confusion
if ($Package) {
    $cpackDir = Join-Path $buildPath "_CPack_Packages"
    if (Test-Path $cpackDir) {
        Remove-Item -Recurse -Force $cpackDir
    }
    Get-ChildItem -Path $buildPath -Filter "KeePassXC-*.zip" -ErrorAction SilentlyContinue | Remove-Item -Force -ErrorAction SilentlyContinue
    Get-ChildItem -Path $buildPath -Filter "KeePassXC-*.msi" -ErrorAction SilentlyContinue | Remove-Item -Force -ErrorAction SilentlyContinue
}

# Run everything under cmd so vcvars64.bat affects the environment.
$cmd = @(
    "cd /d `"$buildPath`"",
    "call `"$VcVars64`" >nul",
    "cmake $($cmakeArgs -join ' ')",
    "ninja -j$Jobs"
) -join " && "

cmd.exe /c $cmd

if ($Package) {
    # Use WIX generator to create an MSI (WiX toolset must be installed).
    $pkgCmd = @(
        "cd /d `"$buildPath`"",
        "call `"$VcVars64`" >nul",
        "cpack -G `"ZIP;WIX`""
    ) -join " && "
    cmd.exe /c $pkgCmd
}
