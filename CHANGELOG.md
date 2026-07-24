# Changelog

All notable native-port changes are documented here. Shared Awakened PoE Trade
changes remain documented by the upstream project.

## 3.28.104-native.4

### Fixed

- Bundled Qt's Wayland EGL client-buffer integration in the AppImage. The
  previous AppImage accepted overlay shortcuts but could remain transparent
  because the release packager silently omitted this plugin.
- Added release verification that extracts the completed AppImage and fails
  when the required Wayland EGL plugin is absent or has unresolved libraries.
- Prevented the packaged application from inheriting a renderer from a nearby
  source checkout instead of using its verified embedded renderer.
- Isolated system-browser and file-manager launches from AppImage and
  layer-shell Qt environment variables.

## 3.28.104-native.3

### Added

- Native number entry for price-check filters without transferring keyboard
  focus away from Path of Exile.
- Wayland activation-token support when opening external trade links.
- Regression coverage for clean shutdown with connected renderer clients and
  advanced item-copy bindings.

### Changed

- Tray actions now use non-blocking desktop notifications and launch external
  applications without inheriting the overlay's layer-shell environment.
- Path of Exile protocol detection remains diagnostic: native Wayland is the
  supported target, but an inconsistent compositor protocol label no longer
  blocks the overlay.

### Fixed

- Restored upstream advanced item descriptions during item copy so modifier
  stats, sources, and ranges are parsed correctly.
- Held custom advanced-description bindings while pressing `C`, matching
  upstream behavior.
- Prevented a shutdown crash when quitting from the tray.
- Kept Settings and League selection compact, and stopped Dolphin from opening
  as an unminimizable layer-shell surface.
- Closed the price-check panel when Trade is opened and forwarded compositor
  activation to the system browser.

## 3.28.104-native.2

### Added

- Native-focused documentation site with correct AppImage downloads and
  self-service Plasma Wayland setup.
- Weekly read-only upstream monitor that opens one integration issue when
  canonical upstream commits require review.
- SPDX 2.3 release inventory plus GitHub build-provenance and SBOM
  attestations.
- Native tests for localized item detection, PoE window recognition, PoE
  application identification, updater versions, shortcut parsing, and Linux
  key mappings.

### Changed

- Pinned every GitHub Action to an immutable commit.
- Verified linuxdeploy, its Qt plugin, and the AppImage runtime against pinned
  SHA-256 values before release packaging.
- Clarified that the AppImage bundles and loads the KWin bridge automatically.
- Locked documentation dependencies and updated the release metadata.

### Fixed

- Prevented an empty configured window title from matching unrelated Wayland
  windows.

## 3.28.104-native.1

Initial native Linux release based on Awakened PoE Trade 3.28.104.

### Added

- C++20 Qt 6 / KDE native backend.
- Plasma Wayland layer-shell overlay that stays above fullscreen PoE without
  taking keyboard focus or revealing the taskbar.
- Native system tray, KGlobalAccel shortcuts, configuration storage, HTTP and
  WebSocket renderer transport, trade proxy, uploads, and updater.
- KWin focus, client-protocol, geometry, and cursor bridge.
- Wayland desktop validation and PoE client-protocol diagnostics.
- Startup dependency diagnostics and self-loading bundled KWin bridge.
- Reliable PoE item capture using clean Ctrl+C timing and `wl-paste` fallback.
- Quick and locked price-check behavior with compositor-native auto-hide.
- Ctrl, Alt, and Shift selectors plus native letter capture for both
  price-check hotkeys.
- Direct typing for plain chat commands to prevent stale clipboard item text.
- Embedded trade browser, game-log monitoring, stash search, Heist OCR, and
  optional Ctrl+wheel stash navigation.
- AppImage packaging, checksum verification, automated version tags/releases,
  upstream contract checks, native unit/integration tests in CI, issue
  templates, and self-service docs.

### Attribution

The renderer, item parser, trade behavior, data, and translations come from
[Awakened PoE Trade](https://github.com/SnosMe/awakened-poe-trade). See the
README for complete acknowledgements.
