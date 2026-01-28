#!/bin/sh
# install-xlibre.sh
# Clean Xorg → Xlibre replacement for Debian-family systems

set -e

echo "=== Xlibre installation starting ==="

# Safety check
if [ "$(id -u)" -ne 0 ]; then
    echo "ERROR: Run this script as root (sudo ./install_xlibre.sh)"
    exit 1
fi

echo "[1/9] Backing up existing X11 config (if present)"
if [ -d /etc/X11 ]; then
    cp -a /etc/X11 /etc/X11.backup.$(date +%Y%m%d%H%M%S)
fi

echo "[2/9] Removing Xorg (core only)"
apt remove --purge -y \
    xserver-xorg \
    xserver-xorg-core || true

echo "[3/9] Installing Xlibre repository key"
curl -fsSL https://xlibre.org/repo/pubkey.gpg \
    | gpg --dearmor \
    > /usr/share/keyrings/xlibre.gpg

chmod 644 /usr/share/keyrings/xlibre.gpg

echo "[4/9] Adding Xlibre repository"
cat > /etc/apt/sources.list.d/xlibre.list <<EOF
deb [signed-by=/usr/share/keyrings/xlibre.gpg] https://xlibre.org/repo/debian stable main
EOF

echo "[5/9] Updating package lists"
apt update

echo "[6/9] Installing Xlibre core"
apt install -y \
    xlibre-core \
    xlibre-xserver

echo "[7/9] Installing minimal input + video drivers"
apt install -y \
    xlibre-input-libinput \
    xlibre-video-modesetting

echo "[8/9] Setting Xlibre as default X server"
if command -v update-alternatives >/dev/null 2>&1; then
    update-alternatives --set x-server /usr/bin/X.xlibre || true
fi

echo "[9/9] Preventing accidental Xorg reinstall"
apt-mark hold \
    xserver-xorg \
    xserver-xorg-core || true

echo " "
echo "=== Xlibre installation complete ==="
