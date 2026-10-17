# Pinned dependencies

Run `bash scripts/bootstrap.sh` from a clone named `openrgb-game-sync`.
The build currently expects these sibling directories:

- `../openrgb-sequence-fix`: OpenRGB Effects Plugin at
  `0e0f1b4708c302bad4270ab62349b2efe5dcc439`, including its recursive submodules.
  OpenRGB itself is pinned by that tree to
  `81bbe18a84c2e507006f19dd252e397e40a56bfe`.
  Apply `effects-game-sync.patch`, then copy `OpenRGBEffectsPlugin.json` and
  `effects-tests/` (as `tests/`) into that checkout.
- `../openrgb-cue-sdk`: https://github.com/CorsairOfficial/cue-sdk at
  `69cc6d71b14b1d33c231a29be7c5b33cc3ce7f0b` (SDK 3 headers).
- SDK 4 ABI headers already in `bridge/sdk4/` originate from official Corsair
  commit `890bc3e27e651f19ff73eff87bfa6497bb93dac5`. Preserve their upstream terms.

The Effects patch fixes Sequence interpolation and adds the priority lease
shared with Game Sync. Installing an unpatched Effects binary removes that
coordination. Upstream dependencies retain their respective licenses.

`python3 scripts/setup-toolchain.py` downloads pinned MinGW packages and checks
their SHA-256 before extracting them into the ignored `toolchain/` directory.
It requires tar with zstd support. If the original mirror removes these exact
packages, use the same files from an archive; do not bypass the checksum check.
No game DLLs, local Wine prefixes or binary SDK copies are distributed in Git.
