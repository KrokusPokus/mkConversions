#!/bin/bash
# Installs the Dolphin context menus for the current user:
#   - "Convert formats"                         (files and folders)
#   - "Convert formats (including subfolders)"  (folders only)
# Two separate .desktop files, because MimeType= applies to all actions of a file.
# Prerequisite: mkConversions has been built (build.sh copies it to ~/.local/bin).

set -e
cd "$(dirname "$0")"

EXEC="$HOME/.local/bin/mkConversions"
ICON_DIR="$HOME/.local/share/icons/hicolor/scalable/apps"
MENU_DIR="$HOME/.local/share/kio/servicemenus"

mkdir -p "$ICON_DIR" "$MENU_DIR"
cp res/mkConversions.svg "$ICON_DIR/mkConversions.svg"

for f in local.mkconversions.desktop local.mkconversions-recursive.desktop; do
    sed -e "s|@EXEC@|$EXEC|g" -e "s|@ICON@|mkConversions|g" "res/$f" > "$MENU_DIR/$f"
    chmod +x "$MENU_DIR/$f"
    echo "Installed: $MENU_DIR/$f"
done
