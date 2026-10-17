#!/usr/bin/env bash
set -Eeuo pipefail
root="$(cd -- "$(dirname -- "$0")" && pwd)"
if [[ -f "$root/SHA256.json" ]]; then
  exec python3 "$root/install.py" "$@"
fi
version=$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["VersionStr"])' "$root/plugin.json")
archive="$root/dist/openrgb-game-sync-$version-linux-x86_64.tar.gz"
if [[ ! -f "$archive" ]]; then
  echo 'Descarga el paquete de Releases o compílalo siguiendo docs/DEVELOPMENT.md.' >&2
  exit 1
fi
temp=$(mktemp -d)
trap 'rm -rf -- "$temp"' EXIT
tar -xzf "$archive" -C "$temp"
python3 "$temp/openrgb-game-sync-$version/install.py" "$@"
