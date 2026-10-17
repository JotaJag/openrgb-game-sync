"""Prepare Logitech LED SDK discovery in a stopped Wine prefix."""
from pathlib import Path
import re
import time

LIGHTSYNC_KEY = r"Software\\Classes\\CLSID\\{a6519e67-7632-4375-afdf-caa889744403}\\ServerBinary"

def registered_system_text(text: str, include32=False) -> str:
    if not text.startswith("WINE REGISTRY Version 2"):
        raise ValueError("El archivo no es un registro Wine reconocido")
    keys = [(LIGHTSYNC_KEY,'system32')]
    if include32:
        keys.append((LIGHTSYNC_KEY.replace(r'Classes\\CLSID',r'Classes\\Wow6432Node\\CLSID'),'syswow64'))
    for key,directory in keys:
        section = "[" + key + "]"
        pattern = re.compile(r"^" + re.escape(section) + r"[^\n]*\n.*?(?=^\[|\Z)", re.M | re.S)
        line = '@="C:\\\\windows\\\\'+directory+'\\\\LogitechLed.dll"'
        existing = pattern.search(text)
        if existing and line in existing.group(0).splitlines(): continue
        replacement = section + " " + str(int(time.time())) + '\n'+line+'\n\n'
        if existing: text = pattern.sub(lambda _: replacement,text,count=1)
        else: text = text.rstrip()+"\n\n"+replacement
    return text
