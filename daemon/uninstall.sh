#!/bin/bash
# Remove o daemon do Craudiômetro (LaunchAgent + arquivos instalados).
set -euo pipefail

PLIST_LABEL="com.craudiometro.daemon"
PLIST_PATH="$HOME/Library/LaunchAgents/$PLIST_LABEL.plist"
INSTALL_DIR="$HOME/.craudiometro"

echo "-> descarregando LaunchAgent"
launchctl unload "$PLIST_PATH" 2>/dev/null || true
rm -f "$PLIST_PATH"

echo "-> removendo $INSTALL_DIR (inclui o bundle Craudiometro.app)"
rm -rf "$INSTALL_DIR"

echo "-> pronto. Logs em ~/Library/Logs/craudiometro*.log foram mantidos."
