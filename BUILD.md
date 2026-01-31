# Building linux-desktop Monorepo

Complete build guide for the GNOME stack + kernel GUI integration on Ubuntu.

## Prerequisites

Ubuntu 24.04 LTS recommended (best NVIDIA/CUDA support).

```bash
# System update
sudo apt update && sudo apt upgrade -y

# Build essentials
sudo apt install -y build-essential git meson ninja-build cmake pkg-config

# Kernel build deps
sudo apt install -y libncurses-dev flex bison libssl-dev libelf-dev bc

# GNOME/Wayland deps
sudo apt install -y \
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
    sassc gettext libxml2-utils xsltproc

# AI/ML stack
sudo apt install -y python3-pip python3-venv
```

## NVIDIA + CUDA Setup (for transformer training)

```bash
# NVIDIA driver (latest)
sudo apt install -y nvidia-driver-550

# CUDA toolkit
wget https://developer.download.nvidia.com/compute/cuda/repos/ubuntu2404/x86_64/cuda-keyring_1.1-1_all.deb
sudo dpkg -i cuda-keyring_1.1-1_all.deb
sudo apt update
sudo apt install -y cuda-toolkit-12-4

# cuDNN
sudo apt install -y libcudnn8 libcudnn8-dev

# Add to PATH (~/.bashrc)
echo 'export PATH=/usr/local/cuda/bin:$PATH' >> ~/.bashrc
echo 'export LD_LIBRARY_PATH=/usr/local/cuda/lib64:$LD_LIBRARY_PATH' >> ~/.bashrc
source ~/.bashrc

# Verify
nvidia-smi
nvcc --version
```

## PyTorch + Transformers

```bash
# Create venv for ML work
python3 -m venv ~/ml-env
source ~/ml-env/bin/activate

# PyTorch with CUDA
pip install torch torchvision torchaudio --index-url https://download.pytorch.org/whl/cu124

# Transformers ecosystem
pip install transformers datasets tokenizers accelerate
pip install bitsandbytes  # quantization
pip install flash-attn --no-build-isolation  # fast attention
pip install wandb  # experiment tracking

# Verify CUDA works
python -c "import torch; print(f'CUDA: {torch.cuda.is_available()}, Device: {torch.cuda.get_device_name(0)}')"
```

## Build Order

### 1. Kernel with GUI Subsystem

```bash
cd ~/linux-desktop

# Get kernel source (match your running kernel)
apt source linux-image-$(uname -r)
cd linux-*

# Copy our GUI subsystem
cp -r ~/linux-desktop/kernel-gui kernel/gui/
cp -r ~/linux-desktop/include/linux/gui include/linux/
cp -r ~/linux-desktop/include/uapi/linux/gui include/uapi/linux/
cp -r ~/linux-desktop/include/drm/* include/drm/

# Add to kernel Makefile
echo 'obj-y += gui/' >> kernel/Makefile

# Configure
cp /boot/config-$(uname -r) .config
make olddefconfig

# Enable GUI subsystem
echo 'CONFIG_GUI_COMPOSITOR=y' >> .config
echo 'CONFIG_GUI_INPUT_LOWLATENCY=y' >> .config
echo 'CONFIG_GUI_GESTURES=y' >> .config
echo 'CONFIG_GUI_FRAME_PACING=y' >> .config
echo 'CONFIG_DRM_COMPOSITOR_EFFECTS=y' >> .config

# Build (use all cores)
make -j$(nproc) bindeb-pkg

# Install
sudo dpkg -i ../linux-image-*.deb ../linux-headers-*.deb
sudo reboot
```

### 2. GTK4

```bash
cd ~/linux-desktop/gtk

meson setup build \
    --prefix=/usr/local \
    -Dbuild-examples=false \
    -Dbuild-tests=false \
    -Dmedia-gstreamer=enabled \
    -Dvulkan=enabled

ninja -C build
sudo ninja -C build install
sudo ldconfig
```

### 3. Mutter (Compositor)

```bash
cd ~/linux-desktop/mutter

meson setup build \
    --prefix=/usr/local \
    -Degl_device=true \
    -Dwayland=true \
    -Dnative_backend=true \
    -Dremote_desktop=true \
    -Dprofiler=true \
    -Dtests=false

ninja -C build
sudo ninja -C build install
sudo ldconfig
```

### 4. GNOME Shell

```bash
cd ~/linux-desktop/gnome-shell

meson setup build \
    --prefix=/usr/local \
    -Dextensions_app=true \
    -Dman=false \
    -Dtests=false

ninja -C build
sudo ninja -C build install
```

### 5. Activate

```bash
# Log out, select "GNOME" at login screen
# Or restart GNOME Shell: Alt+F2, type "r", Enter (X11 only)
```

## Verify Kernel GUI Subsystem

```bash
# Check sysfs interface
ls /sys/kernel/gui/

# Should see:
# compositor/  input/  gestures/  frame_pacing/

# Check loaded
dmesg | grep -i "gui compositor"
```

## Directory Structure

```
linux-desktop/
├── mutter/           # Wayland compositor (renders windows)
├── gnome-shell/      # Desktop shell (panels, overview, app grid)
├── gtk/              # Widget toolkit (buttons, windows, etc)
├── kernel-gui/       # Kernel GUI subsystem source
│   ├── compositor.c      # Window state tracking, scheduler hints
│   ├── input_lowlatency.c # Direct input routing (<10ms)
│   ├── gestures.c        # Multi-touch recognition
│   ├── frame_pacing.c    # Vsync management
│   └── sched_integration.c # +5 priority boost for focused windows
├── include/          # Headers
│   ├── linux/gui/        # Kernel API
│   ├── uapi/linux/gui/   # Userspace API
│   └── drm/              # DRM compositor effects
└── BUILD.md          # This file
```

## AI/ML Workflow Tips

```bash
# Activate ML environment
source ~/ml-env/bin/activate

# Monitor GPU during training
watch -n1 nvidia-smi

# tmux for persistent sessions
sudo apt install tmux
tmux new -s training
# Ctrl+B, D to detach
# tmux attach -t training

# Typical training command (AGILLM style)
python n.py train \
    --preset large \
    --batch_size 4 \
    --block_size 512 \
    --amp \
    --save_every_sec 43200
```

## Troubleshooting

**Black screen after kernel install:**
- Boot previous kernel from GRUB menu
- Check `dmesg` for errors

**GNOME Shell crashes:**
- Check `journalctl -f` in another TTY (Ctrl+Alt+F2)
- Revert to system mutter: `sudo apt install --reinstall mutter`

**CUDA not found:**
- Verify `nvidia-smi` works
- Check LD_LIBRARY_PATH includes `/usr/local/cuda/lib64`

**Build failures:**
- Usually missing deps, read error and `apt install` what it wants
- GTK4 needs meson >= 0.63, ninja >= 1.10

## Performance Targets

With kernel GUI subsystem enabled:
- 40% latency reduction vs stock GNOME
- 5-10ms input-to-photon latency
- 50-70% less CPU for compositor effects (hardware accelerated)
- <1ms frame jitter with vsync management
- ~4KB memory overhead per window
