#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
MODE="${1:---check}"
UPSTREAM_URL="${UPSTREAM_URL:-https://github.com/SnosMe/awakened-poe-trade.git}"
UPSTREAM_BRANCH="${UPSTREAM_BRANCH:-master}"

if [[ ! -d "$ROOT/.git" || ! -f "$ROOT/ipc/types.ts" || ! -d "$ROOT/native" ]]; then
  echo "error: run this script from the root of the native awakened-poe-trade fork" >&2
  exit 1
fi
if [[ "$MODE" != "--check" && "$MODE" != "--merge" ]]; then
  echo "usage: $0 [--check|--merge]" >&2
  exit 1
fi
if [[ -n "$(git -C "$ROOT" status --porcelain)" && "$MODE" == "--merge" ]]; then
  echo "error: commit or stash local changes before merging upstream" >&2
  exit 1
fi

REMOTE=""
while read -r name url; do
  if [[ "$url" == "$UPSTREAM_URL" || "$url" == "${UPSTREAM_URL%.git}" ]]; then
    REMOTE="$name"
    break
  fi
done < <(git -C "$ROOT" remote -v | awk '$3 == "(fetch)" { print $1, $2 }')

if [[ -z "$REMOTE" ]]; then
  REMOTE="upstream"
  if git -C "$ROOT" remote get-url "$REMOTE" >/dev/null 2>&1; then
    echo "error: remote '$REMOTE' exists but does not point to $UPSTREAM_URL" >&2
    exit 1
  fi
  git -C "$ROOT" remote add "$REMOTE" "$UPSTREAM_URL"
fi

git -C "$ROOT" fetch "$REMOTE" "$UPSTREAM_BRANCH"
TARGET="$REMOTE/$UPSTREAM_BRANCH"

echo "Upstream commits not in this branch:"
git -C "$ROOT" log --oneline --no-decorate "HEAD..$TARGET" || true
echo
echo "Backend and IPC files changed upstream:"
git -C "$ROOT" diff --name-status "HEAD..$TARGET" -- main/src ipc || true

if [[ "$MODE" == "--merge" ]]; then
  git -C "$ROOT" merge --no-edit "$TARGET"
  python "$ROOT/scripts/check-upstream-contract.py" "$ROOT"
else
  echo
  echo "Review the list above, then run '$0 --merge' from a clean branch."
fi
