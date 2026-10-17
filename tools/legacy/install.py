"""Install the paired local builds and prepare a Wine prefix, preserving originals."""
from pathlib import Path
import argparse
import datetime
import hashlib
import json
import os
import shutil
from bridge_registration import registered_system_text

root = Path(__file__).resolve().parent
parser = argparse.ArgumentParser()
parser.add_argument("--prefix", type=Path, default=Path.home() / "Games/battlenet")
parser.add_argument("--icue-game", type=Path, help="Carpeta de un juego con CUESDK.x64_2015.dll")
args = parser.parse_args()
prefix = args.prefix.expanduser().resolve()
system32 = prefix / "drive_c/windows/system32"
if not system32.is_dir():
    raise SystemExit("No existe el directorio system32 en ese entorno Wine")
plugins = Path.home() / ".config/OpenRGB/plugins"
plugins.mkdir(parents=True, exist_ok=True)
backup = Path.home() / ".local/share/openrgb-backups" / ("game-sync-" + datetime.datetime.now().strftime("%Y%m%d-%H%M%S"))
backup.mkdir(parents=True)
manifest = []
files = [
    (root / "build/libOpenRGBGameSyncPlugin.so", plugins / "libOpenRGBGameSyncPlugin.so"),
    (root.parent / "openrgb-sequence-fix/build/libOpenRGBEffectsPlugin.so", plugins / "OpenRGBEffectsPlugin_1.0_Linux_amd64_0e0f1b4.so"),
    (root / "build/RzChromaSDK64.dll", system32 / "RzChromaSDK64.dll"),
    (root / "build/CUESDK.x64_2015.dll", system32 / "CUESDK.x64_2015.dll"),
    (root / "build/LogitechLed.dll", system32 / "LogitechLed.dll"),
    (root / "build/LogitechLed.dll", system32 / "LogitechLedEnginesWrapper.dll"),
]
if args.icue_game:
    target = args.icue_game.expanduser().resolve() / "CUESDK.x64_2015.dll"
    if not target.is_file():
        raise SystemExit("No se ha encontrado una biblioteca CUESDK.x64_2015.dll para guardar y sustituir")
    files.append((root / "build/CUESDK.x64_2015.dll", target))
registry = prefix / "system.reg"
registry_text = registered_system_text(registry.read_text())
# Do not edit a registry cached by a live Wine process.
for proc in Path("/proc").iterdir():
    if not proc.name.isdigit(): continue
    try:
        env = dict(part.split(b"=",1) for part in (proc / "environ").read_bytes().split(b"\0") if b"=" in part)
        running_prefix = env.get(b"WINEPREFIX")
        if running_prefix and Path(os.fsdecode(running_prefix)).resolve() == prefix:
            raise SystemExit("El entorno Wine sigue en uso. Detén Battle.net y sus juegos antes de instalar")
    except (OSError, ValueError): pass
for source, target in files:
    if not source.is_file() or target.is_symlink():
        raise SystemExit(f"No se puede instalar con seguridad: {target}")
for number, (source, target) in enumerate(files):
    if target.is_symlink():
        raise SystemExit(f"No se reemplaza un enlace simbólico: {target}")
    previous = backup / (str(number) + "-" + target.name) if target.exists() else None
    if previous:
        shutil.copy2(target, previous)
    temporary = target.with_name(target.name + ".new")
    shutil.copy2(source, temporary)
    temporary.chmod(0o644)
    os.replace(temporary, target)
    digest = hashlib.sha256(target.read_bytes()).hexdigest()
    assert digest == hashlib.sha256(source.read_bytes()).hexdigest()
    manifest.append({"target": str(target), "previous": str(previous) if previous else None, "sha256": digest})
    print("Instalado:", target)
previous = backup / "system.reg"
shutil.copy2(registry,previous)
temporary = registry.with_name("system.reg.new")
temporary.write_text(registry_text)
shutil.copymode(registry,temporary)
os.replace(temporary,registry)
manifest.append({"target":str(registry),"previous":str(previous),"sha256":hashlib.sha256(registry.read_bytes()).hexdigest()})
print("Registrado: Logitech LED SDK de 64 bits en el entorno Wine")
(backup / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
print("Copia y manifiesto:", backup)
