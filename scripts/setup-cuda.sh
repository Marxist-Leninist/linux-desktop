#!/bin/bash
# setup-cuda.sh - Install NVIDIA drivers and CUDA toolkit
set -e

echo "=== Setting up NVIDIA + CUDA ==="

if [ "$EUID" -ne 0 ]; then
    SUDO="sudo"
else
    SUDO=""
fi

# Check for NVIDIA GPU
if ! lspci | grep -i nvidia > /dev/null; then
    echo "WARNING: No NVIDIA GPU detected. Continue anyway? (y/n)"
    read -r response
    if [ "$response" != "y" ]; then
        exit 1
    fi
fi

echo "[1/4] Installing NVIDIA driver..."
$SUDO apt install -y nvidia-driver-550

echo "[2/4] Adding CUDA repository..."
if [ ! -f /usr/share/keyrings/cuda-keyring.gpg ]; then
    wget -q https://developer.download.nvidia.com/compute/cuda/repos/ubuntu2404/x86_64/cuda-keyring_1.1-1_all.deb
    $SUDO dpkg -i cuda-keyring_1.1-1_all.deb
    rm cuda-keyring_1.1-1_all.deb
    $SUDO apt update
fi

echo "[3/4] Installing CUDA toolkit..."
$SUDO apt install -y cuda-toolkit-12-4

echo "[4/4] Installing cuDNN..."
$SUDO apt install -y libcudnn8 libcudnn8-dev || echo "cuDNN not available in repo, manual install may be needed"

# Add to PATH if not already there
if ! grep -q "cuda/bin" ~/.bashrc; then
    echo "" >> ~/.bashrc
    echo "# CUDA" >> ~/.bashrc
    echo 'export PATH=/usr/local/cuda/bin:$PATH' >> ~/.bashrc
    echo 'export LD_LIBRARY_PATH=/usr/local/cuda/lib64:$LD_LIBRARY_PATH' >> ~/.bashrc
    echo "Added CUDA to PATH in ~/.bashrc"
fi

echo ""
echo "=== CUDA setup complete ==="
echo ""
echo "IMPORTANT: Reboot required for NVIDIA driver!"
echo ""
echo "After reboot, verify with:"
echo "  nvidia-smi"
echo "  nvcc --version"
echo ""
echo "Next: Run ./scripts/setup-ml.sh (for PyTorch + transformers)"
