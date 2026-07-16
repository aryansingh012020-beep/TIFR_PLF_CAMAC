#!/bin/bash
# LAMPS + EPICS Automated Setup Script
# This script installs all necessary apt dependencies and compiles EPICS base.

set -e

echo "=========================================="
echo " LAMPS & EPICS Environment Setup Script"
echo "=========================================="

echo "[1/3] Updating package lists..."
sudo apt-get update

echo "[2/3] Installing required system dependencies..."
# Installs all packages previously listed in DEPENDENCIES, plus git and perl for EPICS
sudo apt-get install -y \
    build-essential \
    gcc \
    g++ \
    make \
    pkg-config \
    gfortran \
    libgfortran5 \
    libgtk2.0-dev \
    libglib2.0-dev \
    libcanberra-gtk-module \
    libcanberra-gtk3-module \
    linux-headers-$(uname -r) \
    libusb-dev \
    usbutils \
    git \
    wget \
    perl \
    libreadline-dev

echo "[3/3] Setting up EPICS Base..."
EPICS_ROOT="$HOME/epics"
EPICS_BASE="$EPICS_ROOT/base"

if [ ! -d "$EPICS_BASE" ]; then
    echo "Cloning EPICS Base..."
    mkdir -p "$EPICS_ROOT"
    cd "$EPICS_ROOT"
    # Clone a stable, recent release of EPICS base
    git clone --branch R7.0.7 https://github.com/epics-base/epics-base.git base
else
    echo "EPICS Base directory already exists at $EPICS_BASE."
fi

echo "Compiling EPICS Base (this may take a few minutes)..."
cd "$EPICS_BASE"
make -j$(nproc)

echo "========================================================"
echo "                     SETUP COMPLETE!                    "
echo "========================================================"
echo "IMPORTANT: You need to add EPICS to your environment."
echo "Please run the following command to add it to your ~/.bashrc:"
echo ""
echo "echo 'export PATH=\$PATH:$EPICS_BASE/bin/linux-x86_64/' >> ~/.bashrc"
echo "echo 'export EPICS_HOST_ARCH=linux-x86_64' >> ~/.bashrc"
echo "source ~/.bashrc"
echo ""
echo "After doing that, you can compile LAMPS by running:"
echo "cd src/"
echo "make EPICS_BASE=$EPICS_BASE"
echo "========================================================"


#use ./setup_environment.sh to install usingthis requirement file (use sudo before this )
