#!/bin/bash
# Entfernt die Dolphin-Kontextmenüs wieder.

rm -f "$HOME/.local/share/kio/servicemenus/local.mkconversions.desktop"
rm -f "$HOME/.local/share/kio/servicemenus/local.mkconversions-recursive.desktop"
rm -f "$HOME/.local/share/icons/hicolor/scalable/apps/mkConversions.svg"
echo "Removed."
