#!/bin/bash

# Exit immediately if any command fails
set -e

echo "=========================================="
echo "    Installing Voxel Version Control      "
echo "=========================================="

# 1. Download the repository into a specific temporary folder
echo "➔ Downloading Voxel source code..."
git clone --depth 1 https://github.com/Naman1177/Voxel.git voxel_temp_source > /dev/null 2>&1

# Move into the downloaded folder
cd voxel_temp_source

# 2. Compile the engine using your Makefile
echo "➔ Compiling the engine (this may take a few seconds)..."
make > /dev/null 2>&1

# 3. Install globally (This triggers the 'install' rule in your Makefile!)
echo "➔ Installing to /usr/local/bin (Administrator privileges required)..."
sudo make install > /dev/null 2>&1

# 4. Step back out of the folder
cd ..

# 5. The Nuke: Completely delete the downloaded codebase
echo "➔ Wiping source code to keep your system clean..."
rm -rf voxel_temp_source

echo "=========================================="
echo "  Success! Source code destroyed.         "
echo "  Voxel is permanently installed globally."
echo "=========================================="