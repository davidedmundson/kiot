#!/bin/bash
# Generate icons from svg

set -e

SVG_SOURCE="org.davidedmundson.kiot.svg"
OUTPUT_DIR="generated-icons"

# Create output directory
mkdir -p "$OUTPUT_DIR"

# Check for required tools
if command -v inkscape >/dev/null 2>&1; then
    echo "Using Inkscape for icon generation..."
    TOOL="inkscape"
elif command -v convert >/dev/null 2>&1; then
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
#Whats the max size on icons?
sizes=(16 22 24 32 48 64 128 )

for size in "${sizes[@]}"; do
    echo "Generating ${size}x${size} icon..."
    
    if [ "$TOOL" = "inkscape" ]; then
        inkscape -w "$size" -h "$size" "$SVG_SOURCE" -o "$OUTPUT_DIR/${size}x${size}.png"
    else
        convert -background none -resize "${size}x${size}" "$SVG_SOURCE" "$OUTPUT_DIR/${size}x${size}.png"
    fi
    
    size2x=$((size * 2))
    echo "Generating ${size2x}x${size2x} (@2x) icon..."
    
    if [ "$TOOL" = "inkscape" ]; then
        inkscape -w "$size2x" -h "$size2x" "$SVG_SOURCE" -o "$OUTPUT_DIR/${size}x${size}@2.png"
    else
        convert -background none -resize "${size2x}x${size2x}" "$SVG_SOURCE" "$OUTPUT_DIR/${size}x${size}@2.png"
    fi
done

echo "Generating favicon.ico..."
if [ "$TOOL" = "inkscape" ]; then
    inkscape -w 64 -h 64 "$SVG_SOURCE" -o "$OUTPUT_DIR/favicon-64.png"
    convert "$OUTPUT_DIR/favicon-64.png" "$OUTPUT_DIR/favicon.ico"
else
    convert -background none -resize "64x64" "$SVG_SOURCE" "$OUTPUT_DIR/favicon.ico"
fi

echo "Copying scalable icon for AppStream..."
cp "$SVG_SOURCE" "$OUTPUT_DIR/scalable.svg"

echo "Creating icon theme structure..."
mkdir -p "$OUTPUT_DIR/hicolor"

for size in "${sizes[@]}"; do
    mkdir -p "$OUTPUT_DIR/hicolor/${size}x${size}/apps"
    cp "$OUTPUT_DIR/${size}x${size}.png" "$OUTPUT_DIR/hicolor/${size}x${size}/apps/org.davidedmundson.kiot.png"
    
    # @2x for HiDPI
    mkdir -p "$OUTPUT_DIR/hicolor/${size}x${size}@2/apps"
    cp "$OUTPUT_DIR/${size}x${size}@2.png" "$OUTPUT_DIR/hicolor/${size}x${size}@2/apps/org.davidedmundson.kiot.png"
done

# Scalable icon
mkdir -p "$OUTPUT_DIR/hicolor/scalable/apps"
cp "$SVG_SOURCE" "$OUTPUT_DIR/hicolor/scalable/apps/org.davidedmundson.kiot.svg"

echo "\nIcons generated in: $OUTPUT_DIR/"
echo "\nFiles created:"
find "$OUTPUT_DIR" -type f | sort
