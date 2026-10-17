#!/usr/bin/env python3
"""Enable Chroma REST without enabling automatic game library preparation."""
import argparse
import datetime
import json
from pathlib import Path
import shutil
import subprocess

def configure(config, enabled):
    settings = json.loads(config.read_text())
    runtime = Path(settings['script']).parent
    if enabled and not (runtime/'chroma_rest.py').is_file():
        raise ValueError('Falta el runtime REST; instala el paquete completo.')
    backup = config.with_name('GameSyncAutomation.before-rest-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S-%f')+'.json')
    shutil.copy2(config, backup)
    settings.update(enabled=False, rest=enabled)
    temporary = config.with_suffix('.tmp')
    temporary.write_text(json.dumps(settings, indent=2)+'\n')
    temporary.chmod(0o600)
    temporary.replace(config)
    return backup

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--disable', action='store_true')
    parser.add_argument('--config-dir', type=Path, default=Path.home()/'.config/OpenRGB')
    args = parser.parse_args()
    if not shutil.which('pgrep') or subprocess.run(['pgrep','-ix','openrgb'], stdout=subprocess.DEVNULL).returncode != 1:
        parser.error('Cierra OpenRGB antes de configurar REST; se necesita pgrep (procps).')
    try:
        backup = configure(args.config_dir.expanduser().resolve()/'plugins/settings/GameSyncAutomation.json', not args.disable)
    except (OSError, ValueError, KeyError) as error:
        parser.error(str(error))
    print('REST desactivado.' if args.disable else 'REST configurado. Abre OpenRGB y activa Game Sync y Chroma.')
    print('Preparación automática de bibliotecas desactivada. Copia:', backup)

if __name__ == '__main__':
    main()
