from __future__ import annotations

from contextlib import contextmanager
from pathlib import Path
import fcntl
import hashlib
import json
import os
import re
import shutil
import struct
import tempfile
import time

from .discovery import registry_games

ROOT = Path(__file__).resolve().parents[1]
SDK_NAMES = re.compile(r'^(?:CUESDK(?:\.x64)?(?:_201[3579])?|iCUESDK(?:\.x64)?(?:_20[0-9]{2})?|RzChromaSDK(?:64)?|LogitechLed(?:EnginesWrapper)?)\.dll$', re.I)
SOURCES = {
    64: {'icue4':'iCUESDK.x64.dll','chroma': 'RzChromaSDK64.dll', 'icue': 'CUESDK.x64_2015.dll', 'lightsync': 'LogitechLed.dll'},
    32: {'chroma': 'RzChromaSDK.dll', 'icue': 'CUESDK_2015.dll', 'lightsync': 'LogitechLed32.dll'},
}


def pe_metadata(path):
    """Read bounded PE metadata; never execute a game's library to identify it."""
    if path.stat().st_size > 64 * 1024 * 1024: raise ValueError('DLL demasiado grande')
    data = path.read_bytes()
    def u16(off): return struct.unpack_from('<H', data, off)[0]
    def u32(off): return struct.unpack_from('<I', data, off)[0]
    if data[:2] != b'MZ' or len(data) < 64: raise ValueError('No es PE')
    pe = u32(60)
    if data[pe:pe+4] != b'PE\0\0': raise ValueError('Cabecera PE inválida')
    bits = {0x8664: 64, 0x14c: 32}.get(u16(pe+4))
    if not bits: raise ValueError('Arquitectura no soportada')
    opt = pe+24
    directory = opt + (112 if u16(opt) == 0x20b else 96)
    section_start, section_count = opt+u16(pe+20), u16(pe+6)
    if section_count > 96: raise ValueError('Secciones no válidas')
    def offset(rva):
        for i in range(section_count):
            section = section_start+i*40
            virtual_size, virtual_address, raw_size, raw = struct.unpack_from('<4I',data,section+8)
            if virtual_address <= rva < virtual_address + min(max(virtual_size,raw_size),raw_size):
                result = raw+rva-virtual_address
                if result < len(data): return result
        raise ValueError('RVA fuera de la imagen')
    export = offset(u32(directory))
    count = u32(export+24)
    if count > 8192: raise ValueError('Demasiadas exportaciones')
    names = offset(u32(export+32))
    exports = set()
    for i in range(count):
        start = offset(u32(names+4*i)); end = data.find(b'\0',start,start+256)
        if end < 0: raise ValueError('Nombre de función no válido')
        exports.add(data[start:end].decode('ascii'))
    family = None
    if {'Init','SetEffect','CreateKeyboardEffect'} <= exports: family = 'chroma'
    elif {'CorsairPerformProtocolHandshake','CorsairSetLedsColors'} <= exports: family = 'icue'
    elif {'CorsairConnect','CorsairSetLedColors'} <= exports: family = 'icue4'
    elif {'LogiLedInit','LogiLedSetLighting'} <= exports: family = 'lightsync'
    return bits, family, exports


def pe_info(path):
    return pe_metadata(path)[:2]


def compatible_library(target, source):
    bits,family,exports=pe_metadata(target)
    adapter_bits,adapter_family,provided=pe_metadata(source)
    if (bits,family)!=(adapter_bits,adapter_family) or family is None:
        return False,'Familia o arquitectura incompatible'
    # A game may import any exported SDK function, not only identification ones.
    missing=sorted(exports-provided)
    if missing:
        return False,'Funciones no disponibles: '+', '.join(missing[:6])
    return True,''


def atomic_bytes(path, data, mode=0o644):
    path.parent.mkdir(parents=True, exist_ok=True)
    handle, temporary = tempfile.mkstemp(prefix='.'+path.name+'.',suffix='.gamesync',dir=path.parent)
    try:
        with os.fdopen(handle,'wb') as file:
            file.write(data); file.flush(); os.fsync(file.fileno())
        os.chmod(temporary,mode)
        os.replace(temporary,path)
    finally:
        if os.path.exists(temporary): os.unlink(temporary)


class Store:
    def __init__(self, home=None):
        self.home = Path(home or Path.home())
        self.directory = self.home / '.local/share/openrgb-game-sync/automation'
        self.directory.mkdir(parents=True,exist_ok=True)
        self.path = self.directory / 'state.json'
        try: self.data = json.loads(self.path.read_text())
        except (OSError,ValueError): self.data = {'files':{},'scan_index':{}}
    def save(self): atomic_bytes(self.path,(json.dumps(self.data,indent=2)+'\n').encode())
    def install(self, target, data):
        target = Path(target)
        if target.is_symlink(): raise ValueError('No se sustituye un enlace simbólico: '+str(target))
        current = target.read_bytes() if target.exists() else None
        if current == data: return False
        previous = None
        if current is not None:
            digest = hashlib.sha256(current).hexdigest()
            previous = self.directory / 'originals' / digest
            if not previous.exists(): atomic_bytes(previous,current)
        # Journal the original before replacing a file, including interrupted runs.
        record = {'previous':str(previous) if previous else None,'installed_sha256':hashlib.sha256(data).hexdigest(),'time':int(time.time()),'status':'pending'}
        self.data['files'].setdefault(str(target),[]).append(record); self.save()
        mode = target.stat().st_mode & 0o777 if target.exists() else 0o644
        try:
            atomic_bytes(target,data,mode)
        except OSError:
            record['status']='failed'; self.save(); raise
        record['status']='installed'; self.save()
        return True


