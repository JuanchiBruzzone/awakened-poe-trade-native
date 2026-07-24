#!/usr/bin/env bash
set -euo pipefail

OCR_URL="https://github.com/SnosMe/awakened-poe-trade/releases/download/v3.20.10007/cv-ocr.zip"
OCR_SHA256="56411068510acb1c666c7a6a74bfa6d1d27ebff2b4f305047c96ef9c1e7fc330"
CONFIG_BASE="${XDG_CONFIG_HOME:-$HOME/.config}"
DESTINATION="$CONFIG_BASE/awakened-poe-trade/apt-data/cv-ocr"
TEMP_DIRECTORY="$(mktemp -d)"
trap 'rm -rf -- "$TEMP_DIRECTORY"' EXIT

for tool in curl sha256sum unzip; do
  if ! command -v "$tool" >/dev/null 2>&1; then
    printf 'error: %s is required\n' "$tool" >&2
    exit 1
  fi
done

ARCHIVE="$TEMP_DIRECTORY/cv-ocr.zip"
curl -fL --retry 3 --output "$ARCHIVE" "$OCR_URL"
printf '%s  %s\n' "$OCR_SHA256" "$ARCHIVE" | sha256sum --check --status
unzip -q "$ARCHIVE" -d "$TEMP_DIRECTORY/extracted"

install -d "$DESTINATION"
for asset in heist-lock.bmp eng.traineddata rus.traineddata; do
  install -m 0644 "$TEMP_DIRECTORY/extracted/cv-ocr/$asset" "$DESTINATION/$asset"
done

printf 'Installed native OCR data in: %s\n' "$DESTINATION"
