#!/bin/bash
# build.sh - Build GTK4, Mutter, and GNOME Shell
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(dirname "$SCRIPT_DIR")"
PREFIX="${PREFIX:-/usr/local}"
JOBS="${JOBS:-$(nproc)}"

if [ "$EUID" -ne 0 ]; then
    SUDO="sudo"
else
    SUDO=""
fi

cd "$REPO_ROOT"

build_gtk() {
    echo ""
    echo "=========================================="
    echo "[1/3] Building GTK4"
    echo "=========================================="
    cd "$REPO_ROOT/gtk"
    
    # Clean previous build if exists
    [ -d build ] && rm -rf build
    
    meson setup build \
        --prefix="$PREFIX" \
        -Dbuild-examples=false \
        -Dbuild-tests=false \
        -Dbuild-testsuite=false \
        -Dmedia-gstreamer=enabled \
        -Dvulkan=enabled \
        -Dx11-backend=true \
        -Dwayland-backend=true
    
    ninja -C build -j"$JOBS"
    $SUDO ninja -C build install
    $SUDO ldconfig
    
    echo "GTK4 installed to $PREFIX"
}

build_mutter() {
    echo ""
    echo "=========================================="
    echo "[2/3] Building Mutter (compositor)"
    echo "=========================================="
    cd "$REPO_ROOT/mutter"
    
    [ -d build ] && rm -rf build
    
    meson setup build \
        --prefix="$PREFIX" \
        -Degl_device=true \
        -Dwayland=true \
        -Dnative_backend=true \
        -Dremote_desktop=true \
        -Dprofiler=true \
        -Dtests=false \
        -Dcogl_tests=false \
        -Dclutter_tests=false
    
    ninja -C build -j"$JOBS"
    $SUDO ninja -C build install
    $SUDO ldconfig
    
    echo "Mutter installed to $PREFIX"
}

build_gnome_shell() {
    echo ""
    echo "=========================================="
    echo "[3/3] Building GNOME Shell"
    echo "=========================================="
    cd "$REPO_ROOT/gnome-shell"
    
    [ -d build ] && rm -rf build
    
    meson setup build \
        --prefix="$PREFIX" \
        -Dextensions_app=true \
        -Dextensions_tool=true \
        -Dman=false \
        -Dtests=false
    
    ninja -C build -j"$JOBS"
    $SUDO ninja -C build install
    
    echo "GNOME Shell installed to $PREFIX"
}

# Parse arguments
COMPONENT="${1:-all}"

case "$COMPONENT" in
    gtk)
        build_gtk
        ;;
    mutter)
        build_mutter
        ;;
    shell|gnome-shell)
        build_gnome_shell
        ;;
    all)
        build_gtk
        build_mutter
        build_gnome_shell
        ;;
    *)
        echo "Usage: $0 [gtk|mutter|shell|all]"
        echo ""
        echo "Components:"
        echo "  gtk     - Build GTK4 widget toolkit"
        echo "  mutter  - Build Mutter compositor"
        echo "  shell   - Build GNOME Shell"
        echo "  all     - Build everything (default)"
        echo ""
        echo "Environment variables:"
        echo "  PREFIX=/usr/local  - Installation prefix"
        echo "  JOBS=\$(nproc)      - Parallel build jobs"
        exit 1
        ;;
esac

echo ""
echo "=========================================="
echo "Build complete!"
echo "=========================================="
echo ""
echo "Log out and select 'GNOME' at login screen to use."
echo "Or restart GNOME Shell: Alt+F2, type 'r', Enter (X11 only)"
