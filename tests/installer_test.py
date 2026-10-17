"""Exercise the real installer transaction against a temporary user directory."""
import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
from types import SimpleNamespace

spec = importlib.util.spec_from_file_location('installer', Path(__file__).resolve().parents[1]/'scripts/install-user.py')
installer = importlib.util.module_from_spec(spec)
spec.loader.exec_module(installer)

class InstallerTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.payload = self.root/'payload'
        (self.payload/'plugins').mkdir(parents=True)
        (self.payload/'runtime/build').mkdir(parents=True)
        (self.payload/'runtime/auto_prepare.py').write_text('# runtime')
        (self.payload/'runtime/build/cue-pipe-host.exe').write_bytes(b'host')
        for name in ['libOpenRGBGameSyncPlugin.so', 'libOpenRGBEffectsPlugin.so']:
            (self.payload/'plugins'/name).write_bytes(b'new plugin')
        self.hashes()
        self.config = self.root/'config'
        self.data = self.root/'data'
        self.plugins = self.config/'plugins'
        self.plugins.mkdir(parents=True)
        self.old = self.plugins/'OpenRGBEffectsPlugin_old.so'
        self.old.write_bytes(b'old plugin')
        self.settings = self.plugins/'settings/GameSyncAutomation.json'
        self.settings.parent.mkdir()
        self.original = b'{"enabled":true,"blocked_prefixes":["/example"]}'
        self.settings.write_bytes(self.original)
        mock = patch.object(installer.subprocess, 'run', return_value=SimpleNamespace(returncode=0, stdout='', stderr=''))
        mock.start()
        self.addCleanup(mock.stop)

    def hashes(self):
        hashes = {str(p.relative_to(self.payload)): hashlib.sha256(p.read_bytes()).hexdigest()
                  for p in self.payload.rglob('*') if p.is_file() and p.name != 'SHA256.json'}
        (self.payload/'SHA256.json').write_text(json.dumps(hashes))

    def test_install_and_restore_plugins_and_configuration(self):
        installer.install(self.payload, self.config, self.data)
        self.assertFalse(self.old.exists())
        cfg = json.loads(self.settings.read_text())
        self.assertFalse(cfg['enabled'])
        self.assertEqual(cfg['pipe_services'], [])
        self.assertEqual(cfg['blocked_prefixes'], ['/example'])
        self.assertTrue(Path(cfg['script']).is_file())
        manifest = self.data/'installation.json'
        installer.restore(manifest, json.loads(manifest.read_text()))
        self.assertEqual(self.old.read_bytes(), b'old plugin')
        self.assertEqual(self.settings.read_bytes(), self.original)
        self.assertFalse((self.plugins/'libOpenRGBGameSyncPlugin.so').exists())

    def test_changed_file_blocks_uninstall_before_any_changes(self):
        installer.install(self.payload, self.config, self.data)
        self.settings.write_text('{"user_changed":true}')
        manifest = self.data/'installation.json'
        with self.assertRaisesRegex(RuntimeError, 'cambió'):
            installer.restore(manifest, json.loads(manifest.read_text()))
        self.assertTrue((self.plugins/'libOpenRGBGameSyncPlugin.so').is_file())
        self.assertFalse(self.old.exists())

    def test_tampered_payload_does_not_modify_existing_plugins(self):
        (self.payload/'plugins/libOpenRGBGameSyncPlugin.so').write_bytes(b'tampered')
        with self.assertRaisesRegex(RuntimeError, 'alterado'):
            installer.install(self.payload, self.config, self.data)
        self.assertEqual(self.old.read_bytes(), b'old plugin')
        self.assertFalse(self.data.exists())

    def test_missing_libraries_does_not_modify_existing_plugins(self):
        with patch.object(installer.subprocess, 'run', return_value=SimpleNamespace(returncode=0, stdout='Qt6 => not found', stderr='')):
            with self.assertRaisesRegex(RuntimeError, 'Dependencias'):
                installer.install(self.payload, self.config, self.data)
        self.assertEqual(self.old.read_bytes(), b'old plugin')
        self.assertFalse(self.data.exists())

    def test_failed_install_restores_previous_files(self):
        original_write = Path.write_bytes
        def fail_plugin(path, content):
            if path.name == 'libOpenRGBGameSyncPlugin.so' and path.parent == self.plugins:
                raise OSError('simulated write failure')
            return original_write(path, content)
        with patch.object(Path, 'write_bytes', fail_plugin):
            with self.assertRaisesRegex(OSError, 'simulated'):
                installer.install(self.payload, self.config, self.data)
        self.assertEqual(self.old.read_bytes(), b'old plugin')
        self.assertEqual(self.settings.read_bytes(), self.original)
        self.assertFalse((self.data/'installation.json').exists())

if __name__ == '__main__':
    unittest.main()
