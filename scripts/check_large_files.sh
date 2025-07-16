#!/bin/bash

# Script to check for large files in the repository and ensure they use Git LFS
# This prevents accidentally committing large files that should be stored in LFS

set -e

# Configuration
MAX_FILE_SIZE_KB=200
MAX_FILE_SIZE_BYTES=$((MAX_FILE_SIZE_KB * 1024))
TEMP_FILE="large_files_check.txt"

echo "=========================================="
echo "Checking for large files without Git LFS"
echo "Maximum allowed file size: ${MAX_FILE_SIZE_KB}KB"
echo "=========================================="

# Clean up function
cleanup() {
    rm -f "$TEMP_FILE"
}
trap cleanup EXIT

# Check if git-lfs is installed
if ! command -v git-lfs &> /dev/null; then
    echo "Warning: git-lfs is not installed. Please install Git LFS to handle large files."
    echo "Visit: https://git-lfs.github.io/"
fi

# Function to check if a file is tracked by Git LFS
is_tracked_by_lfs() {
    local file="$1"
    # Check if file has LFS pointer content
    if [ -f "$file" ]; then
        # LFS files start with "version https://git-lfs.github.com/spec/v1"
        if head -1 "$file" 2>/dev/null | grep -q "version https://git-lfs.github.com/spec/v1"; then
            return 0
        fi

        # Also check git-lfs ls-files if git-lfs is available
        if command -v git-lfs &> /dev/null; then
            if git lfs ls-files | grep -q "$(basename "$file")"; then
                return 0
            fi
        fi
    fi
    return 1
}

# Function to get human readable file size
human_readable_size() {
    local size_bytes=$1
    if [ $size_bytes -gt $((1024 * 1024 * 1024)) ]; then
        echo "$(( size_bytes / 1024 / 1024 / 1024 ))GB"
    elif [ $size_bytes -gt $((1024 * 1024)) ]; then
        echo "$(( size_bytes / 1024 / 1024 ))MB"
    elif [ $size_bytes -gt 1024 ]; then
        echo "$(( size_bytes / 1024 ))KB"
    else
        echo "${size_bytes}B"
    fi
}

# Initialize counters and arrays
large_files_count=0
large_files_without_lfs=()

# Check if we're in a git repository
if ! git rev-parse --git-dir > /dev/null 2>&1; then
    echo "Error: Not in a git repository"
    exit 1
fi

# Get all tracked files in the repository
echo "Checking all tracked files in the repository..."
FILES_TO_CHECK=$(git ls-files)

if [ -z "$FILES_TO_CHECK" ]; then
    echo "No files to check."
    exit 0
fi

echo "Checking $(echo "$FILES_TO_CHECK" | wc -l) files..."

# Check each file
while IFS= read -r file; do
    # Skip if file doesn't exist (might be deleted)
    if [ ! -f "$file" ]; then
        continue
    fi

    # Get file size
    file_size=$(stat -f%z "$file" 2>/dev/null || stat -c%s "$file" 2>/dev/null || echo 0)

    # Check if file is larger than threshold
    if [ "$file_size" -gt "$MAX_FILE_SIZE_BYTES" ]; then
        large_files_count=$((large_files_count + 1))
        human_size=$(human_readable_size "$file_size")

        echo "Found large file: $file ($human_size)"

        # Check if it's tracked by LFS
        if is_tracked_by_lfs "$file"; then
            echo "  ✓ File is tracked by Git LFS"
        else
            echo "  ✗ File is NOT tracked by Git LFS"
            large_files_without_lfs+=("$file ($human_size)")
        fi
    fi
done <<< "$FILES_TO_CHECK"

# Report results
echo ""
echo "=========================================="
echo "Large Files Check Results"
echo "=========================================="

if [ $large_files_count -eq 0 ]; then
    echo "✓ No large files found. All good!"
    exit 0
fi

echo "Found $large_files_count large file(s) (> ${MAX_FILE_SIZE_KB}KB)"

if [ ${#large_files_without_lfs[@]} -eq 0 ]; then
    echo "✓ All large files are properly tracked by Git LFS"
    exit 0
fi

# Error: found large files without LFS
echo ""
echo "✗ ERROR: Found ${#large_files_without_lfs[@]} large file(s) that are NOT tracked by Git LFS:"
echo ""

for file_info in "${large_files_without_lfs[@]}"; do
    echo "  - $file_info"
done

echo ""
echo "To fix this issue:"
echo "1. Install Git LFS if not already installed:"
echo "   git lfs install"
echo ""
echo "2. Track the large file types with LFS:"
echo "   git lfs track \"*.zip\""
echo "   git lfs track \"*.tar.gz\""
echo "   git lfs track \"*.bin\""
echo "   git lfs track \"*.model\""
echo "   # Add patterns for your specific file types"
echo ""
echo "3. Add the .gitattributes file:"
echo "   git add .gitattributes"
echo ""
echo "4. Re-add your large files:"
echo "   git rm --cached <large-file>"
echo "   git add <large-file>"
echo ""
echo "For more information, visit: https://git-lfs.github.io/"
echo ""

exit 1
