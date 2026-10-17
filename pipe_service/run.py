#!/usr/bin/env python3
from pathlib import Path
import sys
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import argparse, os, json, struct, subprocess, signal, socket, secrets, time
from pipe_service.backend import Backend,ROOT

def exact(stream,size):
 data=b''
 while len(data)<size:
  part=stream.read(size-len(data))
  if not part:raise EOFError
  data+=part
 return data

def main():
 signal.signal(signal.SIGTERM,lambda *_: (_ for _ in ()).throw(SystemExit(0)))
 parser=argparse.ArgumentParser();parser.add_argument('--prefix',required=True);parser.add_argument('--wine',default='wine');parser.add_argument('--log');parser.add_argument('--umu-proton');parser.add_argument('--status');parser.add_argument('--host-log');args=parser.parse_args()
 env=dict(os.environ,WINEPREFIX=args.prefix,WINEDEBUG='-all')
 backend=Backend(int(os.environ.get('OPENRGB_GAME_SYNC_PORT','6743')),os.environ.get('OPENRGB_GAME_SYNC_BACKGROUND')=='1')
 command=[args.wine,str(ROOT/'build/cue-pipe-host.exe')]
 if args.umu_proton:
  env.update(PROTONPATH=args.umu_proton,PROTON_VERB='runinprefix',GAMEID='umu-default',UMU_RUNTIME_UPDATE='0',UMU_LOG='warn')
  command=['/usr/bin/umu-run',str(ROOT/'build/cue-pipe-host.exe')]
 listener=socket.socket(socket.AF_INET,socket.SOCK_STREAM);listener.bind(('127.0.0.1',0));listener.listen(4);listener.settimeout(.2)
 token=secrets.token_hex(16);env.update(OPENRGB_CUE_PIPE_PORT=str(listener.getsockname()[1]),OPENRGB_CUE_PIPE_TOKEN=token)
 process=subprocess.Popen(command,env=env,stdin=subprocess.DEVNULL,stdout=subprocess.DEVNULL,stderr=open(args.host_log,'w') if args.host_log else subprocess.DEVNULL,start_new_session=True)
 connection=None
 try:
  deadline=time.monotonic()+30
  while connection is None:
   if process.poll() is not None:raise RuntimeError('Wine host failed: '+str(process.returncode))
   if time.monotonic()>deadline:raise TimeoutError('Wine host connection timed out')
   try: candidate,_=listener.accept()
   except socket.timeout:continue
   candidate.settimeout(3);stream=candidate.makefile('rb',buffering=0)
   try: authenticated=exact(stream,32)==token.encode()
   except (OSError,EOFError): authenticated=False
   if not authenticated:stream.close();candidate.close();continue
   connection=candidate;connection.settimeout(5);connection.setsockopt(socket.IPPROTO_TCP,socket.TCP_NODELAY,1);break
  listener.close()
  last_status=0
  while True:
   marker=exact(stream,4)
   skipped=0
   while marker!=b'OSGP':
    marker=marker[1:]+exact(stream,1);skipped+=1
    if skipped>1048576:raise ValueError('Unexpected host output')
   size=struct.unpack('<I',exact(stream,4))[0]
   if size>65536:raise ValueError('Host packet limit')
   event=json.loads(exact(stream,size))
   try:reply=backend.dispatch(event)
   except (ValueError,TypeError,KeyError) as exc:backend.errors=(backend.errors+[str(exc)])[-8:];reply={'errorCode':5,'result':False}
   if args.log and event.get('event')=='request':
    with open(args.log,'a') as log:log.write(json.dumps({'request':event['request'],'response':reply})+'\n')
   data=json.dumps(reply,separators=(',',':')).encode();connection.sendall(struct.pack('<I',len(data))+data)
   if args.status and time.monotonic()-last_status>=1:
    last_status=time.monotonic()
    from automation.prepare import atomic_bytes
    status={'running':True,'sessions':len(backend.sessions),'calls':backend.calls,'errors':backend.errors[-8:],'updated_at':int(time.time()),'accepted_color_calls':backend.accepted_colors,'frames_sent':backend.frames_sent,'focus_pid':backend.focus,'client_pids':[s['pid'] for s in backend.sessions.values()],'ignored_led_ids':sorted(backend.ignored_leds)}
    atomic_bytes(Path(args.status),(json.dumps(status)+'\n').encode())
 except EOFError:
  code=process.wait(timeout=3)
  if code: raise RuntimeError("Wine pipe host exited: "+str(code))
 except KeyboardInterrupt:pass
 finally:
  if connection: connection.close()
  listener.close()
  backend.close()
  if args.status:
   from automation.prepare import atomic_bytes
   atomic_bytes(Path(args.status),(json.dumps({'running':False,'calls':backend.calls,'errors':backend.errors[-8:]})+'\n').encode())
  if process.poll() is None:
   try:os.killpg(process.pid,signal.SIGTERM)
   except ProcessLookupError:pass
   try:process.wait(timeout=2)
   except subprocess.TimeoutExpired:os.killpg(process.pid,signal.SIGKILL);process.wait()
if __name__=='__main__':main()
