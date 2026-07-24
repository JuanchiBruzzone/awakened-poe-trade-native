# Awakened PoE Trade Native

A native Linux host for [Awakened PoE Trade](https://github.com/SnosMe/awakened-poe-trade), focused on KDE Plasma and Wayland.

The upstream Vue interface, item parser, trade logic, data, and translations are preserved. The Electron backend is complemented by a C++20 host built with Qt 6, KDE Frameworks, and LayerShellQt.

> This is an unofficial community port. It is not affiliated with or endorsed by Grinding Gear Games.

> ## **IMPORTANT: THIS PROJECT IS MAINTAINED WITH AI ASSISTANCE**
>
> **The reason is simple: I do not have time to maintain and develop this
> project by myself. AI assistance is used so the project can continue to
> receive fixes, upstream integrations, documentation, and new features. The
> software is provided as-is. Please open a
> [GitHub issue](https://github.com/JuanchiBruzzone/awakened-poe-trade-native/issues)
> for reproducible bugs or feature requests, and open a pull request when you
> have a tested fix or improvement. AI-assisted changes are reviewed and
> tested where practical, but users and contributors must verify behavior in
> their own environment. Help make the project better by including clear
> reproduction steps, logs, platform details, and focused patches.**

## Highlights

- Native Wayland layer-shell overlay above fullscreen Path of Exile
- Price checking with quick and locked modes
- KDE global shortcuts with editable Ctrl, Alt, or Shift price-check bindings
- System tray integration
- Native clipboard capture and reliable Proton/Wayland command entry
- KWin focus and cursor bridge without taking focus away from the game
- Embedded trade browser
- Client log monitoring, stash search, chat commands, and Heist OCR
- AppImage packaging and native release updates
- Upstream compatibility checks and a documented synchronization workflow

The live-tested configuration is Path of Exile 1 running natively on Plasma Wayland. The shared renderer and trade behavior remain aligned with upstream Awakened PoE Trade.

This application is Wayland-only. At startup it checks the desktop session and
required host tools. The KWin bridge also verifies that Path of Exile itself is
a native Wayland client; XWayland game windows are rejected with an explicit
log message.

## Download

Download the latest AppImage from the [GitHub Releases page](https://github.com/JuanchiBruzzone/awakened-poe-trade-native/releases).

```bash
chmod +x Awakened-PoE-Trade-Native-*.AppImage
./Awakened-PoE-Trade-Native-*.AppImage
```

The AppImage includes the application and renderer. The following host components are still required:

- KDE Plasma 6 on Wayland
- `ydotool` with a running `ydotoold` service
- KDE global shortcuts
- `wl-clipboard`

Read [Linux dependencies and setup](docs/linux-dependencies.md) before
installing. For logs, recovery steps, and common fixes, use the
[self-service troubleshooting guide](docs/troubleshooting.md).

Install and enable the optional KWin bridge from a source checkout for strict game-focus gating, cursor-based quick-check auto-hide, and overlay integration:

```bash
./install-kwin-integration.sh
```

## Default behavior

- Quick Price Check appears without taking focus from Path of Exile. Release its modifier and move the cursor to hide it; keep the modifier held and move onto the popup to interact.
- Locked Price Check stays open until closed.
- Plain chat commands such as `/hideout` and `/exit` are typed directly and never reuse item clipboard contents.
- Shift+Space toggles the full overlay in the tested configuration. All shortcuts remain editable in Settings.

## Build from source

On an Arch Linux or compatible Plasma system, install:

```text
cmake ninja extra-cmake-modules
qt6-base qt6-webengine layer-shell-qt
kglobalaccel kpackage kconfig
opencv tesseract tesseract-data-eng tesseract-data-rus
nodejs npm ydotool wl-clipboard
```

Then run:

```bash
./build-native.sh
ctest --test-dir native/build --output-on-failure
./native/build/awakened-poe-trade-native
```

For a per-user installation:

```bash
./install-native-user.sh
./install-kwin-integration.sh
```

Heist OCR assets can be installed with:

```bash
./install-ocr-data.sh
```

See [Native architecture](docs/native-architecture.md) for the backend design and security boundaries.

## Upstream updates

The port intentionally keeps native changes isolated under `native/` wherever possible. To review new upstream work:

```bash
./sync-upstream.sh --check
```

After reviewing, merge from a clean branch:

```bash
./sync-upstream.sh --merge
python scripts/check-upstream-contract.py --require-complete
```

See [Upstream synchronization](docs/upstream-sync.md) for the full workflow.

## Known platform requirement

Ctrl+mouse-wheel stash navigation reads Linux input events. If `/dev/input/event*` access is unavailable, that optional feature is disabled and the application logs the reason. Price checking, overlays, commands, and other shortcuts continue to work.

For all known setup and runtime symptoms, see
[Troubleshooting](docs/troubleshooting.md).

## Development

The original frontend development guide remains in [DEVELOPING.md](DEVELOPING.md). Native releases use tags such as `v3.28.104-native.1`, based on the corresponding upstream application version.

Native releases are automated. A reviewed change to
[`native/VERSION`](native/VERSION) on `master` runs the complete verification
and packaging pipeline, creates the matching Git tag, and publishes the
AppImage plus its SHA-256 file. Never reuse or move an existing release tag.

## Issues, fixes, and feature requests

Use the [issue tracker](https://github.com/JuanchiBruzzone/awakened-poe-trade-native/issues)
for native Linux problems and requests. Before reporting a problem:

1. Confirm Path of Exile, Plasma, and the application are all running under
   the expected Wayland session.
2. Include the application version, Plasma version, distribution, GPU, and
   whether the log identifies Path of Exile as native Wayland or XWayland.
3. Include relevant logs without account names, session cookies, trade
   whispers, item contents, or other personal data.
4. Describe the shortcut pressed, expected result, actual result, and whether
   it reproduces consistently.
5. Check whether the same frontend behavior exists in upstream Awakened PoE
   Trade before filing a native-backend issue.

Pull requests are welcome. Keep upstream renderer changes minimal, place native
Linux work under `native/` where possible, run the compatibility checker, and
include the exact tests performed. For upstream-owned application logic,
translations, item parsing, or trade behavior, contribute to
[SnosMe/awakened-poe-trade](https://github.com/SnosMe/awakened-poe-trade)
first whenever appropriate.

See [CONTRIBUTING.md](CONTRIBUTING.md) for the complete contribution and
release checklist and [SECURITY.md](SECURITY.md) for private vulnerability
reporting. Published changes are recorded in [CHANGELOG.md](CHANGELOG.md).

## Thanks and attribution

This project exists because of the exceptional work of:

- [SnosMe](https://github.com/SnosMe) and every contributor to [Awakened PoE Trade](https://github.com/SnosMe/awakened-poe-trade)
- [Exiled Exchange 2](https://github.com/Kvan7/Exiled-Exchange-2) and its contributors, whose native Linux experiments helped inform this port
- [libuiohook](https://github.com/kwhat/libuiohook)
- [RePoE](https://github.com/brather1ng/RePoE)
- [poeprices.info](https://www.poeprices.info/)
- [poe.ninja](https://poe.ninja/)
- The Qt, KDE Plasma, LayerShellQt, `ydotool`, OpenCV, and Tesseract communities

Thank you to all upstream maintainers, translators, issue reporters, testers, and contributors. The native host is designed to preserve and extend their work, not replace its authorship.

## License

MIT. See [LICENSE](LICENSE). Original Awakened PoE Trade copyright remains with Alexander Drozdov and upstream contributors.
