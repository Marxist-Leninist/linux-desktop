#!/bin/bash
# install-kernel-gui.sh - Integrate kernel GUI subsystem into kernel source
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(dirname "$SCRIPT_DIR")"

echo "=== Kernel GUI Subsystem Integration ==="

# Find kernel source
if [ -n "$1" ]; then
    KERNEL_SRC="$1"
elif [ -d "/usr/src/linux-source-$(uname -r | cut -d- -f1)" ]; then
    KERNEL_SRC="/usr/src/linux-source-$(uname -r | cut -d- -f1)"
else
    echo "Kernel source not found. Downloading..."
    cd /tmp
    apt source linux-image-$(uname -r) 2>/dev/null || {
        echo "ERROR: Could not get kernel source."
        echo "Usage: $0 /path/to/kernel/source"
        exit 1
    }
    KERNEL_SRC=$(ls -d /tmp/linux-* 2>/dev/null | head -1)
fi

echo "Using kernel source: $KERNEL_SRC"

if [ ! -d "$KERNEL_SRC/kernel" ]; then
    echo "ERROR: Invalid kernel source directory (no kernel/ subdir)"
    exit 1
fi

echo ""
echo "[1/4] Copying kernel GUI subsystem..."
mkdir -p "$KERNEL_SRC/kernel/gui"
cp -v "$REPO_ROOT/kernel-gui/"*.c "$KERNEL_SRC/kernel/gui/"
cp -v "$REPO_ROOT/kernel-gui/Kconfig" "$KERNEL_SRC/kernel/gui/"
cp -v "$REPO_ROOT/kernel-gui/Makefile" "$KERNEL_SRC/kernel/gui/"

echo ""
echo "[2/4] Copying headers..."
mkdir -p "$KERNEL_SRC/include/linux/gui"
mkdir -p "$KERNEL_SRC/include/uapi/linux/gui"
cp -v "$REPO_ROOT/include/gui/"*.h "$KERNEL_SRC/include/linux/gui/" 2>/dev/null || true
cp -v "$REPO_ROOT/include/uapi/gui/"*.h "$KERNEL_SRC/include/uapi/linux/gui/" 2>/dev/null || true
cp -v "$REPO_ROOT/include/drm/"*.h "$KERNEL_SRC/include/drm/" 2>/dev/null || true

echo ""
echo "[3/4] Patching kernel Makefile..."
if ! grep -q "obj-y += gui/" "$KERNEL_SRC/kernel/Makefile"; then
    echo "obj-y += gui/" >> "$KERNEL_SRC/kernel/Makefile"
    echo "Added gui/ to kernel/Makefile"
else
    echo "kernel/Makefile already patched"
fi

echo ""
echo "[4/4] Patching kernel Kconfig..."
KCONFIG="$KERNEL_SRC/kernel/Kconfig"
if ! grep -q "source \"kernel/gui/Kconfig\"" "$KCONFIG"; then
    echo "" >> "$KCONFIG"
    echo "source \"kernel/gui/Kconfig\"" >> "$KCONFIG"
    echo "Added gui/Kconfig to kernel/Kconfig"
else
    echo "kernel/Kconfig already patched"
fi

echo ""
echo "=== Integration complete ==="
echo ""
echo "Next steps:"
echo ""
echo "1. Configure kernel:"
echo "   cd $KERNEL_SRC"
echo "   cp /boot/config-\$(uname -r) .config"
echo "   make olddefconfig"
echo ""
echo "2. Enable GUI subsystem (add to .config or use 'make menuconfig'):"
echo "   CONFIG_GUI_COMPOSITOR=y"
echo "   CONFIG_GUI_INPUT_LOWLATENCY=y"
echo "   CONFIG_GUI_GESTURES=y"
echo "   CONFIG_GUI_FRAME_PACING=y"
echo "   CONFIG_DRM_COMPOSITOR_EFFECTS=y"
echo ""
echo "3. Build kernel:"
echo "   make -j\$(nproc) bindeb-pkg"
echo ""
echo "4. Install kernel:"
echo "   sudo dpkg -i ../linux-image-*.deb ../linux-headers-*.deb"
echo "   sudo reboot"
