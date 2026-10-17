import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location('configure_rest',Path(__file__).resolve().parents[1]/'scripts/configure-rest.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)

class RestConfigurationTests(unittest.TestCase):
    def test_rest_changes_preserve_exclusions_and_services_and_pause_preparation(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory)
            (root/'chroma_rest.py').write_text('# runtime')
            config=root/'settings.json'
            initial={'enabled':True,'rest':False,'script':str(root/'auto_prepare.py'),
                     'blocked_prefixes':['/example'],'pipe_services':[{'prefix':'/prefix'}]}
            config.write_text(json.dumps(initial))
            backup=module.configure(config,True)
            self.assertEqual(json.loads(backup.read_text()),initial)
            actual=json.loads(config.read_text())
            self.assertFalse(actual['enabled'])
            self.assertTrue(actual['rest'])
            self.assertEqual(actual['pipe_services'],initial['pipe_services'])
            self.assertEqual(actual['blocked_prefixes'],initial['blocked_prefixes'])
            module.configure(config,False)
            self.assertFalse(json.loads(config.read_text())['rest'])

    def test_missing_runtime_does_not_change_configuration(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);config=root/'settings.json'
            original=json.dumps({'enabled':False,'script':str(root/'auto_prepare.py')})
            config.write_text(original)
            with self.assertRaisesRegex(ValueError,'runtime'):
                module.configure(config,True)
            self.assertEqual(config.read_text(),original)

if __name__=='__main__':
    unittest.main()
