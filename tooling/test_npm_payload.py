#!/usr/bin/env python3
"""Contract checks for production dependency selection and preserved payloads."""
import json
from pathlib import Path
import tempfile
import unittest
from npm_payload import copy_dependencies

class Payload(unittest.TestCase):
    def test_production_and_all(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);source=root/'node_modules';source.mkdir()
            entries={'':{},'node_modules/build-tool':{'version':'1','dev':True},'node_modules/shared':{'version':'1'},'node_modules/@scope/runtime':{'version':'1'},'node_modules/shared/node_modules/child':{'version':'1'},'node_modules/shared/node_modules/dev':{'version':'1','dev':True},'node_modules/optional-shared':{'version':'1','devOptional':True},'node_modules/absent':{'version':'1','optional':True}}
            for name,entry in entries.items():
                if not name or name.endswith('/absent'):continue
                package=root/name;package.mkdir(parents=True,exist_ok=True)
                (package/'package.json').write_text(json.dumps({'version':'1'}));(package/'LICENSE').write_text('notice');(package/'worker.js').write_text('worker')
            lock=root/'lock.json';lock.write_text(json.dumps({'lockfileVersion':3,'packages':entries}))
            info=copy_dependencies(source,root/'production',lock)
            self.assertEqual(info,{'mode':'production','packages':4,'omittedDevPackages':2})
            self.assertFalse((root/'production/build-tool').exists());self.assertFalse((root/'production/shared/node_modules/dev').exists())
            for path in ['shared/LICENSE','shared/node_modules/child/worker.js','@scope/runtime/LICENSE','optional-shared/worker.js']:self.assertTrue((root/'production'/path).is_file())
            self.assertEqual(copy_dependencies(source,root/'all',lock,'all')['packages'],6)
            self.assertTrue((root/'all/build-tool/LICENSE').is_file())
    def test_invalid_or_missing(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);source=root/'installed';source.mkdir();lock=root/'lock.json'
            for packages in [{'../escape':{}},{'node_modules/missing':{'version':'1'}},{'node_modules/link':{'link':True}}]:
                lock.write_text(json.dumps({'lockfileVersion':3,'packages':packages}))
                with self.assertRaises(ValueError):copy_dependencies(source,root/str(len(list(root.iterdir()))),lock)
            lock.write_text(json.dumps({'lockfileVersion':1,'packages':{}}))
            with self.assertRaises(ValueError):copy_dependencies(source,root/'invalid',lock)

if __name__=='__main__':unittest.main()
