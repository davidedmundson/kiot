#!/bin/bash
# Generate icons from svg

set -e

# Accept APP_ID as the first argument, with a safe fallback
APP_ID="${1:-org.davidedmundson.kiot}"
SVG_SOURCE="${APP_ID}.svg"
OUTPUT_DIR="generated-icons"

if [ ! -f "./$SVG_SOURCE" ]; then
    echo "Error: icon template $SVG_SOURCE does not exist, aborting icon generator"
    exit 1
fi

# Create output directory
mkdir -p "$OUTPUT_DIR"

# Check for required tools
if command -v magick >/dev/null 2>&1; then
    echo "Using ImageMagick for icon generation..."
    TOOL="imagemagick"
else
    echo "Error: Neither Inkscape nor ImageMagick found."
    echo "Please install one of them:"
    echo "  sudo apt install inkscape"
    echo "  sudo apt install imagemagick"
    echo "  sudo pacman -S inkscape"
    echo "  sudo pacman -S imagemagick"
    exit 1
fi

sizes=(16 22 24 32 48 64 128)

for size in "${sizes[@]}"; do
    echo "Generating ${size}x${size} icon..."
    magick "$SVG_SOURCE" -background none -resize "${size}x${size}" "$OUTPUT_DIR/${size}x${size}.png"
    
    size2x=$((size * 2))
    echo "Generating ${size2x}x${size2x} (@2x) icon..."
    magick "$SVG_SOURCE" -background none -resize "${size2x}x${size2x}" "$OUTPUT_DIR/${size}x${size}@2.png"
done

echo "Generating favicon.ico..."
magick "$SVG_SOURCE" -background none -resize 64x64 "$OUTPUT_DIR/favicon.ico"

echo "Copying scalable icon for AppStream..."
cp "$SVG_SOURCE" "$OUTPUT_DIR/scalable.svg"

echo "Creating icon theme structure..."
mkdir -p "$OUTPUT_DIR/hicolor"

for size in "${sizes[@]}"; do
    mkdir -p "$OUTPUT_DIR/hicolor/${size}x${size}/apps"
    cp "$OUTPUT_DIR/${size}x${size}.png" "$OUTPUT_DIR/hicolor/${size}x${size}/apps/${APP_ID}.png"
    
    # @2x for HiDPI
    mkdir -p "$OUTPUT_DIR/hicolor/${size}x${size}@2/apps"
    cp "$OUTPUT_DIR/${size}x${size}@2.png" "$OUTPUT_DIR/hicolor/${size}x${size}@2/apps/${APP_ID}.png"
done

# Scalable icon
mkdir -p "$OUTPUT_DIR/hicolor/scalable/apps"
cp "$SVG_SOURCE" "$OUTPUT_DIR/hicolor/scalable/apps/${APP_ID}.svg"

echo -e "\nIcons generated in: $OUTPUT_DIR/"
echo -e "\nFiles created:"
find "$OUTPUT_DIR" -type f | sort