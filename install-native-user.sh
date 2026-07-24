#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
PREFIX="${PREFIX:-$HOME/.local}"

"$ROOT/build-native.sh"
cmake --install "$ROOT/native/build" --prefix "$PREFIX"

printf '\nInstalled native host under: %s\n' "$PREFIX"
printf 'Launch: %s/bin/awakened-poe-trade-native\n' "$PREFIX"
printf 'Optional KDE focus bridge: %s/install-kwin-integration.sh\n' "$ROOT"
