"""Quarantine local RGB adapters and restore the original Diablo library."""
from pathlib import Path
import datetime,hashlib,json,re,shutil
from automation.prepare import ROOT,atomic_bytes,prefix_active,serialized
home=Path.home();prefix=home/'Games/battlenet';game=Path('/mnt/Datos/Diablo IV')
if prefix_active(prefix): raise SystemExit('Battle.net o algún juego sigue abierto; ciérralo antes de restaurar las bibliotecas.')
backup=home/'.local/share/openrgb-backups'/('diablo-rollback-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
backup.mkdir(parents=True)
for name in ['FenrisDebug.txt','last_error.txt']:
 if (game/name).is_file(): shutil.copy2(game/name,backup/name)
original=home/'.local/share/openrgb-backups/game-sync-20261016-171544/6-CUESDK.x64_2015.dll'
if not original.is_file():raise SystemExit('No se encuentra la DLL original guardada')
state_file=home/'.local/share/openrgb-game-sync/automation/state.json';state=json.loads(state_file.read_text())
known={hashlib.sha256(p.read_bytes()).hexdigest() for p in (ROOT/'build').glob('*.dll')}
for entries in state['files'].values():known.update(r['installed_sha256'] for r in entries)
manifest=[]
with serialized(timeout=2):
 target=game/'CUESDK.x64_2015.dll';current=target.read_bytes()
 if current!=original.read_bytes():
  if hashlib.sha256(current).hexdigest() not in known:raise SystemExit('La DLL del juego ha cambiado; no se sobrescribe una actualización desconocida')
  saved=backup/'adapter-CUESDK.x64_2015.dll';shutil.copy2(target,saved)
  atomic_bytes(target,original.read_bytes(),0o644);manifest.append({'target':str(target),'adapter':str(saved),'restored':str(original)})
 for directory in ['system32','syswow64']:
  folder=prefix/'drive_c/windows'/directory
  for name in ['RzChromaSDK64.dll','RzChromaSDK.dll','CUESDK.x64_2015.dll','CUESDK_2015.dll','iCUESDK.x64.dll','LogitechLed.dll','LogitechLedEnginesWrapper.dll']:
   target=folder/name
   if not target.is_file() or target.is_symlink():continue
   if hashlib.sha256(target.read_bytes()).hexdigest() not in known:continue
   saved=backup/(directory+'-'+name);shutil.copy2(target,saved);target.unlink()
   manifest.append({'target':str(target),'adapter':str(saved),'removed':True})
 registry=prefix/'system.reg';text=registry.read_text();old=(home/'.local/share/openrgb-backups/game-sync-20261016-171544/system.reg').read_text()
 from bridge_registration import LIGHTSYNC_KEY
 keys=[LIGHTSYNC_KEY,LIGHTSYNC_KEY.replace(r'Classes\\CLSID',r'Classes\\Wow6432Node\\CLSID')]
 for key in keys:
  pattern=re.compile(r'^'+re.escape('['+key+']')+r'[^\n]*\n.*?(?=^\[|\Z)',re.M|re.S)
  prior=pattern.search(old)
  text=pattern.sub(lambda _:prior.group(0) if prior else '',text,count=1)
 if text!=registry.read_text():
  shutil.copy2(registry,backup/'system.reg.before');atomic_bytes(registry,text.encode(),registry.stat().st_mode&0o777)
 for record in manifest:
  for entry in state['files'].get(record['target'],[]):
   if entry.get('status')=='installed':entry['status']='rolled_back'
 atomic_bytes(state_file,(json.dumps(state,indent=2)+'\n').encode())
 atomic_bytes(backup/'manifest.json',(json.dumps(manifest,indent=2)+'\n').encode())
 assert (game/'CUESDK.x64_2015.dll').read_bytes()==original.read_bytes()
 print('Original Diablo DLL verified. Quarantined adapters:',len(manifest)-1)
 print('Rollback record:',backup)
