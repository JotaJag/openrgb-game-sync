#!/usr/bin/env python3
"""Build a user-installable Linux x86_64 archive from compiled local artifacts."""
from pathlib import Path
import argparse
import hashlib
import json
import re
import shutil
import tarfile
import tempfile

ROOT = Path(__file__).resolve().parents[1]

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--effects-library', type=Path, default=ROOT.parent/'openrgb-sequence-fix/build/libOpenRGBEffectsPlugin.so')
    args = parser.parse_args()
    artifacts = {
        'libOpenRGBGameSyncPlugin.so': ROOT/'build/libOpenRGBGameSyncPlugin.so',
        'libOpenRGBEffectsPlugin.so': args.effects_library,
    }
    host = ROOT/'build/cue-pipe-host.exe'
    adapter_names = ['RzChromaSDK64.dll', 'RzChromaSDK.dll', 'CUESDK.x64_2015.dll',
                     'CUESDK_2015.dll', 'iCUESDK.x64.dll', 'LogitechLed.dll', 'LogitechLed32.dll']
    adapters = [ROOT/'build'/name for name in adapter_names]
    for path in [*artifacts.values(), host, *adapters]:
        if not path.is_file():
            parser.error(f'Missing build artifact: {path}')
    for path in artifacts.values():
        if path.read_bytes()[:4] != b'\x7fELF':
            parser.error(f'Not an ELF library: {path}')
    version = json.loads((ROOT/'plugin.json').read_text())['VersionStr']
    if not re.fullmatch(r'[0-9]+\.[0-9]+\.[0-9]+(?:-[A-Za-z0-9.]+)?', version):
        parser.error('Invalid package version')
    dist = ROOT/'dist'
    dist.mkdir(exist_ok=True)
    archive = dist/f'openrgb-game-sync-{version}-linux-x86_64.tar.gz'
    with tempfile.TemporaryDirectory() as temp:
        payload = Path(temp)/f'openrgb-game-sync-{version}'
        runtime = payload/'runtime'
        (runtime/'build').mkdir(parents=True)
        (payload/'plugins').mkdir()
        for name, path in artifacts.items():
            shutil.copy2(path, payload/'plugins'/name)
        shutil.copy2(host, runtime/'build'/host.name)
        for path in adapters:
            shutil.copy2(path, runtime/'build'/path.name)
        for name in ['automation', 'pipe_service', 'bridge']:
            shutil.copytree(ROOT/name, runtime/name, ignore=shutil.ignore_patterns('__pycache__', '*.pyc'))
        for name in ['auto_prepare.py', 'chroma_rest.py', 'bridge_registration.py', 'auto_launch.sh']:
            shutil.copy2(ROOT/name, runtime/name)
        shutil.copy2(ROOT/'scripts/install-user.py', payload/'install.py')
        shutil.copy2(ROOT/'scripts/configure-game.py', payload/'configure-game.py')
        shutil.copy2(ROOT/'scripts/configure-rest.py', payload/'configure-rest.py')
        for name in ['install.sh', 'uninstall.sh']:
            shutil.copy2(ROOT/name, payload/name)
        shutil.copy2(ROOT/'docs/INSTALL.md', payload/'INSTALL.md')
        if (ROOT/'LICENSE').exists():
            shutil.copy2(ROOT/'LICENSE', payload/'LICENSE')
        manifest = {str(p.relative_to(payload)): hashlib.sha256(p.read_bytes()).hexdigest()
                    for p in payload.rglob('*') if p.is_file()}
        (payload/'SHA256.json').write_text(json.dumps(manifest, indent=2)+'\n')
        with tarfile.open(archive, 'w:gz') as tar:
            tar.add(payload, arcname=payload.name)
    digest = hashlib.sha256(archive.read_bytes()).hexdigest()
    archive.with_suffix(archive.suffix+'.sha256').write_text(f'{digest}  {archive.name}\n')
    print(archive)

if __name__ == '__main__':
    main()
