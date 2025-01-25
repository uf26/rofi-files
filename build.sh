#!/bin/bash

# Define the build directory
BUILD_DIR="build"

# Check if build directory exists, and clean if necessary
echo "Cleaning the build directory..."
rm -rf "$BUILD_DIR"

# Regenerate configuration files (optional if you modified Makefile.am or others)
echo "Running autoreconf..."
autoreconf -i

# Create the build directory again
echo "Creating the build directory..."
mkdir "$BUILD_DIR"
cd "$BUILD_DIR"

# Run configure
echo "Running configure..."
../configure

# Build the project
echo "Building the project..."
bear -- make

echo "Build process complete!"

if [ $# -eq 1 ] && [ "$1" == "run" ]; then
    echo "Running the program..."
    export ROFI_PLUGIN_PATH=$(pwd)/.libs
    rofi -show combi -modi combi -combi-modi drun,files \
        -files-base-dir "$HOME" \
        -files-ignore-path "$HOME/.config/rofi/files_ignore.txt"
    # rofi -show combi -no-config -show-icons -modi combi -combi-modi "drun,files"

    # rofi -show files -modi files:.libs/files.so -no-config

fi
