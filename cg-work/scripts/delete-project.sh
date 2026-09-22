#!/usr/bin/env bash
set -e

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PROJECTS_DIR="$ROOT/projects"
CMAKE_FILE="$ROOT/CMakeLists.txt"

if [ -z "$1" ]; then
    echo "Usage: ./scripts/delete-project.sh <exact-folder-name>"
    echo ""
    echo "Existing projects:"
    ls "$PROJECTS_DIR"
    exit 1
fi

TARGET="$1"
FOLDER="$PROJECTS_DIR/$TARGET"

# Exact match only — no guessing, no partial matches
if [ ! -d "$FOLDER" ]; then
    echo "Error: no project folder named '$TARGET' found in projects/"
    echo ""
    echo "Existing projects:"
    ls "$PROJECTS_DIR"
    exit 1
fi

# Check the CMakeLists.txt has the matching add_subdirectory line
CMAKE_LINE="add_subdirectory(projects/$TARGET)"
if ! grep -qF "$CMAKE_LINE" "$CMAKE_FILE"; then
    echo "Error: folder '$TARGET' exists but '$CMAKE_LINE' not found in CMakeLists.txt."
    echo "Manual inspection required — aborting."
    exit 1
fi

echo "About to permanently delete:"
echo "  Folder:      $FOLDER"
echo "  CMake line:  $CMAKE_LINE"
echo ""
echo "Type the project name exactly to confirm: "
read -r CONFIRM

if [ "$CONFIRM" != "$TARGET" ]; then
    echo "Name did not match. Aborting."
    exit 1
fi

# Remove the add_subdirectory line from CMakeLists.txt
# Use a temp file to avoid in-place sed platform differences
TMPFILE=$(mktemp)
grep -vF "$CMAKE_LINE" "$CMAKE_FILE" > "$TMPFILE"
mv "$TMPFILE" "$CMAKE_FILE"

# Delete the project folder
rm -rf "$FOLDER"

# Reconfigure so compile_commands.json and build tree no longer reference deleted project
cmake -S "$ROOT" -B "$ROOT/build"

echo ""
echo "Deleted:  projects/$TARGET"
echo "Removed:  $CMAKE_LINE from CMakeLists.txt"
echo "CMake reconfigured."
