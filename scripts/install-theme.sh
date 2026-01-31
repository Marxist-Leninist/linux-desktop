#!/bin/bash
# install-theme.sh - Install Fluent theme and Win11 shell extension
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(dirname "$SCRIPT_DIR")"

echo "=== Installing Fluent Theme for linux-desktop ==="

# Theme directories
GTK4_USER="$HOME/.config/gtk-4.0"
SHELL_USER="$HOME/.local/share/themes/Fluent/gnome-shell"
EXTENSION_DIR="$HOME/.local/share/gnome-shell/extensions/win11-shell@linux-desktop"

# System-wide locations (require sudo)
GTK4_SYSTEM="/usr/share/themes/Fluent/gtk-4.0"
SHELL_SYSTEM="/usr/share/themes/Fluent/gnome-shell"

echo "[1/4] Installing GTK4 theme..."
mkdir -p "$GTK4_USER"
cp "$REPO_ROOT/theme/gtk-4.0/gtk.css" "$GTK4_USER/gtk.css"
echo "GTK4 theme installed to $GTK4_USER"

echo "[2/4] Installing GNOME Shell theme..."
mkdir -p "$SHELL_USER"
cp "$REPO_ROOT/theme/gnome-shell/gnome-shell.css" "$SHELL_USER/gnome-shell.css"
echo "GNOME Shell theme installed to $SHELL_USER"

echo "[3/4] Installing Win11 Shell extension..."
mkdir -p "$EXTENSION_DIR"
cp "$REPO_ROOT/extensions/win11-shell/metadata.json" "$EXTENSION_DIR/"
cp "$REPO_ROOT/extensions/win11-shell/extension.js" "$EXTENSION_DIR/"
cp "$REPO_ROOT/extensions/win11-shell/stylesheet.css" "$EXTENSION_DIR/"
echo "Extension installed to $EXTENSION_DIR"

echo "[4/4] Enabling extension..."
# Try to enable extension (may fail if gnome-extensions not available)
if command -v gnome-extensions &> /dev/null; then
    gnome-extensions enable win11-shell@linux-desktop 2>/dev/null || true
    echo "Extension enabled"
else
    echo "Run 'gnome-extensions enable win11-shell@linux-desktop' to enable"
fi

echo ""
echo "=== Theme Installation Complete ==="
echo ""
echo "To apply:"
echo ""
echo "1. GTK4 apps will use the theme automatically"
echo ""
echo "2. For GNOME Shell theme, use gnome-tweaks:"
echo "   sudo apt install gnome-tweaks"
echo "   gnome-tweaks → Appearance → Shell → Fluent"
echo ""
echo "3. For the Win11 extension:"
echo "   gnome-extensions enable win11-shell@linux-desktop"
echo "   Or use Extensions app"
echo ""
echo "4. Log out and back in to see all changes"
