# linux-desktop Makefile
# Convenience targets for building the GNOME stack + kernel GUI

PREFIX ?= /usr/local
JOBS ?= $(shell nproc)

.PHONY: help deps cuda ml build gtk mutter shell kernel clean

help:
	@echo "linux-desktop - GNOME stack + kernel GUI integration"
	@echo ""
	@echo "Setup targets:"
	@echo "  make deps     - Install build dependencies"
	@echo "  make cuda     - Install NVIDIA drivers + CUDA"
	@echo "  make ml       - Set up Python ML environment"
	@echo ""
	@echo "Build targets:"
	@echo "  make build    - Build everything (GTK → Mutter → Shell)"
	@echo "  make gtk      - Build GTK4 only"
	@echo "  make mutter   - Build Mutter only"
	@echo "  make shell    - Build GNOME Shell only"
	@echo ""
	@echo "Kernel targets:"
	@echo "  make kernel KERNEL_SRC=/path/to/linux  - Integrate kernel GUI"
	@echo ""
	@echo "Other:"
	@echo "  make clean    - Remove build directories"
	@echo ""
	@echo "Environment variables:"
	@echo "  PREFIX=/usr/local  - Installation prefix"
	@echo "  JOBS=$(JOBS)            - Parallel jobs"

deps:
	./scripts/install-deps.sh

cuda:
	./scripts/setup-cuda.sh

ml:
	./scripts/setup-ml.sh

build:
	PREFIX=$(PREFIX) JOBS=$(JOBS) ./scripts/build.sh all

gtk:
	PREFIX=$(PREFIX) JOBS=$(JOBS) ./scripts/build.sh gtk

mutter:
	PREFIX=$(PREFIX) JOBS=$(JOBS) ./scripts/build.sh mutter

shell:
	PREFIX=$(PREFIX) JOBS=$(JOBS) ./scripts/build.sh shell

kernel:
ifndef KERNEL_SRC
	@echo "Usage: make kernel KERNEL_SRC=/path/to/linux-source"
	@echo ""
	@echo "Or let it auto-detect:"
	./scripts/install-kernel-gui.sh
else
	./scripts/install-kernel-gui.sh $(KERNEL_SRC)
endif

clean:
	rm -rf gtk/build mutter/build gnome-shell/build
	@echo "Build directories removed"

# Quick start
all: deps build
	@echo ""
	@echo "=== All done! ==="
	@echo "Log out and select GNOME at login to use."
