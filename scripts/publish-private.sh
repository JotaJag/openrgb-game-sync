#!/usr/bin/env bash
set -Eeuo pipefail
root="$(cd -- "$(dirname -- "$0")/.." && pwd)"
cd "$root"
gh auth status
owner=$(gh api user --jq .login)
repo="$owner/openrgb-game-sync"
if git remote get-url origin >/dev/null 2>&1; then
  echo 'Ya existe un remoto origin. No se publicará automáticamente en otro destino.' >&2
  exit 1
fi
if gh repo view "$repo" >/dev/null 2>&1; then
  echo "Ya existe $repo. Revisa el destino antes de publicar; no se sobrescribirá." >&2
  exit 1
fi
gh repo create "$repo" --private --description 'OpenRGB Game Sync: game lighting for Linux Wine/Proton' \
  --source "$root" --remote origin --push
visibility=$(gh repo view "$repo" --json visibility --jq .visibility)
[[ "$visibility" == PRIVATE ]] || { echo 'No se ha confirmado la visibilidad privada.' >&2; exit 1; }
gh release create v0.5.0 \
  dist/openrgb-game-sync-0.5.0-linux-x86_64.tar.gz \
  dist/openrgb-game-sync-0.5.0-linux-x86_64.tar.gz.sha256 \
  --repo "$repo" --target main --prerelease --title '0.5.0 — Linux x86_64 preview' \
  --notes-file docs/RELEASE-0.5.0.md
gh repo view "$repo" --json url,visibility --jq '"Repositorio: " + .url + " | " + .visibility'
