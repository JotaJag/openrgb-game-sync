from pathlib import Path
import subprocess, sys
from automation.prepare import ROOT, Store, serialized
home=Path.home()
with serialized(home):
    store=Store(home)
    plugin=home/'.config/OpenRGB/plugins/libOpenRGBGameSyncPlugin.so'
    store.install(plugin,(ROOT/'build/libOpenRGBGameSyncPlugin.so').read_bytes())
result=subprocess.run([sys.executable,str(ROOT/'auto_prepare.py'),'--enable'],check=True)
print('Plugin 0.4 and launcher automation installed; originals are journaled.')
