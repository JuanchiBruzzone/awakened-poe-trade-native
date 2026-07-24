#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
PACKAGE="$ROOT/native/kwin/awakened-poe-trade-native-focus"
PLUGIN_ID="awakened-poe-trade-native-focus"

if ! command -v kpackagetool6 >/dev/null 2>&1; then
  echo "kpackagetool6 is required (usually provided by kpackage)." >&2
  exit 1
fi
if ! command -v kwriteconfig6 >/dev/null 2>&1; then
  echo "kwriteconfig6 is required." >&2
  exit 1
fi

if kpackagetool6 --type=KWin/Script --list 2>/dev/null | grep -Fq "$PLUGIN_ID"; then
  kpackagetool6 --type=KWin/Script --upgrade "$PACKAGE"
else
  kpackagetool6 --type=KWin/Script --install "$PACKAGE"
fi

kwriteconfig6 --file kwinrc --group Plugins --key "${PLUGIN_ID}Enabled" true

if command -v qdbus6 >/dev/null 2>&1; then
  qdbus6 org.kde.KWin /KWin reconfigure || true
  qdbus6 org.kde.KWin /Scripting \
    org.kde.kwin.Scripting.unloadScript "$PLUGIN_ID" >/dev/null || true
  qdbus6 org.kde.KWin /Scripting \
    org.kde.kwin.Scripting.loadScript \
    "$HOME/.local/share/kwin/scripts/$PLUGIN_ID/contents/code/main.js" \
    "$PLUGIN_ID" >/dev/null || true
  qdbus6 org.kde.KWin /Scripting org.kde.kwin.Scripting.start || true
elif command -v qdbus >/dev/null 2>&1; then
  qdbus org.kde.KWin /KWin reconfigure || true
else
  echo "Installed and enabled. Log out and back in, or reconfigure KWin manually."
fi

echo "KWin focus bridge installed and enabled."
