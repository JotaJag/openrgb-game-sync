from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path
import json
import os
import re


def vdf(text):
    tokens = re.findall(r'//[^\n]*|"(?:\\.|[^"\\])*"|[{}]|[^\s{}]+', text)
    tokens = [t for t in tokens if not t.startswith('//')]
    index = 0
    def value():
        nonlocal index
        token = tokens[index]; index += 1
        if token.startswith('"'):
            return re.sub(r'\\(["\\])', r'\1', token[1:-1])
        return token
    def block():
        nonlocal index
        result = {}
        while index < len(tokens) and tokens[index] != '}':
            key = value()
            if index >= len(tokens): break
            if tokens[index] == '{':
                index += 1; result[key] = block()
            else: result[key] = value()
        if index < len(tokens): index += 1
        return result
    return block()


def read_json(path):
    try: return json.loads(path.read_text())
    except (OSError, ValueError): return {}


def read_yaml(path):
    try:
        import yaml
        return yaml.safe_load(path.read_text()) or {}
    except ImportError: return {}
    except (OSError, ValueError, yaml.YAMLError): return {}


def windows_path(prefix, text):
    if not isinstance(text, str): return None
    text = text.replace('\\\\', '\\')
    match = re.match(r'^([A-Za-z]):[\\/](.*)$', text)
    if not match: return Path(text).expanduser() if text.startswith('/') else None
    drive, tail = match.groups()
    base = prefix / 'dosdevices' / (drive.lower() + ':')
    if not base.exists():
        if drive.lower() == 'c': base = prefix / 'drive_c'
        elif drive.lower() == 'z': base = Path('/')
        else: return None
    return (base / tail.replace('\\', '/')).resolve()


@dataclass
class Target:
    prefix: Path
    platforms: set = field(default_factory=set)
    games: set = field(default_factory=set)


@dataclass
class Inventory:
    targets: dict = field(default_factory=dict)
    tools: set = field(default_factory=set)
    configs: dict = field(default_factory=dict)
    libraries: set = field(default_factory=set)

    def add(self, prefix, platform, games=()):
        prefix = Path(prefix).expanduser().resolve()
        target = self.targets.setdefault(str(prefix), Target(prefix))
        target.platforms.add(platform)
        target.games.update(Path(p).expanduser().resolve() for p in games if p and Path(p).is_dir())
        return target


def registry_games(prefix):
    found = set()
    try:
        text = (prefix / 'system.reg').read_text(errors='replace')
        for raw in re.findall(r'^"(?:InstallLocation|InstallPath|InstallDir)"="(.*)"$', text, re.M):
            path = windows_path(prefix, raw)
            if path and path.is_dir() and path != prefix / 'drive_c': found.add(path)
    except OSError: pass
    manifests = prefix / 'drive_c/ProgramData/Epic/EpicGamesLauncher/Data/Manifests'
    for path in manifests.glob('*.item'):
        install = read_json(path).get('InstallLocation')
        game = windows_path(prefix, install)
        if game and game.is_dir(): found.add(game)
    return found


