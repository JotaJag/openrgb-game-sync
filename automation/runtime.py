from __future__ import annotations
from pathlib import Path
import json
import time

from .prepare import atomic_bytes


def configuration(home=None):
    home = Path(home or Path.home())
    return home / '.config/OpenRGB/plugins/settings/GameSyncAutomation.json'


def enabled(home=None):
    home = Path(home or Path.home())
    try:
        config = json.loads(configuration(home).read_text())
        if not config.get('enabled',False): return False
    except (OSError,ValueError): return False
    try:
        settings = json.loads((home/'.config/OpenRGB/OpenRGB.json').read_text())
        if settings.get('OpenRGB Game Sync',{}).get('enabled',True) is False: return False
        for plugin in settings.get('Plugins',{}).get('plugins',[]):
            if plugin.get('name') == 'OpenRGB Game Sync' and plugin.get('enabled') is False: return False
    except (OSError,ValueError,AttributeError): pass
    return True


def save_status(report,home=None):
    home = Path(home or Path.home())
    file = home / '.local/share/openrgb-game-sync/automation/status.json'
    try: previous=json.loads(file.read_text())
    except (OSError,ValueError): previous={}
    if 'launch' in report:
        previous['launch']=report['launch']; report=previous
    elif 'launch' in previous: report=dict(report,launch=previous['launch'])
    report['updated_at']=int(time.time())
    atomic_bytes(file,(json.dumps(report,indent=2)+'\n').encode())
