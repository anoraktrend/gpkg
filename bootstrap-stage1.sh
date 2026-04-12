#!/bin/sh

# gpkg bootstrap-stage1 script - POSIX shell compliant
# Follows EE Ethos - Minimalist & Portable
# 
# Goal: Use host system tools (clang, cmake, etc.) to build gpkg packages.

set -e

# Configuration
REPO_DIR="repo-stage1"
STAGE1_ROOT="$(pwd)/stage1-root"
export GPKG_ROOT="$STAGE1_ROOT"
export PATH="$STAGE1_ROOT/usr/bin:$PATH"
export PKG_CONFIG_PATH="$STAGE1_ROOT/usr/lib/pkgconfig:$STAGE1_ROOT/usr/share/pkgconfig"
export PKG_CONFIG_LIBDIR="$STAGE1_ROOT/usr/lib/pkgconfig:$STAGE1_ROOT/usr/share/pkgconfig"
export PKG_CONFIG_SYSROOT_DIR="/"

# Ensure gpkg itself is built and ready
make all

# Build order (Dependencies first)
PACKAGES="
    linux-headers
    musl
    gettext-tiny
    libexecinfo
    zlib
    ncurses
    openssl
    libffi
    pcre2
    json-c
    pkgconf
    scdoc
    libbsd
    libxml2
    bmake
    gmake
    m4
    byacc
    flex
    gperf
    samu
    cmake
    flit-core
    markupsafe
    jinja2
    meson
    glib
    pixman
    freetype
    fontconfig
    harfbuzz
    libexpat
    wayland
    aria2
    curl
    git
    rnp
    llvm
    busybox
    toybox
    mksh
"

mkdir -p "$STAGE1_ROOT"
mkdir -p db/installed # Ensure local database is ready

echo "Starting Stage1 Bootstrap..."
echo "Stage1 Root: $STAGE1_ROOT"

for pkg in $PACKAGES; do
    # Check if package is already installed
    pkgname=$(grep '^pkgname=' "$REPO_DIR/$pkg/PKGBUILD" | cut -d'"' -f2)
    pkgver=$(grep '^pkgver=' "$REPO_DIR/$pkg/PKGBUILD" | cut -d'"' -f2)
    pkgname_ver="$pkgname-$pkgver"
    
    if [ -d "db/installed/$pkgname_ver" ]; then
        echo "Package $pkgname_ver is already installed, skipping..."
        continue
    fi

    echo "========================================"
    echo "Building $pkg..."
    echo "========================================"
    
    # 1. Build the package
    ./gpkg build "$REPO_DIR/$pkg"
    
    # 2. Get the built filename
    pkgfile=$(ls -t $pkgname-$pkgver*.gpkg.tar.gz 2>/dev/null | head -n 1)
    
    if [ -z "$pkgfile" ]; then
        echo "Error: Package file for $pkgname-$pkgver not found."
        exit 1
    fi
    
    # 3. Install it into Stage1 Root
    ./gpkg install "$pkgfile"
    
    echo "Done building $pkg."
done

echo "========================================"
echo "Stage1 Bootstrap Complete!"
echo "Binaries available in: $STAGE1_ROOT/usr/bin"
echo "========================================"
