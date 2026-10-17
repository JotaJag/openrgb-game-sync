from pathlib import Path
import os,signal,subprocess,time,json
home=Path.home();found=[]
for proc in Path('/proc').iterdir():
 if not proc.name.isdigit():continue
 try:
  if proc.stat().st_uid!=os.getuid():continue
  if os.path.realpath(proc/'exe')!='/usr/bin/openrgb':continue
  found.append(int(proc.name))
 except OSError:pass
if len(found)>1:raise SystemExit('Hay varias instancias de OpenRGB; no se reinician automáticamente.')
for pid in found:
 os.kill(pid,signal.SIGTERM)
 deadline=time.monotonic()+8
 while time.monotonic()<deadline and Path('/proc',str(pid)).exists():time.sleep(.1)
 if Path('/proc',str(pid)).exists():raise SystemExit('OpenRGB no se ha cerrado; no se fuerza su salida.')
log=home/'.local/share/openrgb-game-sync/openrgb-service-launch.log';log.parent.mkdir(parents=True,exist_ok=True)
with log.open('a') as file:
 process=subprocess.Popen(['/usr/bin/openrgb','--startminimized'],stdin=subprocess.DEVNULL,stdout=file,stderr=file,start_new_session=True)
 print('OpenRGB started:',process.pid)
status=home/'.local/share/openrgb-game-sync/pipe-service/battlenet.json'
deadline=time.monotonic()+35
while time.monotonic()<deadline:
 if process.poll() is not None:raise SystemExit('OpenRGB terminó; consulta '+str(log))
 try:
  value=json.loads(status.read_text())
  if value.get('running') and value.get('updated_at',0)>=time.time()-3:
   print('Original-DLL service ready. Connected sessions:',value.get('sessions'));break
 except (OSError,ValueError):pass
 time.sleep(.25)
else:print('OpenRGB is running; service readiness is not yet confirmed. Log:',log)
