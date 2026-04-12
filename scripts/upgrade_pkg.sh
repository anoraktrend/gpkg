#!/bin/sh

# gpkg upgrade script - POSIX shell compliant
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

# For now, upgrade is similar to install but might handle more logic later
# e.g., checking versions, running pre/post upgrade scripts.

echo "Upgrading using $pkgfile..."

# Call install script
# Note: install_pkg.sh currently overwrites and updates the file list.
$(dirname "$0")/install_pkg.sh "$pkgfile"

echo "Upgrade successful."
