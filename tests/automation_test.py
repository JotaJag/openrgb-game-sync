import sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import json, os, tempfile, unittest
from types import SimpleNamespace
from unittest.mock import patch
from automation.discovery import discover, Inventory
from automation.prepare import Store, prepare_prefix, ROOT
from automation.hooks import install_hooks, proton_settings
from automation.proton_hook import install_hook
from automation.runtime import configuration, enabled

class AutomationTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory(); self.addCleanup(self.temp.cleanup)
        self.home=Path(self.temp.name); self.prefix=self.home/'prefix'; self.prefix.mkdir()
        (self.prefix/'system.reg').write_text('WINE REGISTRY Version 2\n#arch=win64\n')
        self.game=self.home/'Game With Spaces'; self.game.mkdir()
    def test_preparation_backup_update_idempotency(self):
        target=self.game/'CUESDK.x64_2015.dll'
        # An earlier version of a valid SDK is preserved before installing.
        original=(ROOT/'build/CUESDK.x64_2015.dll').read_bytes()+b'older version'
        target.write_bytes(original); unknown=self.game/'RzChromaSDK64.dll'; unknown.write_bytes(b'unknown')
        store=Store(self.home)
        with patch('automation.prepare.prefix_active',return_value=False):
            report=prepare_prefix(self.prefix,[self.game],store,force=True)
            self.assertGreater(report['changed'],0); self.assertEqual(unknown.read_bytes(),b'unknown')
            records=store.data['files'][str(target)]; self.assertEqual(Path(records[0]['previous']).read_bytes(),original)
            second=prepare_prefix(self.prefix,[self.game],store,force=True); self.assertEqual(second['changed'],0)
            target.write_bytes(original+b'update'); self.assertGreater(prepare_prefix(self.prefix,[self.game],store,force=True)['changed'],0)
        with patch('automation.prepare.prefix_active',return_value=True):
            self.assertTrue(prepare_prefix(self.prefix,[],store)['deferred'])
    def test_discovery_and_preserving_hooks(self):
        steam=self.home/'.local/share/Steam'; apps=steam/'steamapps'; apps.mkdir(parents=True)
        (apps/'appmanifest_123.acf').write_text('"AppState" { "appid" "123" "installdir" "Space Game" }')
        (apps/'common/Space Game').mkdir(parents=True)
        lutris=self.home/'.local/share/lutris'; lutris.mkdir(); (lutris/'system.yml').write_text('system:\n  gpu: card1\n  prelaunch_command: custom-task\n')
        heroic=self.home/'.config/heroic'; heroic.mkdir(parents=True); (heroic/'config.json').write_text(json.dumps({'defaultSettings':{'winePrefix':str(self.prefix)}}))
        bottles=self.home/'.local/share/bottles/bottles/Test'; bottles.mkdir(parents=True); (bottles/'bottle.yml').write_text('Name: Test\n')
        inv=discover(self.home); inv.tools=set()
        self.assertIn(str((apps/'compatdata/123/pfx').resolve()),inv.targets)
        self.assertIn(str(bottles.resolve()),inv.targets)
        report=install_hooks(inv,Store(self.home)); self.assertTrue(report['errors'])
        self.assertIn('custom-task',(lutris/'system.yml').read_text())
        self.assertIn('auto_launch.sh',(heroic/'config.json').read_text())
        prefs='user_settings = {"MY_SETTING": "preserved"}\n'
        result=proton_settings(prefs); self.assertEqual(result,proton_settings(result)); self.assertIn(prefs.strip(),result)
    def test_proton_before_game_and_disabled(self):
        config=configuration(self.home); config.parent.mkdir(parents=True); config.write_text('{"enabled": true}')
        calls=[]
        def run(*args):
            self.assertTrue((self.prefix/'drive_c/windows/syswow64/RzChromaSDK.dll').is_file()); calls.append('game'); return 7
        session=SimpleNamespace(env={'WINEPREFIX':str(self.prefix),'STEAM_COMPAT_INSTALL_PATH':str(self.game)},run=run,run_proc=lambda args: calls.append(args) or 0)
        main=SimpleNamespace(g_session=session,g_proton=SimpleNamespace(wine_bin='wine'),g_compatdata=SimpleNamespace(prefix_dir=str(self.prefix)))
        with patch.dict(os.environ,HOME=str(self.home)), patch('automation.prepare.prefix_active',return_value=False), patch('automation.process.bounded_run',side_effect=lambda *a,**kw: prepare_prefix(self.prefix,[self.game],Store(self.home)) and 0) as worker:
            self.assertTrue(enabled()); self.assertTrue(install_hook(main)); self.assertEqual(session.run(),7)
            self.assertEqual(calls[-1],'game'); self.assertEqual(worker.call_count,1)
            config.write_text('{"enabled": false}'); calls.clear(); self.assertEqual(session.run(),7); self.assertEqual(calls,['game'])

if __name__=='__main__': unittest.main()
