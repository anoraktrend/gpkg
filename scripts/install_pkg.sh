#!/bin/sh

# gpkg install script - POSIX shell compliant
# Follows EE Ethos - Minimalist & Portable

set -e

if [ $# -lt 1 ]; then
    echo "Usage: $0 <pkgfile>"
    exit 1
fi

pkgfile=$1

if [ ! -f "$pkgfile" ]; then
    echo "Error: Package file '$pkgfile' not found."
    exit 1
fi

# Extract package name and version from filename
# Assuming format: pkgname-pkgver.gpkg.tar.gz
filename=$(basename "$pkgfile")
pkgname_ver=$(echo "$filename" | sed 's/\.gpkg\.tar\.gz$//')

echo "Installing $pkgname_ver..."

# Destination root (default to /)
# For testing, we might want to install to a local root
root=${GPKG_ROOT:-/}

# 1. Extract files and record them
# We use tar -t to get a list of files for the database
db_dir="db/installed/$pkgname_ver"
mkdir -p "$db_dir"

# Extract to root
tar -xzf "$pkgfile" -C "$root"

# Record file list
tar -tf "$pkgfile" > "$db_dir/files"

echo "$pkgname_ver installed successfully into $root."
