# Troubleshooting

Start with the latest release and confirm the requirements in
[Linux dependencies and setup](linux-dependencies.md).

## Finding logs

### In the application

Open the overlay, choose **Settings**, then **Debug**. The native host streams
its current-session log into that page.

### From a terminal

Run the AppImage and save a copy:

```bash
APPIMAGE_EXTRACT_AND_RUN=1 \
  ./Awakened-PoE-Trade-Native-*.AppImage 2>&1 | tee apt-native.log
```

`APPIMAGE_EXTRACT_AND_RUN=1` also works around systems without FUSE.

### Plasma journal

Applications launched by Plasma may appear in the user journal:

```bash
journalctl --user --since "10 minutes ago" | grep -i awakened
```

KWin script errors:

```bash
journalctl --user -u plasma-kwin_wayland.service \
  --since "10 minutes ago"
```

Before posting logs, remove account names, paths containing your login, session
cookies, trade whispers, item text, and any other personal data.

## Nothing appears when a shortcut is pressed

1. Confirm PoE is the active window.
2. Open Settings → Hotkeys and verify the binding.
3. Check for a KDE shortcut conflict in System Settings → Keyboard → Shortcuts.
4. Verify `ydotool` and its daemon.
5. Reinstall the KWin bridge.
6. Look for `[Shortcuts]`, `[GameWindow]`, or `[InputInjector]` messages.

Useful checks:

```bash
command -v ydotool
kpackagetool6 --type=KWin/Script --list | \
  grep awakened-poe-trade-native-focus
```

Do not use `--steal-shortcuts` routinely. It exists for controlled diagnostics
and can replace another application's global binding.

## Path of Exile is reported as XWayland

The native host supports only a Plasma Wayland desktop with Path of Exile
running as a native Wayland client. When KWin reports an XWayland PoE window,
the host logs:

```text
error [GameWindow] Path of Exile 1 is running through XWayland.
```

Gameplay shortcuts remain disabled for that window. Relaunch the game with its
native Wayland path enabled, then look for:

```text
info [GameWindow] Path of Exile 1 is running as a native Wayland client.
```

Do not bypass this check: XWayland has different focus, stacking, clipboard,
and pointer behavior and is outside the supported target.

## Overlay appears below the game

- Confirm the session is Wayland:

  ```bash
  echo "$XDG_SESSION_TYPE"
  echo "$WAYLAND_DISPLAY"
  ```

- Do not launch with `--no-layer-shell`.
- Confirm the KWin script is installed and enabled.
- Restart the application after changing the script.
- Check for `[Overlay] ... layerShell=true`.

The overlay deliberately uses no keyboard interactivity so PoE remains focused
and Plasma does not reveal the taskbar. Changing that behavior is not a valid
workaround.

## Overlay shows the taskbar or steals focus

Quit duplicate instances, confirm the native build is current, and ensure no
older test service is still running:

```bash
pgrep -af awakened-poe-trade-native
```

Only one native instance should be active. If a locally modified build requests
keyboard interactivity from layer-shell, restore the official build.

## Quick Price Check closes instantly

The KWin bridge supplies cursor movement while PoE owns the Wayland pointer.
Reinstall it:

```bash
./install-kwin-integration.sh
```

Then restart the application. Quick Price Check should remain visible while the
cursor is still. Release the configured modifier and move away to close it.
Keep the modifier held and move onto the popup to interact.

## Quick Price Check does not close

This also indicates missing compositor cursor reports. Verify the KWin service
interface while the application is running:

```bash
qdbus6 io.github.awakened_poe_trade.Native /GameWindow
```

The output should include `ReportActiveWindow` and `ReportCursorPosition`.
Reinstall the bridge and check the KWin journal if they are missing.

## Price checking says no item text was found

1. Hover an item in PoE before pressing the shortcut.
2. Confirm `wl-paste` exists:

   ```bash
   command -v wl-paste
   wl-paste --version
   ```

3. Confirm the PoE advanced-description key is readable from
   `production_Config.ini`.
