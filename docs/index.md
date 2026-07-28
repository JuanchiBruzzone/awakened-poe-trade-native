---
title: Awakened PoE Trade Native
---

# Native Path of Exile tools for Plasma Wayland

Awakened PoE Trade Native carries the proven
[Awakened PoE Trade](https://github.com/SnosMe/awakened-poe-trade)
interface, parser, trade logic, data, and translations onto a native C++20
Qt/KDE host.

> **Wayland/Wayland target:** KDE Plasma, the native host, and Path of Exile 1
> are expected to run with native Wayland.

## What the native host adds

- A layer-shell overlay that stays above fullscreen PoE without revealing the
  panel or taking keyboard focus.
- KDE global shortcuts, a system tray, native clipboard handling, and reliable
  Proton/Wayland input.
- Passive text and number entry in overlay fields without activating the
  layer-shell surface.
- Quick and locked price-check modes with compositor cursor tracking.
- Startup diagnostics for required host tools and PoE window recognition.
- A self-loading bundled KWin bridge, native Heist OCR, automatic AppImage
  updates, and rollback-safe replacement.

## Start here

1. [Download the current AppImage](./download).
2. Read the [Linux dependencies and setup guide](./linux-dependencies).
3. Follow the [quick start](./quick-start).
4. If anything behaves unexpectedly, use the
   [self-service troubleshooting guide](./troubleshooting).

## Built on upstream

This port would not exist without SnosMe and every Awakened PoE Trade
contributor. Upstream owns the application behavior users recognize; this fork
maintains the native Plasma Wayland integration and continuously reviews new
upstream work for compatibility.

- [Visit and support upstream](https://github.com/SnosMe/awakened-poe-trade)
- [Native source and issue tracker](https://github.com/JuanchiBruzzone/awakened-poe-trade-native)
- [Latest native release](https://github.com/JuanchiBruzzone/awakened-poe-trade-native/releases/latest)

## Maintenance notice

The native fork is maintained with AI assistance because its owner does not
have time to maintain and develop it alone. It is provided as-is. Reproducible
issues, focused pull requests, testing, and documentation improvements are
welcome.
