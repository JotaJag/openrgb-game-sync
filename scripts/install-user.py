#!/usr/bin/env python3
"""Install/uninstall a release without root, launcher edits, or game DLL changes."""
from pathlib import Path
import argparse
import datetime
import hashlib
import json
import os
import platform
import shutil
import subprocess
import sys

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def write_json(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(path.name+'.tmp')
    temporary.write_text(json.dumps(value, indent=2)+'\n')
    temporary.chmod(0o600)
    temporary.replace(path)

def stopped():
    if shutil.which('pgrep'):
        result = subprocess.run(['pgrep', '-ix', 'openrgb'], stdout=subprocess.DEVNULL)
        if result.returncode == 0:
            raise RuntimeError('Cierra OpenRGB antes de instalar o desinstalar.')
        if result.returncode != 1:
            raise RuntimeError('No se ha podido comprobar si OpenRGB está cerrado.')
    else:
        raise RuntimeError('Se necesita pgrep (procps) para comprobar que OpenRGB está cerrado.')

def restore(manifest, journal):
    for item in journal['files']:
        path = Path(item['path'])
        expected = item.get('installed_sha256')
        if path.exists() and (expected is None or digest(path) != expected):
            raise RuntimeError(f'El archivo cambió después de instalarlo: {path}. Consérvalo y revisa {manifest}.')
    for item in reversed(journal['files']):
        path = Path(item['path'])
        if path.exists():
            path.unlink()
        if item['backup']:
            path.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(item['backup'], path)
    manifest.rename(manifest.with_name('uninstalled-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S-%f')+'.json'))

def install(payload, config, data):
    hashes = json.loads((payload/'SHA256.json').read_text())
    for relative, expected in hashes.items():
        path = payload/relative
        if not path.resolve().is_relative_to(payload.resolve()) or not path.is_file() or digest(path) != expected:
            raise RuntimeError(f'Paquete incompleto o alterado: {relative}')
    for name in ['libOpenRGBGameSyncPlugin.so', 'libOpenRGBEffectsPlugin.so']:
        library = payload/'plugins'/name
        result = subprocess.run(['ldd', str(library)], text=True, capture_output=True)
        if result.returncode or 'not found' in result.stdout+result.stderr:
            raise RuntimeError(f'Dependencias incompatibles para {name}:\n{result.stdout}{result.stderr}')
    manifest = data/'installation.json'
    if manifest.exists():
        raise RuntimeError('Ya hay una instalación gestionada. Desinstálala antes de instalar otra versión.')
    stamp = datetime.datetime.now().strftime('%Y%m%d-%H%M%S-%f')
    runtime = data/'runtime'/stamp
    shutil.copytree(payload/'runtime', runtime)
    plugins = config/'plugins'
    settings = plugins/'settings/GameSyncAutomation.json'
    # Keep game exclusions and unrelated fields while leaving all automation paused.
    cfg = json.loads(settings.read_text()) if settings.exists() else {}
    cfg.update(enabled=False, rest=False, pipe_services=[], script=str(runtime/'auto_prepare.py'),
               status=str(Path.home()/'.local/share/openrgb-game-sync/automation/status.json'))
    new_files = {plugins/name: (payload/'plugins'/name).read_bytes()
                 for name in ['libOpenRGBGameSyncPlugin.so', 'libOpenRGBEffectsPlugin.so']}
    new_files[settings] = (json.dumps(cfg, indent=2)+'\n').encode()
    # Move duplicate Effects/Game Sync binaries out of the plugin directory.
    touched = set(new_files)
    if plugins.exists():
        touched.update(p for p in plugins.glob('*.so') if 'effects' in p.name.lower() or 'gamesync' in p.name.lower())
    backup = data/'backups'/stamp
    backup.mkdir(parents=True)
    journal = {'runtime': str(runtime), 'files': []}
    for index, path in enumerate(sorted(touched)):
        original = None
        if path.exists():
            original = backup/str(index)
            shutil.copy2(path, original)
        journal['files'].append({'path': str(path), 'backup': str(original) if original else None,
                                 'installed_sha256': hashlib.sha256(new_files[path]).hexdigest() if path in new_files else None})
    write_json(manifest, journal)
    try:
        for path in touched:
            if path.exists():
                path.unlink()
        for path, content in new_files.items():
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(content)
            path.chmod(0o600 if path == settings else 0o644)
    except Exception:
        # Installation has not started OpenRGB: restore originals even after a partial write.
        for item in reversed(journal['files']):
            path = Path(item['path'])
            if path.exists():
                path.unlink()
            if item['backup']:
                shutil.copy2(item['backup'], path)
        manifest.unlink()
        raise
    print(f'Instalado en {plugins}. Abre OpenRGB y activa Game Sync en Plugins.')
    print('Preparación automática y servicios de juegos desactivados. Consulta INSTALL.md para configurar un juego.')
    print(f'Copias de seguridad: {backup}')

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--uninstall', action='store_true')
    parser.add_argument('--config-dir', type=Path, default=Path.home()/'.config/OpenRGB', help='Carpeta de configuración usada por OpenRGB')
    args = parser.parse_args()
    if os.geteuid() == 0:
        parser.error('Ejecuta el instalador con tu usuario, sin sudo.')
    if sys.platform != 'linux' or platform.machine().lower() not in ('x86_64', 'amd64'):
        parser.error('Este paquete requiere Linux x86_64 y OpenRGB 1.0 con Qt6/API 5.')
    stopped()
    config = args.config_dir.expanduser().resolve()
    data = Path.home()/'.local/share/openrgb-game-sync/installer'
    manifest = data/'installation.json'
    if args.uninstall:
        if not manifest.exists():
            parser.error('No hay una instalación gestionada para desinstalar.')
        restore(manifest, json.loads(manifest.read_text()))
        print('Plugins anteriores restaurados. Las copias y el runtime se conservan.')
    else:
        install(Path(__file__).resolve().parent, config, data)

if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError, RuntimeError, subprocess.SubprocessError) as error:
        raise SystemExit(str(error))
