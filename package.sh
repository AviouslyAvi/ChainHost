#!/bin/bash
# package.sh — Build a macOS DMG installer for ChainHost
# Usage: ./package.sh [--skip-build]
#
# Produces: ChainHost-<version>-macOS.dmg with:
#   - ChainHost.app (Standalone)
#   - ChainHost.vst3 (VST3 plugin)
#   - Drag-to-install symlinks

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"

# Extract version from CMakeLists.txt
VERSION=$(grep -m1 'project(ChainHost VERSION' "$SCRIPT_DIR/CMakeLists.txt" \
    | sed 's/.*VERSION \([0-9.]*\).*/\1/')
DMG_NAME="ChainHost-${VERSION}-macOS"
STAGING="$BUILD_DIR/_dmg_staging"

echo "=== ChainHost DMG Packager v${VERSION} ==="

# ── Build ─────────────────────────────────────────────────────────
if [[ "${1:-}" != "--skip-build" ]]; then
    echo ">> Building..."
    cd "$BUILD_DIR"
    cmake .. -DCMAKE_BUILD_TYPE=Release
    cmake --build . --config Release
    cd "$SCRIPT_DIR"
else
    echo ">> Skipping build (--skip-build)"
fi

APP_PATH="$BUILD_DIR/ChainHost_artefacts/Standalone/ChainHost.app"
VST3_PATH="$BUILD_DIR/ChainHost_artefacts/VST3/ChainHost.vst3"
VST3FX_PATH="$BUILD_DIR/ChainHostFX_artefacts/VST3/ChainHost FX.vst3"

if [[ ! -d "$APP_PATH" ]]; then
    echo "ERROR: Standalone app not found at $APP_PATH"
    exit 1
fi
if [[ ! -d "$VST3_PATH" ]]; then
    echo "ERROR: VST3 not found at $VST3_PATH"
    exit 1
fi
if [[ ! -d "$VST3FX_PATH" ]]; then
    echo "ERROR: VST3 FX not found at $VST3FX_PATH"
    exit 1
fi

# ── Stage files ───────────────────────────────────────────────────
echo ">> Staging files..."
rm -rf "$STAGING"
mkdir -p "$STAGING"

cp -R "$APP_PATH" "$STAGING/ChainHost.app"
cp -R "$VST3_PATH" "$STAGING/ChainHost.vst3"
cp -R "$VST3FX_PATH" "$STAGING/ChainHost FX.vst3"

# Symlinks for drag-and-drop install
ln -s /Applications "$STAGING/Applications"
ln -s "$HOME/Library/Audio/Plug-Ins/VST3" "$STAGING/VST3 Plug-Ins"

# ── Ad-hoc code sign ─────────────────────────────────────────────
echo ">> Code signing (ad-hoc)..."
codesign --force --deep --sign - "$STAGING/ChainHost.app" 2>/dev/null || true
codesign --force --deep --sign - "$STAGING/ChainHost.vst3" 2>/dev/null || true
codesign --force --deep --sign - "$STAGING/ChainHost FX.vst3" 2>/dev/null || true

# ── Create DMG ────────────────────────────────────────────────────
DMG_OUTPUT="$BUILD_DIR/${DMG_NAME}.dmg"
DMG_TEMP="$BUILD_DIR/${DMG_NAME}_temp.dmg"

echo ">> Creating DMG..."
rm -f "$DMG_OUTPUT" "$DMG_TEMP"

# Create a temporary read-write DMG
hdiutil create \
    -volname "ChainHost ${VERSION}" \
    -srcfolder "$STAGING" \
    -ov \
    -format UDRW \
    "$DMG_TEMP"

# Convert to compressed read-only DMG
hdiutil convert "$DMG_TEMP" \
    -format UDZO \
    -imagekey zlib-level=9 \
    -o "$DMG_OUTPUT"

rm -f "$DMG_TEMP"

# ── Cleanup ───────────────────────────────────────────────────────
rm -rf "$STAGING"

DMG_SIZE=$(du -h "$DMG_OUTPUT" | cut -f1)
echo ""
echo "=== Done! ==="
echo "  DMG: $DMG_OUTPUT"
echo "  Size: $DMG_SIZE"
echo ""
echo "To install:"
echo "  1. Open the DMG"
echo "  2. Drag ChainHost.app → Applications"
echo "  3. Drag ChainHost.vst3 → VST3 Plug-Ins"
echo "  4. Drag ChainHost FX.vst3 → VST3 Plug-Ins"
