from __future__ import annotations
from pathlib import Path
import ast
import json
import re

from .prepare import ROOT

BEGIN = '# BEGIN OPENRGB GAME SYNC'
END = '# END OPENRGB GAME SYNC'


def proton_settings(original):
    """Keep user preferences and append a small, fail-open launch callback."""
    original = re.sub(r'^'+re.escape(BEGIN)+r'\n.*?^'+re.escape(END)+r'\n?', '', original, flags=re.M|re.S)
    hook = ROOT / 'automation/proton_hook.py'
    block = f'''
{BEGIN}
try:
    user_settings
except NameError:
    user_settings = {{}}
try:
    import importlib.util as _orgbgs_import
    import sys as _orgbgs_sys
    _orgbgs_root = {str(ROOT)!r}
    if _orgbgs_root not in _orgbgs_sys.path:
        _orgbgs_sys.path.insert(0, _orgbgs_root)
    _orgbgs_spec = _orgbgs_import.spec_from_file_location('_orgbgs_launch_hook', {str(hook)!r})
    _orgbgs_module = _orgbgs_import.module_from_spec(_orgbgs_spec)
    _orgbgs_spec.loader.exec_module(_orgbgs_module)
    _orgbgs_module.install_hook(__import__('__main__'))
except Exception as _orgbgs_error:
    import sys as _orgbgs_sys
    _orgbgs_sys.stderr.write('OpenRGB Game Sync: launch preparation unavailable: ' + str(_orgbgs_error) + '\\n')
{END}
'''
    result = original.rstrip()+'\n'+block
    ast.parse(result)
    return result


def install_hooks(inventory, store):
    report = {'proton': [],'Lutris':[],'Heroic':[],'errors':[]}
    for tool in sorted(inventory.tools):
        try:
            source = (tool/'proton').read_text(errors='replace')
            if not all(name in source for name in ('user_settings','g_session','def run(')):
                report['errors'].append('Motor Proton no reconocido: '+str(tool)); continue
            import os
            if not os.access(tool,os.W_OK):
                report['errors'].append('Motor Proton de solo lectura: '+str(tool)); continue
            settings = tool/'user_settings.py'
            current = settings.read_text() if settings.exists() else ''
            store.install(settings,proton_settings(current).encode())
            report['proton'].append(str(tool))
        except (OSError,ValueError,SyntaxError) as exc: report['errors'].append(str(tool)+': '+str(exc))
    command = f'"{ROOT / "auto_launch.sh"}"'
    for file in inventory.configs.get('Lutris',[]):
        try:
            import yaml
            content = yaml.safe_load(file.read_text()) if file.exists() else {}
            content = content or {}; system = content.setdefault('system',{})
            existing = system.get('prelaunch_command','')
            if existing and not any(name in existing for name in ('openrgb-game-sync/auto_prepare.py','openrgb-game-sync/auto_launch.sh')):
                report['errors'].append('Se conserva la tarea previa existente de Lutris: '+str(file)); continue
            system['prelaunch_command'] = command; system['prelaunch_wait'] = True
            store.install(file,yaml.safe_dump(content,sort_keys=False,allow_unicode=True).encode())
            report['Lutris'].append(str(file))
        except ImportError as exc: report['errors'].append(str(file)+': '+str(exc))
        except (OSError,ValueError,yaml.YAMLError) as exc: report['errors'].append(str(file)+': '+str(exc))
    for file in inventory.configs.get('Heroic',[]):
        try:
            content = json.loads(file.read_text()); settings = content.setdefault('defaultSettings',{})
            existing = settings.get('beforeLaunchScriptPath','')
            if existing and 'openrgb-game-sync/auto_launch.sh' not in existing:
                report['errors'].append('Se conserva la tarea previa existente de Heroic: '+str(file)); continue
            settings['beforeLaunchScriptPath'] = str(ROOT/'auto_launch.sh')
            for game_file in (file.parent/'GamesConfig').glob('*.json'):
                game_content = json.loads(game_file.read_text())
                def update_game(value):
                    if isinstance(value,dict):
                        if 'winePrefix' in value or 'beforeLaunchScriptPath' in value:
                            previous=value.get('beforeLaunchScriptPath','')
                            if not previous or 'openrgb-game-sync/auto_launch.sh' in previous:
                                value['beforeLaunchScriptPath']=str(ROOT/'auto_launch.sh')
                        for child in value.values(): update_game(child)
                    elif isinstance(value,list):
                        for child in value: update_game(child)
                update_game(game_content)
                store.install(game_file,(json.dumps(game_content,indent=2,ensure_ascii=False)+'\n').encode())
            store.install(file,(json.dumps(content,indent=2,ensure_ascii=False)+'\n').encode())
            report['Heroic'].append(str(file))
        except (OSError,ValueError) as exc: report['errors'].append(str(file)+': '+str(exc))
    return report
