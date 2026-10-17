import sys, os, tempfile, unittest, subprocess, time
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from automation.prepare import serialized, ROOT, compatible_library, prepare_prefix, Store
from automation.process import bounded_run
from unittest.mock import patch
import ast

class Hardening(unittest.TestCase):
 def test_proton_registry_path(self):
  tree=ast.parse((ROOT/'automation/launch_worker.py').read_text())
  value=next(n for n in ast.walk(tree) if isinstance(n,ast.JoinedStr))
  path=eval(compile(ast.Expression(value),'<test>','eval'),{'directory':'system32'})
  self.assertEqual(path,r'C:\windows\system32\LogitechLed.dll')
 def test_lock_timeout(self):
  with tempfile.TemporaryDirectory() as home:
   with serialized(home):
    code='from automation.prepare import serialized\nimport sys\ntry:\n with serialized(sys.argv[1],timeout=.1): pass\nexcept TimeoutError: sys.exit(0)\nsys.exit(1)'
    proc=subprocess.run([sys.executable,'-c',code,home],cwd=ROOT,timeout=2,capture_output=True)
    self.assertEqual(proc.returncode,0,proc.stderr.decode())
 def test_unknown_exports_preserved(self):
  with tempfile.TemporaryDirectory() as home:
   home=Path(home); prefix=home/'prefix'; prefix.mkdir(); (prefix/'system.reg').write_text('WINE REGISTRY Version 2\n#arch=win64\n')
   game=home/'game'; game.mkdir(); target=game/'LogitechLed.dll'
   data=(ROOT/'build/LogitechLed.dll').read_bytes().replace(b'LogiLedStopEffects\0',b'LogiLedNewFunction\0')
   target.write_bytes(data)
   self.assertFalse(compatible_library(target,ROOT/'build/LogitechLed.dll')[0])
   with patch('automation.prepare.prefix_active',return_value=False): report=prepare_prefix(prefix,[game],Store(home),force=True)
   self.assertEqual(target.read_bytes(),data); self.assertTrue(any('SDK conservado' in e for e in report['errors']))
 def test_process_total_timeout(self):
  start=time.monotonic()
  with self.assertRaises(subprocess.TimeoutExpired): bounded_run([sys.executable,'-c','import time; time.sleep(60)'],timeout=.1)
  self.assertLess(time.monotonic()-start,1)
 def test_crashed_game_is_not_reinstalled(self):
  from automation.runtime import configuration
  import json
  with tempfile.TemporaryDirectory() as home:
   home=Path(home);prefix=home/'prefix';prefix.mkdir();(prefix/'system.reg').write_text('WINE REGISTRY Version 2\n#arch=win64\n')
   config=configuration(home);config.parent.mkdir(parents=True);config.write_text(json.dumps({'enabled':True,'blocked_prefixes':[str(prefix)]}))
   report=prepare_prefix(prefix,[],Store(home),force=True,launch=True)
   self.assertEqual(report['changed'],0);self.assertTrue(report['deferred']);self.assertFalse((prefix/'drive_c').exists())
if __name__=='__main__': unittest.main()
