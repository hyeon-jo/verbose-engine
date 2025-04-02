#!/bin/bash

# Exit on error
set -e

echo "🚀 Building and running control_app..."

# Clean previous build
echo "🧹 Cleaning previous build..."
rm -rf build

# Create build directory
echo "📁 Creating build directory..."
mkdir -p build
cd build

# Configure with CMake
echo "⚙️ Configuring with CMake..."
cmake ..

# Build
echo "🔨 Building..."
make -j$(nproc)

# Run the application
echo "▶️ Running control_app..."
./control_app

# Return to original directory
cd .. 