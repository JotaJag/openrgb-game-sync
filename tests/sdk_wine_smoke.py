"""Exercise actual iCUE and Lightsync DLLs under an isolated Wine prefix."""
from pathlib import Path
import os
import socket
import struct
import subprocess
import tempfile
import time
import sys
import shutil

root = Path(__file__).resolve().parent.parent
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind(("127.0.0.1",0)); sock.settimeout(.1)
env = dict(os.environ, WINEPREFIX=tempfile.mkdtemp(prefix="openrgb-sdk-wine-test-"),
           WINEDEBUG="-all", DISPLAY="", WAYLAND_DISPLAY="", OPENRGB_GAME_SYNC_BACKGROUND="1",
           OPENRGB_GAME_SYNC_PORT=str(sock.getsockname()[1]))
for exe, source in [("cue4-smoke.exe",2),("cue-smoke.exe",2),("lightsync-smoke.exe",3),("cue-smoke32.exe",2),("lightsync-smoke32.exe",3)]:
    proc = subprocess.Popen(["wine",str(root / "build" / exe)], cwd=root / "build", env=env,
                            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    frame_count, released, saw_blue, last_release = 0,False,False,False
    deadline=time.monotonic()+50
    while time.monotonic()<deadline:
        try: data,peer=sock.recvfrom(4096)
        except socket.timeout:
            if proc.poll() is not None: break
            continue
        magic,version,kind,device,rows,cols,count=struct.unpack_from("<I6H",data)
        assert magic==0x3153474f and version==2
        if kind==2:
            released=True; last_release=True; continue
        assert not last_release, "adapter resumed after explicit release"
        assert device>>8==source and len(data)==96+4*count
        colors=struct.unpack_from(f"<{count}I",data,96)
        if source==2:
            assert device&255==1 and (rows,cols)==(6,21)
            assert colors[0]==0x1e140a and set(colors)<={0,0x1e140a}
        else:
            assert device&255 in (1,6)
            assert set(colors)<={0x0000ff,0x1e140a,0xff0000}
            saw_blue |= 0xff0000 in colors
        frame_count+=1
    if proc.poll() is None:
        proc.terminate(); raise RuntimeError(f"{exe} timed out")
    assert proc.returncode==0,(exe,proc.returncode)
    assert frame_count>=2 and released,(frame_count,released)
    if source==3: assert saw_blue,"per-key Lightsync effect was not delivered"
    print(f"PASS: {exe}: {frame_count} valid frames, release and API checks")
print("Isolated prefix:",env["WINEPREFIX"])
sys.path.insert(0,str(root))
from bridge_registration import registered_system_text
subprocess.run(["wineserver","-k"],env=env,check=True,timeout=10)
subprocess.run(["wineserver","-w"],env=env,check=True,timeout=10)
prefix=Path(env["WINEPREFIX"])
registry=prefix / "system.reg"
registry.write_text(registered_system_text(registry.read_text(),include32=True))
shutil.copy2(root / "build/LogitechLed.dll",prefix / "drive_c/windows/system32/LogitechLed.dll")
query=subprocess.run(["wine","reg","query",r"HKLM\Software\Classes\CLSID\{a6519e67-7632-4375-afdf-caa889744403}\ServerBinary","/ve","/reg:64"],
                     env=env,capture_output=True,text=True,errors="replace",timeout=20)
assert query.returncode==0 and "LogitechLed.dll" in query.stdout,(query.returncode,query.stdout)
print("PASS: Wine discovers the registered 64-bit Lightsync adapter path")

query=subprocess.run(["wine","reg","query",r"HKLM\Software\Classes\CLSID\{a6519e67-7632-4375-afdf-caa889744403}\ServerBinary","/ve","/reg:32"],env=env,capture_output=True,text=True,errors="replace",timeout=20)
assert query.returncode==0 and "syswow64" in query.stdout.lower(),(query.returncode,query.stdout)
print("PASS: Wine discovers the registered 32-bit Lightsync adapter path")

worker_env=dict(env,HOME=tempfile.mkdtemp(prefix="openrgb-worker-home-"))
worker=subprocess.run([sys.executable,str(root/'automation/launch_worker.py'),str(prefix),shutil.which('wine')],env=worker_env,capture_output=True,timeout=8)
assert worker.returncode==0,(worker.returncode,worker.stderr)
query=subprocess.run(["wine","reg","query",r"HKLM\Software\Classes\CLSID\{a6519e67-7632-4375-afdf-caa889744403}\ServerBinary","/ve","/reg:64"],env=env,capture_output=True,text=True,errors="replace",timeout=10)
assert query.returncode==0 and r"C:\windows\system32\LogitechLed.dll" in query.stdout,(query.returncode,query.stdout)
print("PASS: actual launch worker prepares SDKs and registers the live Wine prefix")
