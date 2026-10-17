"""Install the original-DLL service; do not prepare or modify any game library."""
from pathlib import Path
import json,hashlib
from automation.prepare import ROOT,Store,serialized
from automation.runtime import configuration
home=Path.home();game=Path('/mnt/Datos/Diablo IV/CUESDK.x64_2015.dll')
original=home/'.local/share/openrgb-backups/game-sync-20261016-171544/6-CUESDK.x64_2015.dll'
assert game.read_bytes()==original.read_bytes(),'The game does not have its original DLL'
with serialized(timeout=2):
 store=Store(home)
 store.install(home/'.config/OpenRGB/plugins/libOpenRGBGameSyncPlugin.so',(ROOT/'build/libOpenRGBGameSyncPlugin.so').read_bytes())
 file=configuration();settings=json.loads(file.read_text())
 assert not settings['enabled'],'Keep library preparation paused'
 settings['pipe_services']=[{'prefix':str(home/'Games/battlenet'),
  'proton':'/mnt/Juegos/SteamLibrary/steamapps/common/Proton - Experimental',
  'status':str(home/'.local/share/openrgb-game-sync/pipe-service/battlenet.json')}]
 store.install(file,(json.dumps(settings,indent=2)+'\n').encode())
 assert game.read_bytes()==original.read_bytes()
 print('Installed plugin 0.5 and original-DLL service configuration.')
 print('Diablo original SHA256:',hashlib.sha256(game.read_bytes()).hexdigest())
 print('Library replacement stays paused and blocked for Diablo/Battle.net.')
