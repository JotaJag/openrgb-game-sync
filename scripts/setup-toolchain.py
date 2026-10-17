#!/usr/bin/env python3
"""Download the exact MinGW headers/CRT used by the original prototype."""
from pathlib import Path
import hashlib
import json
import subprocess
import urllib.request

root = Path(__file__).resolve().parents[1]
checksums = json.loads((root/'dependencies/toolchain-sha256.json').read_text())
target = root/'toolchain'
(target/'packages').mkdir(parents=True, exist_ok=True)
for url in (root/'dependencies/toolchain-urls.txt').read_text().splitlines():
    name = url.rsplit('/', 1)[1]
    path = target/'packages'/name
    if not path.exists():
        print('Downloading', name, flush=True)
        urllib.request.urlretrieve(url, path)
    if hashlib.sha256(path.read_bytes()).hexdigest() != checksums[name]:
        raise SystemExit(f'Checksum mismatch: {path}. Nothing extracted from this file.')
    subprocess.run(['tar', '--zstd', '-xf', str(path), '-C', str(target), 'usr'], check=True)
print('Toolchain ready.')
