#!/usr/bin/env bash
set -euo pipefail

###############################################################################
# setup.sh — Install project dependencies
#
# This script is run once to set up the environment.
###############################################################################

# System dependencies
echo "[1/6] Installing system dependencies..."
sudo apt-get update -qq
sudo apt-get install -y --no-install-recommends \
  build-essential \
  cmake \
  libssl-dev \
  libsecp256k1-dev \
  git \
  jq \
  curl

# Node.js
echo "[2/6] Installing Node.js..."
if ! command -v node &>/dev/null; then
  curl -fsSL https://deb.nodesource.com/setup_20.x | sudo -E bash -
  sudo apt-get install -y nodejs
else
  echo "  Node.js already installed: $(node --version)"
fi

# Decompress block fixtures
echo "[3/6] Decompressing block fixtures..."
for gz in fixtures/blocks/*.dat.gz; do
  dat="${gz%.gz}"
  if [[ ! -f "$dat" ]]; then
    echo "  Decompressing $(basename "$gz")..."
    gunzip -k "$gz"
  else
    echo "  $(basename "$dat") already exists, skipping"
  fi
done

# Build C++ tool
echo "[4/6] Building tx_tool..."
cd src
cmake \
  -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build -j"$(nproc)"
cd ..

# Verify binary exists
echo "[5/6] Verifying build and copying to webapp..."
BIN="src/build/tx_tool"
if [[ ! -f "$BIN" ]]; then
  echo "ERROR: Binary not found at $BIN" >&2
  exit 1
fi
if [[ ! -x "$BIN" ]]; then
  echo "ERROR: Binary not executable: $BIN" >&2
  exit 1
fi
echo "  Binary: $BIN"
echo "  Size:   $(du -h "$BIN" | cut -f1)"

# Copy binary to webapp and rename to analyzer
cp "$BIN" webapp/analyzer
chmod +x webapp/analyzer
echo "  Copied to: webapp/analyzer"

# Install Node.js dependencies
echo "[6/6] Installing webapp dependencies..."
cd webapp
npm install
cd ..

# Create output directory
mkdir -p out

echo ""
echo "Setup complete"