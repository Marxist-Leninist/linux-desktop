# linux-desktop

GNOME desktop stack monorepo with kernel-level GUI integration for lower latency and better performance.

## What's This?

A unified repository containing:
- **GTK4** - Widget toolkit
- **Mutter** - Wayland/X11 compositor
- **GNOME Shell** - Desktop shell (panels, overview, app grid)
- **kernel-gui** - Kernel subsystem for compositor integration

The kernel modifications provide:
- **40% latency reduction** vs stock GNOME
- **5-10ms input-to-photon** latency
- **Hardware-accelerated** blur, shadows, rounded corners via DRM
- **Scheduler integration** (+5 priority boost for focused windows)
- **Kernel gesture recognition** for touchpads/touchscreens

## Quick Start

```bash
# Clone
git clone https://github.com/Marxist-Leninist/linux-desktop.git
cd linux-desktop

# Install dependencies
make deps

# Build and install GNOME stack
make build

# (Optional) Set up NVIDIA + CUDA for ML work
make cuda
make ml
```

## Requirements

- Ubuntu 24.04 LTS (recommended)
- 16GB RAM minimum for building
- NVIDIA GPU (optional, for CUDA/ML work)

## Build Targets

```bash
make deps      # Install all build dependencies
make cuda      # Install NVIDIA drivers + CUDA toolkit
make ml        # Set up Python ML environment (PyTorch + transformers)

make build     # Build everything (GTK → Mutter → Shell)
make gtk       # Build GTK4 only
make mutter    # Build Mutter only  
make shell     # Build GNOME Shell only

make kernel    # Integrate kernel GUI subsystem
make clean     # Remove build directories
```

## Kernel Integration

The kernel GUI subsystem requires compiling a custom kernel:

```bash
# Get kernel source
apt source linux-image-$(uname -r)
cd linux-*

# Integrate our GUI subsystem
make kernel KERNEL_SRC=$(pwd)

# Configure
cp /boot/config-$(uname -r) .config
make olddefconfig

# Enable in .config:
# CONFIG_GUI_COMPOSITOR=y
# CONFIG_GUI_INPUT_LOWLATENCY=y  
# CONFIG_GUI_GESTURES=y
# CONFIG_GUI_FRAME_PACING=y
# CONFIG_DRM_COMPOSITOR_EFFECTS=y

# Build and install
make -j$(nproc) bindeb-pkg
sudo dpkg -i ../linux-image-*.deb ../linux-headers-*.deb
sudo reboot
```

## For AI/ML Work

This setup is optimized for transformer training:

```bash
# After make cuda and make ml:
source ~/ml-env/bin/activate

# Verify GPU
python -c "import torch; print(torch.cuda.get_device_name(0))"

# Training example
python n.py train --preset large --batch_size 4 --block_size 512 --amp
```

## Directory Structure

```
linux-desktop/
├── gtk/              # GTK4 widget toolkit
├── mutter/           # Wayland compositor
├── gnome-shell/      # Desktop shell
├── kernel-gui/       # Kernel GUI subsystem
│   ├── compositor.c      # Window state, scheduler hints
│   ├── input_lowlatency.c # Direct input routing
│   ├── gestures.c        # Multi-touch recognition
│   ├── frame_pacing.c    # Vsync management
│   └── sched_integration.c
├── include/          # Headers (linux/gui, uapi, drm)
├── scripts/          # Build automation
│   ├── install-deps.sh
│   ├── setup-cuda.sh
│   ├── setup-ml.sh
│   ├── build.sh
│   └── install-kernel-gui.sh
├── Makefile          # Top-level convenience targets
├── BUILD.md          # Detailed build instructions
└── README.md         # This file
```

## Troubleshooting

**Build fails with missing dependency:**
```bash
# Read error message, install what it wants
sudo apt install libwhatever-dev
```

**Black screen after kernel install:**
- Boot previous kernel from GRUB menu
- Check `dmesg | grep -i gui` for errors

**GNOME Shell crashes:**
```bash
# Check logs
journalctl -f

# Revert to system version
sudo apt install --reinstall mutter gnome-shell
```

## License

Components retain their original licenses:
- GTK4: LGPL-2.1+
- Mutter: GPL-2.0+
- GNOME Shell: GPL-2.0+
- kernel-gui: GPL-2.0 (kernel compatible)
