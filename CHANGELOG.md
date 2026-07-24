# Changelog

All notable native-port changes are documented here. Shared Awakened PoE Trade
changes remain documented by the upstream project.

## 3.28.104-native.2

### Added

- Native-focused documentation site with correct AppImage downloads and
  self-service Plasma Wayland setup.
- Weekly read-only upstream monitor that opens one integration issue when
  canonical upstream commits require review.
- SPDX 2.3 release inventory plus GitHub build-provenance and SBOM
  attestations.
- Native tests for localized item detection, Wayland-only game gating, PoE
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
- Wayland-only desktop and PoE client validation.
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
