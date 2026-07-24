#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
APP_VERSION="${APP_VERSION:-$(tr -d '[:space:]' < "$ROOT/native/VERSION")}"
APP_DIR="${APP_DIR:-$ROOT/native/build/AwakenedPoETrade.AppDir}"
OUTPUT_DIR="${OUTPUT_DIR:-$ROOT/native/build}"
ARCHITECTURE="${ARCHITECTURE:-x86_64}"
LINUXDEPLOY="${LINUXDEPLOY:-linuxdeploy-$ARCHITECTURE.AppImage}"
QT_PLUGIN="${QT_PLUGIN:-linuxdeploy-plugin-qt-$ARCHITECTURE.AppImage}"
APPIMAGE_RUNTIME="${APPIMAGE_RUNTIME:-}"
APPIMAGETOOL="${APPIMAGETOOL:-}"
PATCHELF="${PATCHELF:-$(command -v patchelf || true)}"

if [[ "${SKIP_BUILD:-0}" != "1" ]]; then
  "$ROOT/build-native.sh"
fi
if [[ ! -x "$ROOT/native/build/awakened-poe-trade-native" ]]; then
  echo "error: native/build/awakened-poe-trade-native was not built" >&2
  exit 1
fi
for tool in "$LINUXDEPLOY" "$QT_PLUGIN"; do
  if ! command -v "$tool" >/dev/null 2>&1 && [[ ! -x "$tool" ]]; then
    printf 'error: %s is required\n' "$tool" >&2
    exit 1
  fi
done
if [[ ! -x "$PATCHELF" ]]; then
  echo "error: patchelf 0.18 or newer is required (set PATCHELF)" >&2
  exit 1
fi

rm -rf -- "$APP_DIR"
DESTDIR="$APP_DIR" cmake --install "$ROOT/native/build" --prefix /usr

if [[ "$QT_PLUGIN" == *.AppImage ]]; then
  QT_TOOL_DIRECTORY="$ROOT/native/build/linuxdeploy-plugin-qt-tool"
  rm -rf -- "$QT_TOOL_DIRECTORY"
  install -d "$QT_TOOL_DIRECTORY"
  (
    cd "$QT_TOOL_DIRECTORY"
    "$QT_PLUGIN" --appimage-extract >/dev/null
  )
  install -m 0755 "$(command -v strip)" \
    "$QT_TOOL_DIRECTORY/squashfs-root/usr/bin/strip"
  install -m 0755 "$PATCHELF" \
    "$QT_TOOL_DIRECTORY/squashfs-root/usr/bin/patchelf"
  export PATH="$QT_TOOL_DIRECTORY/squashfs-root/usr/bin:$PATH"
else
  export PATH="$(dirname -- "$(realpath "$QT_PLUGIN")"):$PATH"
fi
export REAL_QMAKE="${QMAKE:-$(command -v qmake6 || command -v qmake)}"
export QT_PLUGIN_STAGING="$ROOT/native/build/qt-plugins"
rm -rf -- "$QT_PLUGIN_STAGING"
for subdirectory in \
  iconengines networkinformation org.kde.kglobalacceld.platforms \
  platforminputcontexts platforms platformthemes styles tls \
  wayland-decoration-client wayland-graphics-integration-client \
  wayland-shell-integration; do
  if [[ -d "/usr/lib/qt6/plugins/$subdirectory" ]]; then
    install -d "$QT_PLUGIN_STAGING/$subdirectory"
    cp -a "/usr/lib/qt6/plugins/$subdirectory/." \
      "$QT_PLUGIN_STAGING/$subdirectory/"
  fi
done
install -d "$QT_PLUGIN_STAGING/imageformats"
for plugin in libqgif.so libqico.so libqjpeg.so libqsvg.so libqwebp.so; do
  install -m 0755 "/usr/lib/qt6/plugins/imageformats/$plugin" \
    "$QT_PLUGIN_STAGING/imageformats/$plugin"
done
export QMAKE="$ROOT/native/resources/qmake-appimage-wrapper.sh"
export EXTRA_PLATFORM_PLUGINS="${EXTRA_PLATFORM_PLUGINS:-libqwayland.so;libqoffscreen.so;libqminimal.so}"
OUTPUT="$OUTPUT_DIR/Awakened-PoE-Trade-Native-$APP_VERSION-$ARCHITECTURE.AppImage"

LINUXDEPLOY_RUN="$LINUXDEPLOY"
if [[ "$LINUXDEPLOY" == *.AppImage ]]; then
  TOOL_DIRECTORY="$ROOT/native/build/linuxdeploy-tool"
  rm -rf -- "$TOOL_DIRECTORY"
  install -d "$TOOL_DIRECTORY"
  (
    cd "$TOOL_DIRECTORY"
    "$LINUXDEPLOY" --appimage-extract >/dev/null
  )
  # linuxdeploy's continuous AppImage ships binutils 2.35, which cannot
  # understand modern RELR sections. Match the build host's ELF format.
  install -m 0755 "$(command -v strip)" \
    "$TOOL_DIRECTORY/squashfs-root/usr/bin/strip"
  install -m 0755 "$PATCHELF" \
    "$TOOL_DIRECTORY/squashfs-root/usr/bin/patchelf"
  LINUXDEPLOY_RUN="$TOOL_DIRECTORY/squashfs-root/AppRun"
  if [[ -z "$APPIMAGETOOL" ]]; then
    APPIMAGETOOL="$TOOL_DIRECTORY/squashfs-root/plugins/linuxdeploy-plugin-appimage/usr/bin/appimagetool"
  fi
fi

"$LINUXDEPLOY_RUN" \
  --verbosity=2 \
  --appdir "$APP_DIR" \
  --desktop-file "$APP_DIR/usr/share/applications/awakened-poe-trade-native.desktop" \
  --icon-file "$APP_DIR/usr/share/icons/hicolor/128x128/apps/awakened-poe-trade-native.png" \
  --plugin qt

if [[ -z "$APPIMAGETOOL" ]]; then
  APPIMAGETOOL="$(command -v appimagetool || true)"
fi
if [[ ! -x "$APPIMAGETOOL" ]]; then
  echo "error: appimagetool is required (set APPIMAGETOOL)" >&2
  exit 1
fi
if [[ -z "$APPIMAGE_RUNTIME" || ! -f "$APPIMAGE_RUNTIME" ]]; then
  echo "error: the AppImage type-2 runtime is required (set APPIMAGE_RUNTIME)" >&2
  exit 1
fi

ARCH="$ARCHITECTURE" "$APPIMAGETOOL" \
  --no-appstream \
  --runtime-file "$APPIMAGE_RUNTIME" \
  "$APP_DIR" \
  "$OUTPUT"

chmod +x "$OUTPUT"
sha256sum "$OUTPUT" > "$OUTPUT.sha256"
printf 'Packaged: %s\n' "$OUTPUT"
