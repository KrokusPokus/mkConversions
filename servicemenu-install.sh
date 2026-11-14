#!/bin/bash
# Installiert die Dolphin-Kontextmenüs für den aktuellen Benutzer:
#   - "Formate konvertieren"                    (Dateien und Ordner)
#   - "Formate konvertieren (inkl. Unterordner)" (nur Ordner)
# Zwei getrennte .desktop-Dateien, da MimeType= für alle Actions einer Datei gemeinsam gilt.
# Voraussetzung: mkConversions wurde gebaut (build.sh kopiert es nach ~/.local/bin).

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