def discover(home=None):
    home = Path(home or Path.home()).resolve()
    result = Inventory()
    steam_roots = [home / '.local/share/Steam', home / '.steam/steam', home / '.var/app/com.valvesoftware.Steam/data/Steam']
    for steam in steam_roots:
        if not steam.is_dir(): continue
        result.libraries.add(steam.resolve())
        for file in (steam / 'steamapps/libraryfolders.vdf', steam / 'config/libraryfolders.vdf'):
            try: folders = vdf(file.read_text()).get('libraryfolders', {})
            except (OSError, ValueError, IndexError): continue
            for item in folders.values():
                path = item.get('path') if isinstance(item, dict) else item
                if isinstance(path, str) and Path(path).is_dir(): result.libraries.add(Path(path).resolve())
        for tool in (steam / 'compatibilitytools.d').glob('*/proton'):
            result.tools.add(tool.parent.resolve())
    for library in result.libraries:
        apps = library / 'steamapps'
        for manifest in apps.glob('appmanifest_*.acf'):
            try: state = vdf(manifest.read_text()).get('AppState', {})
            except (OSError, ValueError, IndexError): continue
            appid, directory = state.get('appid'), state.get('installdir')
            if not isinstance(appid, str) or not appid.isdigit() or not isinstance(directory, str): continue
            game = (apps / 'common' / directory).resolve()
            if not game.is_relative_to((apps / 'common').resolve()): continue
            result.add(apps / 'compatdata' / appid / 'pfx', 'Steam', [game])
        for prefix in (apps / 'compatdata').glob('*/pfx'):
            if (prefix / 'system.reg').is_file(): result.add(prefix, 'Steam')
        for tool in (apps / 'common').glob('*/proton'):
            result.tools.add(tool.parent.resolve())
    lutris_roots = [home / '.local/share/lutris', home / '.config/lutris', home / '.var/app/net.lutris.Lutris/data/lutris']
    for base in lutris_roots:
        if not base.is_dir(): continue
        result.configs.setdefault('Lutris', []).append(base / 'system.yml')
        for file in (base / 'games').glob('*.yml'):
            settings = read_yaml(file); game = settings.get('game', {})
            prefix = game.get('prefix')
            if not isinstance(prefix, str) or '$' in prefix: continue
            target = result.add(prefix, 'Lutris')
            executable = game.get('exe')
            if isinstance(executable, str):
                path = Path(executable)
                if not path.is_absolute(): path = target.prefix / path
                if path.parent.is_dir(): target.games.add(path.parent.resolve())
            for name in ('Battle.net', 'Epic', 'Ubisoft', 'EA', 'GOG'):
                if name.lower() in str(settings.get('name', '')).lower(): target.platforms.add(name)
    heroic_roots = [home / '.config/heroic', home / '.var/app/com.heroicgameslauncher.hgl/config/heroic']
    for base in heroic_roots:
        if not base.is_dir(): continue
        result.configs.setdefault('Heroic', []).append(base / 'config.json')
        default = read_json(base / 'config.json').get('defaultSettings', {})
        prefix = default.get('winePrefix')
        if isinstance(prefix, str): result.add(prefix, 'Heroic')
        documents = list((base / 'GamesConfig').glob('*.json')) + list((base / 'store_cache').glob('*install_info.json'))
        documents += [base / 'legendaryConfig/legendary/installed.json']
        installed = set()
        def walk(value):
            if isinstance(value, dict):
                for key in ('install_path', 'installPath', 'installLocation', 'folder_name'):
                    p = value.get(key)
                    if isinstance(p, str) and Path(p).is_dir(): installed.add(Path(p).resolve())
                prefix = value.get('winePrefix')
                if isinstance(prefix, str): result.add(prefix, 'Heroic')
                for child in value.values(): walk(child)
            elif isinstance(value, list):
                for child in value: walk(child)
        for file in documents: walk(read_json(file))
        for target in result.targets.values():
            if 'Heroic' in target.platforms: target.games.update(installed)
        for tool in (base / 'tools/proton').glob('*/proton'): result.tools.add(tool.parent.resolve())
    bottle_roots = [home / '.local/share/bottles/bottles', home / '.var/app/com.usebottles.bottles/data/bottles/bottles']
    for base in bottle_roots:
        for file in base.glob('*/bottle.yml'):
            target = result.add(file.parent, 'Bottles')
            def programs(value):
                if isinstance(value, dict):
                    for key, child in value.items():
                        if key.lower() in ('path', 'executable'):
                            path = windows_path(target.prefix, child)
                            if path and path.parent.is_dir(): target.games.add(path.parent)
                        programs(child)
                elif isinstance(value, list):
                    for child in value: programs(child)
            programs(read_yaml(file))
    for prefix in [home / '.wine', *list((home / 'Games').glob('*/system.reg'))]:
        if prefix.name == 'system.reg': prefix = prefix.parent
        if (prefix / 'system.reg').is_file(): result.add(prefix, 'Wine')
    for target in result.targets.values(): target.games.update(registry_games(target.prefix))
    # System-distributed Proton tools are detected too; permission failures are
    # reported instead of being hidden or weakening directory permissions.
    for tool in Path('/usr/share/steam/compatibilitytools.d').glob('*/proton'):
        result.tools.add(tool.parent.resolve())
    return result
