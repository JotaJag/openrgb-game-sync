"""Observe the new APIs through real Windows DLLs and worker threads in isolated Wine."""
from pathlib import Path
import os
import signal
import socket
import struct
import subprocess
import tempfile
import time

root=Path(__file__).resolve().parents[1]
sock=socket.socket(socket.AF_INET,socket.SOCK_DGRAM)
sock.bind(('127.0.0.1',0));sock.settimeout(.1)
prefix=tempfile.mkdtemp(prefix='openrgb-extended-wine-')
env=dict(os.environ,WINEPREFIX=prefix,WINEDEBUG='-all',DISPLAY='',WAYLAND_DISPLAY='',
         OPENRGB_GAME_SYNC_BACKGROUND='1',OPENRGB_GAME_SYNC_PORT=str(sock.getsockname()[1]))
process=subprocess.Popen(['wine',str(root/'build/extended-smoke.exe')],cwd=root/'build',env=env,
                         stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,start_new_session=True)
extended=mouse=flash=pulse=False
released=set()
try:
    deadline=time.monotonic()+75
    while time.monotonic()<deadline:
        try: packet=sock.recv(4096)
        except socket.timeout:
            if process.poll() is not None: break
            continue
        magic,version,kind,device,rows,cols,count=struct.unpack_from('<I6H',packet)
        assert magic==0x3153474f and version in (1,2)
        assert len(packet)==96+4*count
        # Older transports leave the role/source field empty on whole-session release.
        if kind==2:
            released.add(packet[16:32]);continue
        assert kind in (1,3)
        if kind==3:continue
        source=1 if version==1 else device>>8
        role=device&255
        colors=struct.unpack_from(f'<{count}I',packet,96)
        if source==1 and role==1 and (rows,cols,count)==(8,24,192):
            assert colors[0]==255 and colors[-1]==0x123456
            extended=True
        if source==2 and role==2:
            assert (rows,cols,count)==(1,4,4) and set(colors)=={0x1e140a}
            mouse=True
        if source==3 and role==1:
            flash |= colors[0]==0xff
            pulse |= (colors[0]&255)==0 and ((colors[0]>>8)&255)>0
    if process.poll() is None:raise RuntimeError('Extended Wine smoke test timed out')
    assert process.returncode==0,('Windows API failure',process.returncode)
    assert extended and mouse and flash and pulse,(extended,mouse,flash,pulse)
    assert len(released)>=3,('Missing session release',len(released))
    print('PASS: real Wine DLLs send Chroma CUSTOM2, iCUE 4 mouse, Lightsync flash/pulse and release.')
finally:
    if process.poll() is None:os.killpg(process.pid,signal.SIGTERM);process.wait(timeout=5)
    sock.close()
    subprocess.run(['wineserver','-k'],env=env,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,timeout=10)
    print('Isolated fixture:',prefix)
