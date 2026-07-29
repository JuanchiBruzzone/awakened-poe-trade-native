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

# Arch's rolling GCC runtime can be built against a glibc newer than the rest
# of the AppImage. Pin the Fortran runtime used by OpenCV/LAPACK to Ubuntu
# 22.04's glibc-2.34-compatible build instead.
COMPAT_DIRECTORY="$(mktemp -d)"
trap 'rm -rf -- "$COMPAT_DIRECTORY"' EXIT
GFORTRAN_DEB="$COMPAT_DIRECTORY/libgfortran5.deb"
QUADMATH_DEB="$COMPAT_DIRECTORY/libquadmath0.deb"
curl -fsSL \
  https://archive.ubuntu.com/ubuntu/pool/main/g/gcc-12/libgfortran5_12.3.0-1ubuntu1~22.04.3_amd64.deb \
  -o "$GFORTRAN_DEB"
curl -fsSL \
  https://archive.ubuntu.com/ubuntu/pool/main/g/gcc-12/libquadmath0_12.3.0-1ubuntu1~22.04.3_amd64.deb \
  -o "$QUADMATH_DEB"
printf '%s  %s\n%s  %s\n' \
  14dbb269458f60ca64af646c9300b8b85dd962ddbb434ba362062a54c07c4db6 "$GFORTRAN_DEB" \
  c4c59e67e76674c8c494ce5e0415b71d770a759b48893c0d3cbabddceeb690b2 "$QUADMATH_DEB" \
  | sha256sum -c -
install -d "$COMPAT_DIRECTORY/root"
for package in "$GFORTRAN_DEB" "$QUADMATH_DEB"; do
  ar p "$package" data.tar.zst | bsdtar -xf - -C "$COMPAT_DIRECTORY/root"
done
for library in libgfortran.so.5 libgfortran.so.5.0.0 libquadmath.so.0 libquadmath.so.0.0.0; do
  cp -a \
    "$COMPAT_DIRECTORY/root/usr/lib/x86_64-linux-gnu/$library" \
    "$APP_DIR/usr/lib/$library"
done

# The current Qt/KDE bundle targets glibc 2.43. Reject a single accidentally
# newer dependency before producing another AppImage that passes CI but cannot
# start on the supported rolling-release baseline.
while IFS= read -r -d '' binary; do
  if ! readelf -h "$binary" >/dev/null 2>&1; then
    continue
  fi
  required="$(
    readelf --version-info "$binary" \
      | sed -nE 's/.*GLIBC_([0-9.]+).*/\1/p' \
      | sort -Vu \
      | tail -1
  )"
  if [[ -n "$required" ]] &&
     [[ "$(printf '%s\n' 2.43 "$required" | sort -V | tail -1)" != "2.43" ]]; then
    printf 'error: %s requires glibc %s (maximum supported is 2.43)\n' \
      "$binary" "$required" >&2
    exit 1
  fi
done < <(find "$APP_DIR/usr" -type f -print0)

# linuxdeploy-plugin-qt does not recognize Qt 6's Wayland client-buffer
# integration category, even when its plugin root is supplied through qmake.
# Without this plugin the layer-shell surface maps and accepts shortcuts but
# cannot create a render context, leaving the overlay completely transparent.
WAYLAND_EGL_SOURCE="$QT_PLUGIN_STAGING/wayland-graphics-integration-client/libqt-plugin-wayland-egl.so"
WAYLAND_EGL_DIRECTORY="$APP_DIR/usr/plugins/wayland-graphics-integration-client"
WAYLAND_EGL_PLUGIN="$WAYLAND_EGL_DIRECTORY/libqt-plugin-wayland-egl.so"
if [[ ! -f "$WAYLAND_EGL_SOURCE" ]]; then
  echo "error: Qt Wayland EGL client-buffer integration was not found" >&2
  exit 1
fi
install -d "$WAYLAND_EGL_DIRECTORY"
install -m 0755 "$WAYLAND_EGL_SOURCE" "$WAYLAND_EGL_PLUGIN"
"$PATCHELF" --set-rpath '$ORIGIN/../../lib' "$WAYLAND_EGL_PLUGIN"
if LD_LIBRARY_PATH="$APP_DIR/usr/lib" ldd "$WAYLAND_EGL_PLUGIN" | grep -q 'not found'; then
  echo "error: bundled Qt Wayland EGL plugin has unresolved dependencies" >&2
  LD_LIBRARY_PATH="$APP_DIR/usr/lib" ldd "$WAYLAND_EGL_PLUGIN" >&2
  exit 1
fi

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
