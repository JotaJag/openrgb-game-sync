"""Exercise the actual Windows DLL under Wine in a separate temporary prefix."""
import os
import sys
from pathlib import Path
import socket
import struct
import subprocess
import tempfile
import time

root = Path(__file__).resolve().parent.parent
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind(("127.0.0.1", 0))
sock.settimeout(0.1)
env = dict(os.environ, WINEPREFIX=tempfile.mkdtemp(prefix="openrgb-bridge-wine-test-"),
           WINEDEBUG="-all", DISPLAY="", WAYLAND_DISPLAY="", OPENRGB_GAME_SYNC_BACKGROUND="1",
           OPENRGB_GAME_SYNC_PORT=str(sock.getsockname()[1]))
proc = subprocess.Popen(["wine", str(root / ("build/native-smoke32.exe" if "--32" in sys.argv else "build/native-smoke.exe"))], cwd=root / "build", env=env,
                        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
frames, released = 0, False
deadline = time.monotonic() + 50
while time.monotonic() < deadline:
    try:
        data, peer = sock.recvfrom(4096)
    except socket.timeout:
        if proc.poll() is not None:
            break
        continue
    assert peer[0] == "127.0.0.1"
    magic, version, kind, device, rows, cols, count = struct.unpack_from("<I6H", data)
    assert magic == 0x3153474F and version == 1
    if kind == 1:
        assert device == 6 and rows == 1 and cols == count == 5
        assert struct.unpack_from("<5I", data, 96) == (0x1E140A,) * 5
        frames += 1
    elif kind == 2:
        released = True
if proc.poll() is None:
    proc.terminate()
    raise RuntimeError("Wine bridge test timed out")
assert proc.returncode == 0, f"Windows smoke test failed: {proc.returncode}"
assert frames >= 2 and released, (frames, released)
print(f"PASS: real Chroma DLL via Wine delivered {frames} valid frames and released its session")
env["OPENRGB_GAME_SYNC_BACKGROUND"] = "0"
proc = subprocess.Popen(["wine", str(root / ("build/native-smoke32.exe" if "--32" in sys.argv else "build/native-smoke.exe"))], cwd=root / "build", env=env,
                        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
background_frames = 0
deadline = time.monotonic() + 20
while time.monotonic() < deadline:
    try:
        data, _ = sock.recvfrom(4096)
        background_frames += struct.unpack_from("<H", data, 6)[0] == 1
    except socket.timeout:
        if proc.poll() is not None:
            break
assert proc.poll() == 0 and background_frames == 0
print("PASS: a process without foreground focus does not take over lighting")
print("Isolated test prefix:", env["WINEPREFIX"])
