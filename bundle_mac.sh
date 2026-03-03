#!/bin/bash
set -e

APP="Bogoshop.app"
BINARY="viewer_mac"
SDL2_DYLIB="/opt/homebrew/opt/sdl2/lib/libSDL2-2.0.0.dylib"
ICON_SRC="assets/icon.png"

echo "==> Kompilieren..."
gcc -DSDL_MAIN_HANDLED src/viewer.c src/effects.c src/functions.c src/license.c vendor/tinyfiledialogs.c \
    -o "$BINARY" -O2 \
    -I/opt/homebrew/include -I include/ -I . -L/opt/homebrew/lib -lSDL2

echo "==> Bundle-Struktur anlegen..."
rm -rf "$APP"
mkdir -p "$APP/Contents/MacOS"
mkdir -p "$APP/Contents/Frameworks"
mkdir -p "$APP/Contents/Resources"

echo "==> Binärdatei kopieren..."
cp "$BINARY" "$APP/Contents/MacOS/Bogoshop"

echo "==> Docs kopieren..."
cp -r docs "$APP/Contents/MacOS/docs"

echo "==> SDL2 einbetten..."
cp "$SDL2_DYLIB" "$APP/Contents/Frameworks/libSDL2-2.0.0.dylib"
install_name_tool \
    -change "$SDL2_DYLIB" \
    "@executable_path/../Frameworks/libSDL2-2.0.0.dylib" \
    "$APP/Contents/MacOS/Bogoshop"

echo "==> Icon konvertieren (.icns)..."
ICONSET="$(mktemp -d)/bogoshop.iconset"
mkdir -p "$ICONSET"
sips -z 16   16   "$ICON_SRC" --out "$ICONSET/icon_16x16.png"      > /dev/null
sips -z 32   32   "$ICON_SRC" --out "$ICONSET/icon_16x16@2x.png"   > /dev/null
sips -z 32   32   "$ICON_SRC" --out "$ICONSET/icon_32x32.png"      > /dev/null
sips -z 64   64   "$ICON_SRC" --out "$ICONSET/icon_32x32@2x.png"   > /dev/null
sips -z 128  128  "$ICON_SRC" --out "$ICONSET/icon_128x128.png"    > /dev/null
sips -z 256  256  "$ICON_SRC" --out "$ICONSET/icon_128x128@2x.png" > /dev/null
sips -z 256  256  "$ICON_SRC" --out "$ICONSET/icon_256x256.png"    > /dev/null
sips -z 512  512  "$ICON_SRC" --out "$ICONSET/icon_256x256@2x.png" > /dev/null
sips -z 512  512  "$ICON_SRC" --out "$ICONSET/icon_512x512.png"    > /dev/null
iconutil -c icns "$ICONSET" -o "$APP/Contents/Resources/icon.icns"

echo "==> Info.plist schreiben..."
cat > "$APP/Contents/Info.plist" << 'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN"
  "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleName</key>             <string>Bogoshop</string>
    <key>CFBundleDisplayName</key>      <string>Bogoshop</string>
    <key>CFBundleIdentifier</key>       <string>com.bogoshop.app</string>
    <key>CFBundleVersion</key>          <string>0.2.0</string>
    <key>CFBundleShortVersionString</key><string>0.2.0</string>
    <key>CFBundleExecutable</key>       <string>Bogoshop</string>
    <key>CFBundleIconFile</key>         <string>icon</string>
    <key>CFBundlePackageType</key>      <string>APPL</string>
    <key>NSHighResolutionCapable</key>  <true/>
    <key>LSMinimumSystemVersion</key>   <string>10.13</string>
</dict>
</plist>
EOF

echo "==> Ad-hoc signieren..."
codesign --deep --force --sign - "$APP"

echo ""
echo "Fertig: $APP"
echo "Starten mit: open $APP"
