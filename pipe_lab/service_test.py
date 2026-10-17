from pathlib import Path
import os,subprocess,time,socket,struct,json,signal,sys,tempfile
root=Path(__file__).resolve().parents[1];lab=root/'build/pipe-lab';prefix=(lab/'prefix.txt').read_text().strip()
sock=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);sock.bind(('127.0.0.1',0));sock.settimeout(.1)
env=dict(os.environ,WINEPREFIX=prefix,WINEDEBUG='-all',DISPLAY='',WAYLAND_DISPLAY='',OPENRGB_GAME_SYNC_BACKGROUND='1',OPENRGB_GAME_SYNC_PORT=str(sock.getsockname()[1]))
umu='--umu' in sys.argv
proton=os.environ.get('PROTONPATH', '')
if umu and not proton: raise SystemExit('Set PROTONPATH to the Proton installation used for this isolated test.')
if umu:
 prefix=tempfile.mkdtemp(prefix='openrgb-pipe-proton-test-');env.update(WINEPREFIX=prefix,PROTONPATH=proton,PROTON_VERB='runinprefix',GAMEID='umu-default',UMU_RUNTIME_UPDATE='0',UMU_LOG='warn')
log=lab/('protocol-umu.jsonl' if umu else 'protocol.jsonl');log.unlink(missing_ok=True)
service=subprocess.Popen(['python3',str(root/'pipe_service/run.py'),'--prefix',prefix,'--log',str(log),'--host-log',str(lab/'host.log')]+(['--umu-proton',proton] if umu else []),env=env,stdout=subprocess.DEVNULL,stderr=subprocess.PIPE,start_new_session=True)
try:
 time.sleep(10 if umu else 2)
 print('Service before client:',service.poll())
 if service.poll() is not None:print(service.stderr.read().decode(errors='replace')[:2000])
 client=subprocess.Popen(['/usr/bin/umu-run' if umu else 'wine',str(lab/('latency-probe.exe' if '--latency' in sys.argv else 'original-probe.exe'))],cwd=lab,env=env,stdout=subprocess.PIPE,stderr=subprocess.DEVNULL)
 packets=[];deadline=time.monotonic()+(60 if umu else 15)
 while time.monotonic()<deadline:
  try:packets.append(sock.recv(4096))
  except socket.timeout:pass
  if client.poll() is not None:break
 if client.poll() is None:client.terminate();raise RuntimeError('Original SDK timed out')
 print(client.stdout.read().decode(errors='replace'));print('Packets:',len(packets),'Exit:',client.returncode)
 assert client.returncode==0,'The original SDK API checks failed'
 if '--latency' in sys.argv: raise SystemExit(0)
 frames=[p for p in packets if struct.unpack_from('<H',p,6)[0]==1]
 releases=[p for p in packets if struct.unpack_from('<H',p,6)[0]==2]
 assert len(frames)>=2 and releases,'Colors or release missing'
 assert all(struct.unpack_from('<I6H',p)==(0x3153474f,2,1,513,6,21,126) for p in frames)
 assert all(struct.unpack_from('<I',p,96)[0]==0x1e140a for p in frames)
 assert all(len(p)==96+126*4 for p in frames)
 assert any(struct.unpack_from('<I',p,100)[0]==0x1e140a for p in frames),'Buffered colors missing'
 print('PASS: unchanged original SDK handshake, devices, sync/async/buffered colors, readback and release')
 print('Host diagnostics:',(lab/'host.log').read_text(errors='replace')[-1600:])

 if log.exists():
  for line in log.read_text().splitlines():
   record=json.loads(line);print(record['request'], '=>',str(record['response'])[:160])
finally:
 if service.poll() is None: os.killpg(service.pid,signal.SIGTERM)
 service.wait(timeout=5)

 if not umu: subprocess.run(['wineserver','-k'],env=env,timeout=5)
