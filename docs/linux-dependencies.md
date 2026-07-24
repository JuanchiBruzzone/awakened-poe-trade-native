# Linux dependencies and setup

## Supported target

The native host targets x86-64 Linux with KDE Plasma 6 on Wayland. Continuous
integration and release packaging use Arch Linux. Other distributions can work
when they provide compatible Qt 6, KDE Frameworks 6, LayerShellQt, and WebEngine
packages, but they are not release-tested automatically.

Both the desktop session and Path of Exile must be native Wayland. X11 desktop
sessions and PoE running through XWayland are unsupported and are identified in
startup logs.

## Runtime components

| Component | Why it is needed | Required |
|---|---|---|
| KDE Plasma 6 / KWin Wayland | Overlay stacking, global shortcuts, focus and cursor bridge | Yes |
| Qt 6 WebEngine | Runs the unchanged Awakened PoE Trade renderer | Bundled in AppImage |
| LayerShellQt | Places the overlay above fullscreen PoE without showing the panel | Bundled in AppImage |
| KDE GlobalAccel | Registers configurable system-wide shortcuts | Host integration required |
| `ydotool` and `ydotoold` | Sends copy, chat, and command keystrokes to PoE | Yes |
| `wl-clipboard` | Reliable Wayland clipboard fallback through `wl-paste` | Yes |
| KWin bridge script | Game-focus gating and compositor-native cursor tracking | Strongly recommended |
| OpenCV and Tesseract data | Heist OCR | Optional |
| `/dev/input/event*` access | Ctrl+wheel stash navigation only | Optional |

The AppImage bundles application libraries, the renderer, and the KWin script
package, but Linux security boundaries still require the host services above.

## Arch Linux

Runtime:

```bash
sudo pacman -S --needed ydotool wl-clipboard kglobalaccel kpackage kconfig
```

Source build:

```bash
sudo pacman -S --needed \
  base-devel git cmake ninja extra-cmake-modules \
  qt6-base qt6-webengine qt6-svg layer-shell-qt \
  kglobalaccel kpackage kconfig \
  opencv tesseract tesseract-data-eng tesseract-data-rus \
  nodejs npm curl file patchelf appstream
```

Package names differ on other distributions. Search for packages providing:

- Qt 6 Core, GUI, Widgets, Network, DBus, WebEngine, and Concurrent;
- KDE Frameworks 6 GlobalAccel;
- LayerShellQt 6;
- OpenCV core/image processing;
- Tesseract development files;
- CMake, Ninja, a C++20 compiler, Node.js, and npm.

## `ydotool`

`ydotool` requires its daemon and access to `/dev/uinput`. Enable the service
provided by your distribution. The unit name varies:

```bash
systemctl --user status ydotool.service
systemctl --user status ydotoold.service
systemctl status ydotool.service
```

Only one of those needs to exist. Follow the distribution package's uinput
permission instructions; do not run the overlay as root.

Verify the client is available:

```bash
command -v ydotool
ydotool --help
```

## KWin integration

From a source checkout:

```bash
./install-kwin-integration.sh
```

This installs the script under:

```text
~/.local/share/kwin/scripts/awakened-poe-trade-native-focus/
```

It reports only active-window metadata and cursor coordinates to the local
native process. It does not record keyboard input.

Verify the installed package:

```bash
kpackagetool6 --type=KWin/Script --list | grep awakened-poe-trade-native-focus
```

## Configuration and data

Default data directory:

```text
${XDG_CONFIG_HOME:-~/.config}/awakened-poe-trade/apt-data/
```

Important paths:

| Path | Contents |
|---|---|
| `config.json` | User settings and widgets |
| `config.json.tmp` | Temporary settings state, when present |
| `files/` | User-imported overlay files |
| `cv-ocr/` | Heist OCR templates and language data |
| `updates/` | Staged native AppImage updates |

Use the tray menu's **Open config folder** action instead of guessing the path.

## Heist OCR

Install the pinned upstream OCR data:

```bash
./install-ocr-data.sh
```

The installer verifies the archive checksum and installs only the required
template and trained-language files.

## Optional Ctrl+wheel input access

This feature reads mouse-wheel and Ctrl state from Linux evdev devices. Granting
input-device access can expose all keyboard events to every process with the
same permission, so use your distribution's narrowly scoped udev policy when
possible. Do not make `/dev/input/event*` world-readable.

The rest of the application works when this optional permission is unavailable.