4. Look for `[ClipboardPoller]` and `[InputInjector]` messages.
5. Test PoE's own Ctrl+C over the item, then inspect only whether clipboard
   text exists. Do not publish the item contents unnecessarily.

The native host uses a clean Ctrl+C with Proton-aware timing and retries once.

## F5/F9 or chat commands paste item contents

Current native releases type plain commands directly and do not use the
clipboard. If this occurs:

- confirm `--version` reports the latest native release;
- quit all older instances;
- confirm the command contains no `@last` placeholder;
- include sanitized `[Shortcuts]` and `[InputInjector]` logs in an issue.

Commands containing `@last` intentionally use the clipboard because they need
the previous player name.

## Hotkey field cannot capture Ctrl, Alt, or Shift

Quick and Locked Price Check use separate modifier buttons plus a single letter
field. Select Ctrl, Alt, or Shift, click the letter box, then press A–Z.
Backspace clears the letter. Other shortcuts retain the upstream hotkey editor.

If letter capture does not start, look for:

```text
[Shortcuts] Letter capture armed with 27 keys.
```

## Ctrl+wheel stash navigation is unavailable

The log explains whether permission was denied or no wheel device was found.
This feature requires read access to the relevant `/dev/input/event*` keyboard
and mouse devices.

Inspect permissions:

```bash
getfacl /dev/input/event* 2>/dev/null
```

Use your distribution's input or udev policy. Do not run the app as root and do
not make all input devices world-readable. Disabling stash-scroll in settings
silences the optional monitor without affecting price checking.

## Heist OCR fails

Install OCR data:

```bash
./install-ocr-data.sh
```

Verify:

```bash
ls "${XDG_CONFIG_HOME:-$HOME/.config}/awakened-poe-trade/apt-data/cv-ocr"
```

Expected files include `heist-lock.bmp`, `eng.traineddata`, and
`rus.traineddata`. KWin's `org.kde.KWin.ScreenShot2` interface must also be
available.

## Trade requests fail or leagues do not load

- Confirm general internet access and system time.
- Check whether pathofexile.com is reachable in a normal browser.
- Look for `[cors-proxy]` messages.
- Cloudflare or trade API rate limits can fail temporarily; do not repeatedly
  hammer the endpoint.
- Confirm the selected league and realm.

Never post session cookies or authenticated request headers.

## AppImage does not start

Show version information without opening the overlay:

```bash
APPIMAGE_EXTRACT_AND_RUN=1 \
  ./Awakened-PoE-Trade-Native-*.AppImage --version
```

Verify the published checksum:

```bash
sha256sum -c Awakened-PoE-Trade-Native-*.AppImage.sha256
```

Ensure the file is executable:

```bash
chmod +x Awakened-PoE-Trade-Native-*.AppImage
```

## Resetting configuration safely

Quit the app first. Move the configuration aside instead of deleting it:

```bash
config_dir="${XDG_CONFIG_HOME:-$HOME/.config}/awakened-poe-trade/apt-data"
mv "$config_dir/config.json" "$config_dir/config.json.backup"
```

Restart to generate defaults. Restore the backup if the reset does not help.
Also inspect `config.json.tmp`; a temporary settings session can take
precedence over the normal file.

## Update errors

- `404` means no release exists in the repository compiled into that build.
- Digest failures mean the download is rejected; do not bypass verification.
- Automatic replacement works only when running from a writable AppImage.
- Package-managed or local binaries receive notifications but are not
  overwritten.

Check the release page manually and compare the SHA-256 file.

## Opening a useful issue

Include:

- native version;
- distribution, Plasma, Qt, GPU, and Wayland/XWayland mode;
- PoE 1 or PoE 2 and Steam/Wine/Proton details;
- exact reproduction steps;
- expected and actual behavior;
- sanitized logs;
- whether the KWin bridge and `ydotool` daemon are active.

State any AI-assisted debugging or patch generation used, and verify the result
before submitting it.
