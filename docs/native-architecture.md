# Native host architecture

## Compatibility boundary

The Vue renderer is treated as an independently deployable client. The native host implements the same local transport used by the Electron main process:

```text
renderer/dist
  ├─ GET /config
  ├─ GET/POST /uploads/*
  ├─ /proxy/*
  └─ WS /events
```

This is preferable to introducing a second frontend-specific bridge because it keeps browser mode, renderer development, and native overlay mode on one protocol.

## Components

- `NativeHost`: application coordinator and IPC action dispatcher.
- `EventServer`: loopback HTTP, WebSocket, static assets, uploads, and allowlisted proxy.
- `OverlayWindow`: Qt WebEngine plus LayerShellQt surface management.
- `ShortcutManager`: stable KGlobalAccel actions generated from upstream `HostConfig.shortcuts`.
- `InputInjector`: bounded, argument-only `ydotool` process execution.
- `ClipboardService`: PoE item detection, polling, and optional restoration.
- `GameConfigReader`: resolves the advanced item descriptions key from `production_Config.ini`.
- `GameLogWatcher`: tails new `Client.txt` lines and emits existing game-log events.
- `ConfigStore`: upstream-compatible `apt-data/config.json` persistence.
- `GameWindowTracker`: optional KWin-script/D-Bus bridge for active-game gating.
- `ScreenshotService`: asynchronous KWin ScreenShot2 active-window capture.
- `HeistOcrService`: upstream-compatible OpenCV lock matching and Tesseract OCR.
- `AppTray`: lifecycle and debugging controls.

## Shortcut policy

KGlobalAccel owns registration. The normal mode never overwrites another application or KDE action. Conflicts are returned as log events. The `--steal-shortcuts` switch exists for controlled testing and deliberately uses KDE's system-wide steal API.

Stable action IDs are generated from the action type, target, and index. KDE can therefore persist user choices between launches.

## Input policy

Wayland intentionally prevents arbitrary clients from injecting input. This host does not pretend otherwise. It uses one explicit backend (`ydotool`) with:

- no shell evaluation;
- a small known key map;
- serialized commands;
- timeouts and error reporting;
- minimal modifier injection when the triggering shortcut already holds a required modifier.

A future RemoteDesktop portal backend can implement the same interface where compositor support is adequate.

## Overlay policy

The layer-shell surface fills the output without reserving screen space. Passive
mode is keyboard- and mouse-transparent. The surface deliberately uses
`KeyboardInteractivityNone`, including while pointer-interactive, so Path of
Exile keeps keyboard focus and Plasma does not reveal its panel above the
fullscreen game.

The renderer's existing `track-area` event is preserved. On Plasma Wayland,
the KWin bridge supplies compositor-native cursor coordinates because an
unfocused client cannot reliably poll the global pointer. While the configured
hold modifier remains pressed, entering the widget area activates pointer
interaction. After the modifier is released, moving past the close threshold
emits the existing `hide-exclusive-widget` event. A short settling interval
prevents layer mapping from being mistaken for real pointer movement.

## Security boundaries

- Server binds to loopback unless explicitly overridden.
- Static files are constrained to the configured renderer directory.
- Upload reads use basenames and request bodies are capped.
- Proxy hosts match the upstream allowlist and cannot be supplied arbitrarily.
- WebSocket payload size is capped.
- External navigation leaves the embedded overlay and opens in the default browser.


## Active-window policy

Wayland does not expose a generic API that lets an arbitrary application
enumerate or focus other clients. On KDE, the optional
`awakened-poe-trade-native-focus` KWin script reports the active window caption,
resource class, PID, geometry, client protocol, and cursor position to the host
over a private session D-Bus service. It neither captures key contents nor
changes the active window. When the bridge is connected, game actions are
disabled while Path of Exile is not active or is running through XWayland,
matching the native host's Wayland-only focus policy. The
overlay-toggle action remains registered only so an active overlay can be
closed; opening it from unrelated applications is rejected. The dispatcher
repeats the check as defense in depth.

Without the bridge, the host preserves compatibility by allowing hotkeys globally, matching historical behavior, and logs that focus gating is unavailable.

## OCR policy

KDE's ScreenShot2 service captures the active Path of Exile window directly, without
showing a desktop-selection dialog or briefly activating the overlay. Processing runs
off the UI thread and ports the upstream lock-template thresholds, line grouping,
color mask, confidence cutoff, and English/Russian language mapping.

Install the upstream OCR assets with `./install-ocr-data.sh`. The helper downloads
the archive linked by the official OCR Guide, verifies a pinned SHA-256 checksum,
and installs only the template and trained-language files needed by the native host.

## Update policy

The native release workflow produces a versioned x86-64 AppImage. When the
application is itself running as a writable AppImage, the updater checks the
latest release in the repository compiled into the build, selects only an asset
whose name identifies it as the native AppImage, verifies the SHA-256 digest
reported by GitHub, stages it, keeps the previous AppImage as a rollback copy,
and restarts into the replacement.

Package-managed and ordinary local builds still receive update notifications,
but the unchanged renderer offers a manual download instead of overwriting
files owned by the package manager. `--no-updates` preserves upstream's
download-disabled behavior.
