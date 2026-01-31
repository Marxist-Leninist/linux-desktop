#!/bin/bash
# install-deps.sh - Install all dependencies for linux-desktop build
set -e

echo "=== Installing linux-desktop dependencies ==="

# Check if running as root or with sudo
if [ "$EUID" -ne 0 ]; then
    SUDO="sudo"
else
    SUDO=""
fi

echo "[1/4] Updating system..."
$SUDO apt update && $SUDO apt upgrade -y

echo "[2/4] Installing build essentials..."
$SUDO apt install -y \
    build-essential git meson ninja-build cmake pkg-config \
    libncurses-dev flex bison libssl-dev libelf-dev bc

echo "[3/4] Installing GNOME/Wayland dependencies..."
$SUDO apt install -y \
    libwayland-dev wayland-protocols libxkbcommon-dev \
    libinput-dev libudev-dev libdrm-dev libgbm-dev \
    libx11-dev libxext-dev libxdamage-dev libxfixes-dev \
    libxrandr-dev libxcomposite-dev libxcursor-dev \
    libpango1.0-dev libcairo2-dev libgdk-pixbuf-2.0-dev \
    libatk1.0-dev libgtk-3-dev gobject-introspection \
    libgirepository1.0-dev libglib2.0-dev libdbus-1-dev \
    libpolkit-gobject-1-dev libsystemd-dev libpipewire-0.3-dev \
    libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev \
    libjson-glib-dev libgnome-desktop-4-dev libgcr-4-dev \
    libsecret-1-dev libnm-dev libpulse-dev libibus-1.0-dev \
    sassc gettext libxml2-utils xsltproc \
    libcolord-dev libcanberra-gtk3-dev libgudev-1.0-dev \
    libstartup-notification0-dev libxkbfile-dev \
    libgraphene-1.0-dev libepoxy-dev libegl1-mesa-dev

echo "[4/4] Installing Python for ML work..."
$SUDO apt install -y python3-pip python3-venv python3-dev

echo ""
echo "=== Dependencies installed successfully ==="
echo "Next: Run ./scripts/setup-cuda.sh (if you have NVIDIA GPU)"
echo "      Or:  ./scripts/build.sh (to build GNOME stack)"
