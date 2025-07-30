#!/usr/bin/env bash

echo "Cleaning the build directory..."
rm -rf build
mkdir build
cd build
cp ../src ./ -r
cp ../configure.ac .
cp ../Makefile.am .

out="$(pwd)/output"
export out=$out
OUTPUT="$out/lib/rofi"
mkdir -p $OUTPUT

echo "Running autoreconf..."
autoreconf -i

echo "Creating the build directory..."
mkdir build
cd build

echo "Running configure..."
../configure

echo "Building the project..."
bear -- make
cp compile_commands.json ../

echo "Installing to $(echo $out)..."
make install
make clean

if [[ "$1" == "run" ]]; then
  echo "Running the program..."
  export ROFI_PLUGIN_PATH=$OUTPUT
  rofi -show combi -modi combi -combi-modi drun,files #\
      # -files-base-dir "$HOME" \
      # -files-ignore-path "$HOME/.config/rofi/files_ignore.txt"
fi

