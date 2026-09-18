#!/bin/bash

set -e # If any command fails exit

echo "================================"
echo " ScriptExecutor Setup"
echo "================================"

# check for perms

if ! sudo -v; then
    echo "Failed to acquire sudo privileges."
    exit 1
fi

# update & upgrade

echo "[1/3] Updating System..."
sudo apt update
sudo apt upgrade -y

echo "[2/3] Installing Dependencies..."

sudo apt install -y \
    cmake \
    build-essential \
    ninja-build \
    qt6-base-dev \
    qt6-base-dev-tools \
    qt6-declarative-dev \
    libxkbcommon-dev \
    qml6-module-qtquick \
    qml6-module-qtquick-window \
    qml6-module-qtquick-controls \
    qml6-module-qtquick-layouts \
    qml6-module-qtquick-templates \
    qml6-module-qtqml \
    qml6-module-qtqml-workerscript

echo "[3/3] Verifying installation..."

cmake --version
qmake6 --version

echo
echo "Setup completed successfully!"