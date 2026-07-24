#!/usr/bin/env bash
set -euo pipefail

if [[ -z "${REAL_QMAKE:-}" || -z "${QT_PLUGIN_STAGING:-}" ]]; then
  echo "error: qmake AppImage wrapper requires REAL_QMAKE and QT_PLUGIN_STAGING" >&2
  exit 1
fi

if [[ "${1:-}" == "-query" && "${2:-}" == "QT_INSTALL_PLUGINS" ]]; then
  printf '%s\n' "$QT_PLUGIN_STAGING"
elif [[ "${1:-}" == "-query" && $# -eq 1 ]]; then
  "$REAL_QMAKE" -query | sed \
    "s|^QT_INSTALL_PLUGINS:.*$|QT_INSTALL_PLUGINS:$QT_PLUGIN_STAGING|"
else
  exec "$REAL_QMAKE" "$@"
fi
