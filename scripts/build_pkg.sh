#!/bin/sh

# gpkg build script - POSIX shell compliant
# Follows EE Ethos - Minimalist & Portable

set -e

if [ $# -lt 1 ]; then
    echo "Usage: $0 <pkgdir>"
    exit 1
fi

pkgdir=$1

if [ ! -f "$pkgdir/PKGBUILD" ]; then
    echo "Error: PKGBUILD not found in $pkgdir"
    exit 1
fi

# Source the PKGBUILD
. "$pkgdir/PKGBUILD"

echo "Building package: $pkgname-$pkgver"

# 1. Handle sources
if [ -n "$source" ]; then
    echo "Fetching sources for $pkgname..."
    mkdir -p "src-cache"
    
    # We use aria2c for high-performance downloading
    # --dir sets the download directory
    # --out sets the filename
    # -x16 uses 16 connections for speed
    for src_url in $(echo "$source" | tr ';' ' '); do
        src_file=$(basename "$src_url")
        if [ ! -f "src-cache/$src_file" ]; then
            echo "Downloading $src_url..."
            if command -v aria2c >/dev/null 2>&1; then
                aria2c -d "src-cache" -o "$src_file" -x16 "$src_url"
            else
                echo "Warning: aria2c not found, falling back to curl..."
                curl -L -o "src-cache/$src_file" "$src_url"
            fi
        fi
        
        echo "Extracting $src_file..."
        # Extract into the package directory
        # First, find and remove potential previous extraction to ensure clean patching
        src_base=$(echo "$src_file" | sed -E 's/\.(tar\.gz|tgz|tar\.xz|tar\.bz2|zip)$//')
        rm -rf "$pkgdir/$src_base"
        
        case "$src_file" in
            *.tar.gz|*.tgz) tar -xzf "src-cache/$src_file" -C "$pkgdir" ;;
            *.tar.xz) tar -xJf "src-cache/$src_file" -C "$pkgdir" ;;
            *.tar.bz2) tar -xjf "src-cache/$src_file" -C "$pkgdir" ;;
            *.zip) unzip -q "src-cache/$src_file" -d "$pkgdir" ;;
            *) echo "Warning: Unknown file type for $src_file" ;;
        esac
    done
fi

# 2. Apply patches
if [ -d "$pkgdir/patches" ]; then
    echo "Applying patches for $pkgname..."
    for patch in "$pkgdir/patches"/*.patch; do
        [ -f "$patch" ] || continue
        echo "Applying $(basename "$patch")..."
        # Try to find the actual source directory (e.g. cmake-3.30.2)
        # Look for a directory that contains common markers like 'Source' or 'src' or 'README'
        src_dir=$(find "$pkgdir" -maxdepth 2 -name "Source" -o -name "src" -o -name "README*" | head -n 1 | xargs dirname)
        if [ -z "$src_dir" ] || [ "$src_dir" = "." ]; then
             src_dir=$(find "$pkgdir" -maxdepth 1 -type d | grep -v "^$pkgdir$" | head -n 1)
        fi
        
        if [ -n "$src_dir" ]; then
            echo "Patching in $src_dir..."
            patch -p1 -d "$src_dir" < "$patch"
        else
            patch -p1 -d "$pkgdir" < "$patch"
        fi
    done
fi

# Create a clean destination directory
destdir="pkg-$pkgname"
rm -rf "$destdir"
mkdir -p "$destdir"

# 2. Run build function if it exists
if type build >/dev/null 2>&1; then
    (cd "$pkgdir" && build)
fi

# 3. Run package function with DESTDIR set
if type package >/dev/null 2>&1; then
    DESTDIR="$(pwd)/$destdir"
    export DESTDIR
    (cd "$pkgdir" && package)
fi

# 4. Create the package tarball
pkgfile="$pkgname-$pkgver.gpkg.tar.gz"
tar -czf "$pkgfile" -C "$destdir" .

echo "Package $pkgfile created successfully."
