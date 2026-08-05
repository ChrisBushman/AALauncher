#!/usr/bin/env bash
#
# Build AALauncher.app for macOS and package it as AALauncher-macOS.zip.
#
# macOS is built locally (there is no macOS runner in CI for this repo), so this
# script is the reproducible packaging path. It:
#   1. Configures + builds the flat AALauncher binary (Ninja).
#   2. Assembles a .app skeleton around it (Info.plist, icon, qt.conf).
#   3. Runs macdeployqt to bundle Qt6 + the QtWebEngine stack.
#   4. Applies the WebEngine helper fix (see below) -- WITHOUT it the webview is
#      blank because the render helper process crashes on launch.
#   5. Ad-hoc signs the helper and then the whole bundle.
#   6. Zips with ditto (preserves the symlink and code signatures).
#
# The WebEngine helper fix
# ------------------------
# QtWebEngineCore and the Qt frameworks it pulls in reference their siblings via
# @executable_path/../Frameworks/X. For the main app that resolves to
# AALauncher.app/Contents/Frameworks (correct). But QtWebEngine spawns a second
# executable -- QtWebEngineProcess.app/Contents/MacOS/QtWebEngineProcess -- and
# for *that* process @executable_path/../Frameworks points inside the helper
# .app, where those frameworks (QtWebChannel, QtCore, the ICU dylibs, ...) do not
# exist. The helper dies in dyld and no render process comes up, so the webview
# is blank. macdeployqt does not fix this. We drop a single symlink
#     QtWebEngineProcess.app/Contents/Frameworks -> (the app's real Frameworks)
# so every @executable_path/../Frameworks/X reference the helper makes -- at any
# depth, frameworks and plain dylibs alike -- resolves to the real libraries.
#
# Usage: mac/package-macos.sh [VERSION]   (VERSION defaults to 1.2.3)

set -euo pipefail

VERSION="${1:-1.2.3}"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"      # the mac/ dir
BUILD="$HERE/build"
STAGE="$HERE/stage"
APP="$STAGE/AALauncher.app"
ZIP="$HERE/AALauncher-macOS.zip"

QT_PREFIX="${QT_PREFIX:-$(brew --prefix qt 2>/dev/null || echo /usr/local/opt/qt)}"
MACDEPLOYQT="$QT_PREFIX/bin/macdeployqt"
[ -x "$MACDEPLOYQT" ] || { echo "macdeployqt not found at $MACDEPLOYQT (set QT_PREFIX)"; exit 1; }

echo ">> Configuring + building (Ninja) against $QT_PREFIX"
# Reconfigure from clean: a stale build/ configured with a different generator
# makes cmake error out on a generator mismatch, which is easy to miss.
rm -rf "$BUILD"
cmake -S "$HERE" -B "$BUILD" -G Ninja -DCMAKE_PREFIX_PATH="$QT_PREFIX"
cmake --build "$BUILD"

echo ">> Assembling $APP"
rm -rf "$STAGE"; mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Resources"
cp "$BUILD/AALauncher" "$APP/Contents/MacOS/AALauncher"
cp "$HERE/amulets.icns" "$APP/Contents/Resources/amulets.icns"
# qt.conf keeps Qt from wandering off to the Homebrew prefix at runtime.
printf '[Paths]\nPlugins = PlugIns\n' > "$APP/Contents/Resources/qt.conf"

cat > "$APP/Contents/Info.plist" <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
	<key>CFBundleDisplayName</key>
	<string>Amulets &amp; Armor Launcher</string>
	<key>CFBundleExecutable</key>
	<string>AALauncher</string>
	<key>CFBundleIconFile</key>
	<string>amulets.icns</string>
	<key>CFBundleIdentifier</key>
	<string>com.chrisbushman.aalauncher</string>
	<key>CFBundleInfoDictionaryVersion</key>
	<string>6.0</string>
	<key>CFBundleName</key>
	<string>AALauncher</string>
	<key>CFBundlePackageType</key>
	<string>APPL</string>
	<key>CFBundleShortVersionString</key>
	<string>$VERSION</string>
	<key>CFBundleSignature</key>
	<string>????</string>
	<key>CFBundleVersion</key>
	<string>$VERSION</string>
	<key>LSMinimumSystemVersion</key>
	<string>10.14</string>
</dict>
</plist>
PLIST

echo ">> Running macdeployqt"
"$MACDEPLOYQT" "$APP" -always-overwrite

echo ">> Applying WebEngine helper Frameworks symlink fix"
HELPER_CONTENTS="$APP/Contents/Frameworks/QtWebEngineCore.framework/Versions/A/Helpers/QtWebEngineProcess.app/Contents"
if [ -d "$HELPER_CONTENTS" ]; then
    rm -f "$HELPER_CONTENTS/Frameworks"
    # 6 levels up from QtWebEngineProcess.app/Contents lands on the app's
    # Contents/Frameworks (Frameworks/QtWebEngineCore.framework/Versions/A/
    # Helpers/QtWebEngineProcess.app/Contents -> ../../../../../..).
    ln -s ../../../../../.. "$HELPER_CONTENTS/Frameworks"
    # Prove it resolves to a real framework before trusting it.
    test -e "$HELPER_CONTENTS/Frameworks/QtWebChannel.framework/Versions/A/QtWebChannel" \
        || { echo "helper Frameworks symlink does not resolve -- aborting"; exit 1; }
else
    echo "WARNING: QtWebEngineProcess helper not found; webview may be broken."
fi

echo ">> Ad-hoc signing (helper first, then the whole bundle)"
codesign -s - -f "$APP/Contents/Frameworks/QtWebEngineCore.framework/Versions/A/Helpers/QtWebEngineProcess.app"
codesign -s - -f --deep "$APP"
codesign --verify --deep --strict "$APP"

echo ">> Zipping -> $ZIP"
rm -f "$ZIP"
ditto -c -k --sequesterRsrc --keepParent "$APP" "$ZIP"

echo ">> Done: $ZIP ($(du -h "$ZIP" | cut -f1))"
