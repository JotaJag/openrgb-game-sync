"""Disposable preparation worker: the launch callback enforces a total budget."""
import sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import os
import subprocess
from automation.prepare import Store, serialized, prepare_prefix
from automation.runtime import save_status


def main():
    prefix=Path(sys.argv[1]); wine=sys.argv[2]; games=[Path(p) for p in sys.argv[3:] if p]
    def register():
        paths=[('64','system32'),('32','syswow64')]
        if '#arch=win32' in (prefix/'system.reg').read_text(): paths=[('32','system32')]
        for bits,directory in paths:
            # Children stay in this worker's process group, so the parent can
            # terminate the whole preparation if Wine or storage stalls.
            rc=subprocess.run([wine,'reg','add',
                r'HKLM\Software\Classes\CLSID\{a6519e67-7632-4375-afdf-caa889744403}\ServerBinary',
                '/ve','/t','REG_SZ','/d',f'C:\\windows\\{directory}\\LogitechLed.dll','/f','/reg:'+bits],
                env=dict(os.environ,WINEPREFIX=str(prefix)),stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,timeout=2).returncode
            if rc: return False
        return True
    with serialized(timeout=.25):
        report=prepare_prefix(prefix,games,Store(),launch=True,registry_writer=register,force=True)
        save_status({'launch':report})
    return 0

if __name__=='__main__':
    try: sys.exit(main())
    except Exception as exc:
        save_status({'errors':['Preparación de arranque omitida: '+str(exc)]}); sys.exit(1)
