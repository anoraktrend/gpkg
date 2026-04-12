#!/bin/sh

# gpkg remove script - POSIX shell compliant
# Follows EE Ethos - Minimalist & Portable

set -e

if [ $# -lt 1 ]; then
    echo "Usage: $0 <pkgname_ver>"
    exit 1
fi

pkgname_ver=$1
db_dir="db/installed/$pkgname_ver"

if [ ! -d "$db_dir" ]; then
    echo "Error: Package '$pkgname_ver' not found in database."
    exit 1
fi

echo "Removing $pkgname_ver..."

# Root for removal
root=${GPKG_ROOT:-/}

files_list="$db_dir/files"

# 1. Remove files and then directories in reverse order
# We use a temporary file to store the reversed list
temp_list=$(mktemp)
# POSIX way to reverse lines: sed '1!G;h;$!d'
# Or just use an array in shell (too complex)
# Let's just use a simple approach: 
#   1. Remove files
#   2. Remove empty directories in reverse order

while IFS= read -r file; do
    full_path="$root/$file"
    if [ -f "$full_path" ] || [ -L "$full_path" ]; then
        rm -f "$full_path"
    fi
done < "$files_list"

# Now try to remove directories in reverse order
# We'll use a simple trick to reverse: sort -r
# (Assuming file paths don't contain newlines)
sort -r "$files_list" | while IFS= read -r file; do
    full_path="$root/$file"
    if [ -d "$full_path" ]; then
        # Use 2>/dev/null to silence errors if directory is not empty
        rmdir "$full_path" 2>/dev/null || true
    fi
done

# 3. Remove database entry
rm -rf "$db_dir"

echo "$pkgname_ver removed successfully."
