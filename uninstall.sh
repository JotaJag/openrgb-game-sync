#!/usr/bin/env bash
set -Eeuo pipefail
root="$(cd -- "$(dirname -- "$0")" && pwd)"
if [[ -f "$root/SHA256.json" ]]; then
  exec python3 "$root/install.py" --uninstall "$@"
fi
exec python3 "$root/scripts/install-user.py" --uninstall "$@"
