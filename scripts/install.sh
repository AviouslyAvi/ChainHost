#!/bin/bash
# ChainHost Installer — removes macOS quarantine and copies to Applications

APP_NAME="ChainHost.app"
DMG_MOUNT=""

echo "==================================="
echo "  ChainHost Installer"
echo "==================================="
echo ""

# Check if running from a mounted DMG
if [[ "$0" == /Volumes/* ]]; then
    SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
    APP_PATH="$SCRIPT_DIR/$APP_NAME"
else
    # Look for the app next to this script or in common locations
    SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
    if [[ -d "$SCRIPT_DIR/$APP_NAME" ]]; then
        APP_PATH="$SCRIPT_DIR/$APP_NAME"
    elif [[ -d "$HOME/Downloads/$APP_NAME" ]]; then
        APP_PATH="$HOME/Downloads/$APP_NAME"
    else
        echo "Could not find $APP_NAME."
        echo "Place this script next to $APP_NAME and try again."
        exit 1
    fi
fi

echo "Found: $APP_PATH"
echo ""

# Remove quarantine flag
echo "Removing macOS quarantine flag..."
xattr -cr "$APP_PATH"
echo "Done."
echo ""

# Ask to copy to Applications
read -p "Copy to /Applications? (y/n) " -n 1 -r
echo ""

if [[ $REPLY =~ ^[Yy]$ ]]; then
    if [[ -d "/Applications/$APP_NAME" ]]; then
        echo "Removing old version..."
        rm -rf "/Applications/$APP_NAME"
    fi
    cp -R "$APP_PATH" /Applications/
    xattr -cr "/Applications/$APP_NAME"
    echo "Installed to /Applications/$APP_NAME"
else
    echo "Skipped. You can run ChainHost from its current location."
fi

echo ""
echo "Done! Launch ChainHost from Applications or Spotlight."
