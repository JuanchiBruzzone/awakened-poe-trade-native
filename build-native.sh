#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
BUILD_TYPE="${BUILD_TYPE:-Release}"
JOBS="${JOBS:-$(nproc)}"
APP_VERSION="${APP_VERSION:-}"
UPDATE_REPOSITORY="${UPDATE_REPOSITORY:-}"

if [[ ! -f "$ROOT/renderer/package.json" ]]; then
  echo "error: run this script from a patched awakened-poe-trade checkout" >&2
  exit 1
fi

if [[ -z "$APP_VERSION" && -f "$ROOT/native/VERSION" ]]; then
  APP_VERSION="$(tr -d '[:space:]' < "$ROOT/native/VERSION")"
fi
if [[ -z "$APP_VERSION" && -f "$ROOT/main/package.json" ]]; then
  APP_VERSION="$(node -p "require('$ROOT/main/package.json').version")"
fi
if [[ -z "$APP_VERSION" ]]; then
  echo "error: could not determine the upstream application version" >&2
  exit 1
fi

if [[ -z "$UPDATE_REPOSITORY" ]]; then
  ORIGIN_URL="$(git -C "$ROOT" config --get remote.origin.url 2>/dev/null || true)"
  if [[ "$ORIGIN_URL" =~ github\.com[:/]([^/]+/[^/]+)(\.git)?$ ]]; then
    UPDATE_REPOSITORY="${BASH_REMATCH[1]}"
    UPDATE_REPOSITORY="${UPDATE_REPOSITORY%.git}"
  fi
fi

if [[ "${SKIP_RENDERER_BUILD:-0}" != "1" ]]; then
  echo "[1/3] Installing renderer dependencies"
  cd "$ROOT/renderer"
  if [[ -f yarn.lock ]] && command -v yarn >/dev/null 2>&1; then
    yarn install --frozen-lockfile
    echo "[2/3] Building the existing Vue renderer"
    yarn make-index-files
    yarn build
  elif [[ -f package-lock.json ]] && command -v npm >/dev/null 2>&1; then
    npm ci
    echo "[2/3] Building the existing Vue renderer"
    npm run make-index-files
    npm run build
  else
    echo "error: no supported package manager matching the renderer lockfile was found" >&2
    exit 1
  fi
else
  echo "[1/3] Renderer build skipped"
  echo "[2/3] Using existing renderer/dist"
fi

if [[ ! -f "$ROOT/renderer/dist/index.html" ]]; then
  echo "error: renderer/dist/index.html was not produced" >&2
  exit 1
fi

echo "[3/3] Building the Qt/KDE native host"
cmake -S "$ROOT/native" -B "$ROOT/native/build" -G Ninja \
  -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
  -DAPT_NATIVE_VERSION="$APP_VERSION" \
  -DAPT_UPDATE_REPOSITORY="$UPDATE_REPOSITORY" \
  -DAPT_RENDERER_DEFAULT_PATH="$ROOT/renderer/dist"
cmake --build "$ROOT/native/build" --parallel "$JOBS"

echo
echo "Built: $ROOT/native/build/awakened-poe-trade-native"
echo "Run:   $ROOT/native/build/awakened-poe-trade-native --renderer '$ROOT/renderer/dist'"
