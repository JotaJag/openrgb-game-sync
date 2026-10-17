#!/usr/bin/env python3
"""Configure the original Corsair DLL service for one explicitly selected prefix."""
from pathlib import Path
import argparse
import json
import shutil
import subprocess

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--prefix', type=Path, required=True, help='Prefijo Wine/Proton del juego, no la carpeta del juego')
    parser.add_argument('--proton', type=Path, help='Carpeta de Proton; requiere umu-run')
    parser.add_argument('--wine', default='wine', help='Ejecutable Wine usado por ese prefijo')
    parser.add_argument('--config-dir', type=Path, default=Path.home()/'.config/OpenRGB')
    args = parser.parse_args()
    if not shutil.which('pgrep'):
        parser.error('Se necesita pgrep (procps).')
    running = subprocess.run(['pgrep', '-ix', 'openrgb'], stdout=subprocess.DEVNULL)
    if running.returncode != 1:
        parser.error('Cierra OpenRGB antes de cambiar los servicios.')
    prefix = args.prefix.expanduser().resolve()
    if not (prefix/'system.reg').is_file():
        parser.error('El prefijo debe contener system.reg.')
    config = args.config_dir.expanduser().resolve()/'plugins/settings/GameSyncAutomation.json'
    settings = json.loads(config.read_text())
    runtime = Path(settings['script']).parent
    if not (runtime/'build/cue-pipe-host.exe').is_file():
        parser.error('Falta el servicio compilado. Instala primero el paquete completo.')
    service = {'prefix': str(prefix), 'status': str(Path.home()/'.local/share/openrgb-game-sync/pipe-service/game.json')}
    if args.proton:
        proton = args.proton.expanduser().resolve()
        if not (proton/'proton').is_file() or not shutil.which('umu-run'):
            parser.error('Proton debe contener el archivo proton y umu-run debe estar instalado.')
        service['proton'] = str(proton)
    else:
        wine = shutil.which(args.wine)
        if not wine:
            parser.error('No se encuentra Wine; usa --wine con el ejecutable correcto.')
        service['wine'] = wine
    backup = config.with_name('GameSyncAutomation.before-service.json')
    if backup.exists():
        parser.error(f'Ya existe {backup}; consérvalo o renómbralo antes de reconfigurar.')
    shutil.copy2(config, backup)
    settings.update(enabled=False, pipe_services=[service])
    temporary = config.with_suffix('.tmp')
    temporary.write_text(json.dumps(settings, indent=2)+'\n')
    temporary.chmod(0o600)
    temporary.replace(config)
    print('Servicio configurado. Abre OpenRGB, activa Game Sync e iCUE y después abre el juego.')
    print('No se ha modificado ninguna DLL ni iniciado el juego. La preparación automática sigue desactivada.')

if __name__ == '__main__':
    main()
