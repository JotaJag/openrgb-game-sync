#!/usr/bin/env bash
set -Eeuo pipefail
root="$(cd -- "$(dirname -- "$0")/.." && pwd)"
parent="$(dirname -- "$root")"
effects="$parent/openrgb-sequence-fix"
sdk="$parent/openrgb-cue-sdk"
if [[ -e "$effects" || -e "$sdk" ]]; then
  echo 'Las carpetas de dependencias ya existen. No se sobrescribirán; consulta dependencies/README.md.' >&2
  exit 1
fi
git clone https://gitlab.com/OpenRGBDevelopers/OpenRGBEffectsPlugin.git "$effects"
git -C "$effects" checkout --detach 0e0f1b4708c302bad4270ab62349b2efe5dcc439
git -C "$effects" submodule update --init --recursive
git -C "$effects" apply --check "$root/dependencies/effects-game-sync.patch"
git -C "$effects" apply "$root/dependencies/effects-game-sync.patch"
cp "$root/dependencies/OpenRGBEffectsPlugin.json" "$effects/"
cp -R "$root/dependencies/effects-tests" "$effects/tests"
git clone https://github.com/CorsairOfficial/cue-sdk.git "$sdk"
git -C "$sdk" checkout --detach 69cc6d71b14b1d33c231a29be7c5b33cc3ce7f0b
echo 'Fuentes de las dependencias preparadas. No se ha instalado ni activado ningún plugin.'
