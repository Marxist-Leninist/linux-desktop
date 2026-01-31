#!/bin/bash
# setup-ml.sh - Set up Python ML environment with PyTorch + transformers
set -e

echo "=== Setting up ML environment ==="

ML_ENV="${ML_ENV:-$HOME/ml-env}"

echo "[1/5] Creating virtual environment at $ML_ENV..."
python3 -m venv "$ML_ENV"
source "$ML_ENV/bin/activate"

echo "[2/5] Upgrading pip..."
pip install --upgrade pip wheel setuptools

echo "[3/5] Installing PyTorch with CUDA..."
pip install torch torchvision torchaudio --index-url https://download.pytorch.org/whl/cu124

echo "[4/5] Installing transformers ecosystem..."
pip install \
    transformers \
    datasets \
    tokenizers \
    accelerate \
    safetensors \
    sentencepiece

echo "[5/5] Installing training utilities..."
pip install \
    bitsandbytes \
    wandb \
    tensorboard \
    tqdm \
    einops \
    ninja

# Try flash-attn (may fail on some systems)
echo "Attempting to install flash-attn (optional, may fail)..."
pip install flash-attn --no-build-isolation 2>/dev/null || echo "flash-attn install failed (not critical)"

# Verify CUDA
echo ""
echo "=== Verifying installation ==="
python -c "
import torch
print(f'PyTorch version: {torch.__version__}')
print(f'CUDA available: {torch.cuda.is_available()}')
if torch.cuda.is_available():
    print(f'CUDA version: {torch.version.cuda}')
    print(f'GPU: {torch.cuda.get_device_name(0)}')
    print(f'GPU memory: {torch.cuda.get_device_properties(0).total_memory / 1e9:.1f} GB')
"

echo ""
echo "=== ML environment ready ==="
echo ""
echo "Activate with: source $ML_ENV/bin/activate"
echo ""
echo "Add to ~/.bashrc for convenience:"
echo "  alias ml='source $ML_ENV/bin/activate'"