@contextmanager
def serialized(home=None, timeout=1.0):
    directory = Path(home or Path.home()) / '.local/share/openrgb-game-sync/automation'
    directory.mkdir(parents=True,exist_ok=True)
    with (directory / 'lock').open('a') as lock:
        deadline=time.monotonic()+timeout
        while True:
            try:
                fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB); break
            except BlockingIOError:
                if time.monotonic() >= deadline: raise TimeoutError('La preparación RGB está ocupada')
                time.sleep(.02)
        try: yield
        finally: fcntl.flock(lock,fcntl.LOCK_UN)


def prefix_active(prefix):
    prefix = prefix.resolve()
    for proc in Path('/proc').iterdir():
        if not proc.name.isdigit(): continue
        if int(proc.name) == os.getpid(): continue
        try:
            executable = os.path.basename(os.readlink(proc/'exe')).lower()
            if 'wine' not in executable and not executable.endswith('.exe'): continue
            parts = dict(item.split(b'=',1) for item in (proc/'environ').read_bytes().split(b'\0') if b'=' in item)
            current = parts.get(b'WINEPREFIX')
            if current and Path(os.fsdecode(current)).resolve() == prefix: return True
        except (OSError,ValueError): pass
    return False


def sdk_files(game, store=None, force=False):
    game = game.resolve()
    key = str(game)
    cached = store.data.setdefault('scan_index',{}).get(key) if store else None
    if cached and not force and time.time()-cached['time'] < 600:
        return [Path(p) for p in cached['files'] if Path(p).is_file()]
    found, count = [], 0
    for directory, dirs, files in os.walk(game,followlinks=False):
        count += 1
        depth = len(Path(directory).relative_to(game).parts)
        dirs[:] = [d for d in dirs if d.lower() not in {'data','.git','node_modules','shadercache','gpuCache'.lower()} and depth < 6]
        for name in files:
            if SDK_NAMES.fullmatch(name): found.append(Path(directory)/name)
        if count > 4096: break
    if store:
        store.data['scan_index'][key] = {'time':time.time(),'files':[str(p) for p in found]}
        store.save()
    return found


def prepare_prefix(prefix, games, store, build=None, launch=False, registry_writer=None, force=False):
    from bridge_registration import registered_system_text
    prefix, build = Path(prefix).resolve(), Path(build or ROOT/'build')
    report = {'prefix':str(prefix),'changed':0,'deferred':[],'errors':[]}
    from .runtime import configuration
    try: policy=json.loads(configuration(store.home).read_text())
    except (OSError,ValueError): policy={}
    if str(prefix) in policy.get('blocked_prefixes',[]):
        report['deferred'].append('Integración suspendida para este entorno tras un fallo'); return report
    blocked_games={Path(p).resolve() for p in policy.get('blocked_games',[])}
    if not (prefix/'system.reg').is_file():
        report['deferred'].append('El lanzador todavía no ha creado el entorno'); return report
    active = prefix_active(prefix)
    if active and not launch:
        report['deferred'].append('Entorno en uso; se preparará antes del siguiente arranque'); return report
    try:
        registry = (prefix/'system.reg').read_text()
        is_32 = '#arch=win32' in registry
        for bits, directory in ([(32,'system32')] if is_32 else [(64,'system32'),(32,'syswow64')]):
            for family, filename in SOURCES[bits].items():
                source = build / filename
                if not source.is_file(): report['errors'].append('Falta el adaptador '+filename); continue
                target_name = 'LogitechLed.dll' if family == 'lightsync' else filename
                target = prefix/'drive_c/windows'/directory/target_name
                if target.exists() and not target.is_symlink():
                    architecture, actual = pe_info(target)
                    if actual != family or architecture != bits or not compatible_library(target,source)[0]:
                        report['errors'].append('Biblioteca no reconocida: '+str(target)); continue
                report['changed'] += store.install(target,source.read_bytes())
                if family == 'lightsync':
                    alias = target.with_name('LogitechLedEnginesWrapper.dll')
                    if alias.exists() and not compatible_library(alias,source)[0]:
                        report['errors'].append('Biblioteca no reconocida: '+str(alias))
                    else: report['changed'] += store.install(alias,source.read_bytes())
        if registry_writer:
            if not registry_writer(): report['errors'].append('El lanzador no pudo registrar Lightsync')
        elif not active:
            updated = registered_system_text(registry, include32=not is_32)
            if updated != registry: report['changed'] += store.install(prefix/'system.reg',updated.encode())
        else: report['deferred'].append('Registro ocupado; pendiente de la tarea previa al arranque')
        for game in set(Path(p).resolve() for p in games) | registry_games(prefix):
            if any(game==blocked or game.is_relative_to(blocked) or blocked.is_relative_to(game) for blocked in blocked_games):
                report['deferred'].append('Se conserva sin modificar el juego suspendido: '+str(game)); continue
            if not game.is_dir(): continue
            for target in sdk_files(game,store,force):
                if target.is_symlink(): continue
                try:
                    bits,family = pe_info(target)
                    if not family: report['errors'].append('SDK desconocido: '+str(target)); continue
                    source_name=SOURCES[bits].get(family)
                    if not source_name:
                        report['errors'].append('Arquitectura del SDK no soportada: '+str(target)); continue
                    source = build/source_name
                    if source.is_file():
                        compatible,reason=compatible_library(target,source)
                        if not compatible:
                            report['errors'].append('SDK conservado: '+str(target)+' ('+reason+')'); continue
                        report['changed'] += store.install(target,source.read_bytes())
                except (OSError,ValueError,struct.error,UnicodeError) as exc:
                    report['errors'].append(str(target)+': '+str(exc))
    except (OSError,ValueError,struct.error,UnicodeError) as exc:
        report['errors'].append(str(exc))
    return report
