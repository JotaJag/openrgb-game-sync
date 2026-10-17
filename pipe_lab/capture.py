from pathlib import Path
import os,subprocess,time
root=Path(__file__).resolve().parents[1];lab=root/'build/pipe-lab'
env=dict(os.environ,WINEPREFIX=(lab/'prefix.txt').read_text().strip(),WINEDEBUG='-all,+file',DISPLAY='',WAYLAND_DISPLAY='')
(lab/'captured.bin').unlink(missing_ok=True)
with (lab/'server.log').open('w') as log,(lab/'server-wine.log').open('w') as err:
 server=subprocess.Popen(['wine',str(lab/'capture-server.exe')],cwd=lab,env=env,stdout=log,stderr=err)
 deadline=time.monotonic()+10
 while time.monotonic()<deadline:
  if 'ready' in (lab/'server.log').read_text():break
  if server.poll() is not None:raise RuntimeError((lab/'server.log').read_text())
  time.sleep(.1)
 with (lab/'client.log').open('w') as out,(lab/'client-wine.log').open('w') as error:
  client=subprocess.Popen(['wine',str(lab/'original-probe.exe')],cwd=lab,env=env,stdout=out,stderr=error)
  time.sleep(5)
  subprocess.run(['wineserver','-k'],env=env,timeout=10,check=False)
  client.wait(timeout=5);server.wait(timeout=5)
print((lab/'server.log').read_text());print((lab/'client.log').read_text())
if (lab/'captured.bin').exists():print('Raw:',repr((lab/'captured.bin').read_bytes()))
for line in (lab/'client-wine.log').read_text(errors='replace').splitlines():
 if 'CorsairUtilityEngine' in line and ('CreateFileW ' in line or 'CreateNamedPipeW' in line):print(line[:400])
