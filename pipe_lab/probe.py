from pathlib import Path
import os,subprocess,tempfile
root=Path(__file__).resolve().parents[1]
prefix=tempfile.mkdtemp(prefix='openrgb-original-cue-lab-')
env=dict(os.environ,WINEPREFIX=prefix,WINEDEBUG='-all,+file',DISPLAY='',WAYLAND_DISPLAY='')
log=root/'build/pipe-lab/wine-file.log'
with log.open('w') as error:
 result=subprocess.run(['wine',str(root/'build/pipe-lab/original-probe.exe')],cwd=root/'build/pipe-lab',env=env,stdout=subprocess.PIPE,stderr=error,text=True,timeout=60)
print(result.stdout);print('Exit:',result.returncode,'Isolated prefix:',prefix)
(root/'build/pipe-lab/prefix.txt').write_text(prefix)
for line in log.read_text(errors='replace').splitlines():
 if 'CorsairUtilityEngine' in line:print(line[:700])
