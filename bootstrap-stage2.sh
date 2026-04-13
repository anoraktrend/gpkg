#!/bin/sh

# gpkg bootstrap-stage2 script - POSIX shell compliant
# Follows EE Ethos - Minimalist & Portable
# 
# Goal: Use Stage1 tools (clang, samu, etc.) to build gpkg packages again.

set -e

# Configuration
REPO_DIR="repo-stage2"
STAGE1_ROOT="$(pwd)/stage1-root"
STAGE2_ROOT="$(pwd)/stage2-root"

# PATH priority: Stage1 tools first
export PATH="$STAGE1_ROOT/usr/bin:/usr/bin:/bin"
export GPKG_ROOT="$STAGE2_ROOT"

# Compiler configuration (Use Stage1 Clang)
export CC="clang"
export CXX="clang++"
export AR="llvm-ar"
export NM="llvm-nm"
export RANLIB="llvm-ranlib"

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

mkdir -p "$STAGE2_ROOT"
mkdir -p db/installed # Reuse db but root will be different

echo "Starting Stage2 Bootstrap..."
echo "Using Stage1 Root: $STAGE1_ROOT"
echo "Stage2 Root: $STAGE2_ROOT"

export GPKG_REPO="repo-stage2"

for pkg in $PACKAGES; do
    pkgname=$(grep '^pkgname=' "$REPO_DIR/$pkg/PKGBUILD" | cut -d'"' -f2)
    pkgver=$(grep '^pkgver=' "$REPO_DIR/$pkg/PKGBUILD" | cut -d'"' -f2)
    pkgname_ver="$pkgname-$pkgver"
    
    if [ -d "db/$GPKG_REPO/installed/$pkgname_ver" ]; then
        echo "Package $pkgname_ver is already installed in $GPKG_REPO, skipping..."
        continue
    fi

    echo "========================================"
    echo "Building $pkg (Stage2)..."
    echo "========================================"
    
    # 1. Build the package
    ./gpkg build "$REPO_DIR/$pkg"
    
    # 2. Get the built filename
    pkgfile=$(ls -t $pkgname-$pkgver*.gpkg 2>/dev/null | head -n 1)
    
    if [ -z "$pkgfile" ]; then
        echo "Error: Package file for $pkgname-$pkgver not found."
        exit 1
    fi
    
    # 3. Install it into Stage2 Root
    ./gpkg install "$pkgfile"
    
    echo "Done building $pkg."
done

echo "========================================"
echo "Stage2 Bootstrap Complete!"
echo "Binaries available in: $STAGE2_ROOT/usr/bin"
echo "========================================"
