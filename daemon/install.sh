#!/bin/bash
# Instala o daemon do Craudiômetro como LaunchAgent do usuario.
#
# O venv (com --copies) vive DENTRO de um .app bundle minimo cujo Info.plist
# declara NSBluetoothAlwaysUsageDescription: sem isso, o CoreBluetooth aborta
# (SIGABRT) qualquer processo sem bundle iniciado pelo launchd, sem nem exibir
# o prompt de permissao. Com o bundle, o macOS mostra o prompt uma unica vez.
#
# Interpretador configuravel: PYTHON=/usr/bin/python3 ./install.sh
set -euo pipefail

INSTALL_DIR="$HOME/.craudiometro"
APP_DIR="$INSTALL_DIR/Craudiometro.app"
MACOS_DIR="$APP_DIR/Contents/MacOS"
VENV_DIR="$MACOS_DIR/venv"
PLIST_LABEL="com.craudiometro.daemon"
PLIST_PATH="$HOME/Library/LaunchAgents/$PLIST_LABEL.plist"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PYTHON="${PYTHON:-python3}"

# O bundle (Info.plist), o venv e a assinatura so sao criados uma vez:
# reescrever o plist ou o binario do Python quebraria o selo da assinatura e
# invalidaria a permissao de Bluetooth ja concedida no TCC. Reinstalacoes
# apenas atualizam dependencias e arquivos fora do bundle.
if [ ! -x "$VENV_DIR/bin/python3" ]; then
    echo "-> criando bundle em $APP_DIR"
    mkdir -p "$MACOS_DIR"
    cat > "$APP_DIR/Contents/Info.plist" <<'PLIST'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN"
  "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleName</key>
    <string>Craudiometro</string>
    <key>CFBundleIdentifier</key>
    <string>com.craudiometro.daemon</string>
    <key>CFBundleExecutable</key>
    <string>python3</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>CFBundleShortVersionString</key>
    <string>1.0.0</string>
    <key>NSBluetoothAlwaysUsageDescription</key>
    <string>O Craudiômetro envia o uso do Claude Code ao dispositivo ESP32 via Bluetooth.</string>
</dict>
</plist>
PLIST
    echo "-> criando venv em $VENV_DIR (com $PYTHON)"
    "$PYTHON" -m venv --copies "$VENV_DIR"
    "$VENV_DIR/bin/python" -m pip install --quiet --upgrade pip
    "$VENV_DIR/bin/pip" install --quiet -r "$SCRIPT_DIR/requirements.txt"
    echo "-> assinatura ad-hoc do bundle"
    codesign --force --deep -s - "$APP_DIR" 2>/dev/null || true
else
    echo "-> venv existente: atualizando dependencias"
    "$VENV_DIR/bin/pip" install --quiet -r "$SCRIPT_DIR/requirements.txt"
fi

# O launchd deve executar o binario REAL (nao um symlink): o TCC atribui a
# permissao pelo caminho resolvido do executavel, que precisa estar dentro
# do bundle para o Info.plist ser encontrado.
PYEXEC="$(readlink -f "$VENV_DIR/bin/python3")"

echo "-> copiando daemon e icones do tray para $INSTALL_DIR"
cp "$SCRIPT_DIR/craudiometro_daemon.py" "$INSTALL_DIR/"
cp "$SCRIPT_DIR/../assets/tray/"craudinho_*.png "$INSTALL_DIR/" 2>/dev/null || true

echo "-> registrando LaunchAgent $PLIST_LABEL"
mkdir -p "$HOME/Library/LaunchAgents"
cat > "$PLIST_PATH" <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN"
  "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>Label</key>
    <string>$PLIST_LABEL</string>
    <key>ProgramArguments</key>
    <array>
        <string>$PYEXEC</string>
        <string>$INSTALL_DIR/craudiometro_daemon.py</string>
    </array>
    <key>RunAtLoad</key>
    <true/>
    <key>KeepAlive</key>
    <true/>
    <key>ThrottleInterval</key>
    <integer>15</integer>
    <key>StandardOutPath</key>
    <string>$HOME/Library/Logs/craudiometro.launchd.log</string>
    <key>StandardErrorPath</key>
    <string>$HOME/Library/Logs/craudiometro.launchd.log</string>
</dict>
</plist>
PLIST

launchctl unload "$PLIST_PATH" 2>/dev/null || true
launchctl load "$PLIST_PATH"

echo "-> instalado. Logs: ~/Library/Logs/craudiometro.log"
echo "   Aprove o prompt de Bluetooth do 'Craudiômetro' na primeira execucao."
