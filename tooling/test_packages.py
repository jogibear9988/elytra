#!/usr/bin/env python3
"""Build isolated npm-style trees and compare resolution with real Node."""
import argparse
import json
from pathlib import Path
import shutil
import sys
import tempfile
from test_runtime import run


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mite', required=True)
    parser.add_argument('--node', default=shutil.which('node'))
    args = parser.parse_args()
    if not args.node:
        parser.error('Node is required for differential tests')
    with tempfile.TemporaryDirectory(prefix='mite-packages-') as temporary:
        root = Path(temporary)

        def write(name, text):
            file = root / name
            file.parent.mkdir(parents=True, exist_ok=True)
            file.write_text(text, encoding='utf-8')

        def config(name, value):
            write(name, json.dumps(value))

        def package(name, value, files):
            base = f'node_modules/{name}/'
            config(base + 'package.json', value)
            for file, text in files.items():
                write(base + file, text)

        config('package.json', {
            'name': 'app-self', 'type': 'commonjs',
            'exports': {'.': './self.cjs', './read': './read.cjs'},
            'imports': {'#local': './local.cjs', '#dual': 'dual',
                        '#wild/*': './aliases/*.cjs', '#blocked': None},
        })
        for file, label in [('self.cjs', 'self'), ('read.cjs', 'read'),
                            ('local.cjs', 'local'), ('aliases/x.cjs', 'alias')]:
            write(file, f'module.exports = {json.dumps(label)};')
        package('dual', {'exports': {'.': {'import': './esm.mjs', 'require': './cjs.cjs'}}},
                {'esm.mjs': 'export default "import";', 'cjs.cjs': 'module.exports = "require";'})
        package('@test/scoped', {'exports': {'.': './root.cjs', './features/*': './lib/*.cjs',
                                            './features/*.js': './special/*.cjs', './features/special/*': './prefix/*.cjs',
                                            './features/exact': './exact.cjs'}},
                {'root.cjs': 'module.exports="scoped";', 'lib/foo.cjs': 'module.exports="general";',
                 'special/foo.cjs': 'module.exports="specific";',
                 'prefix/foo.cjs': 'module.exports="prefix";', 'exact.cjs': 'module.exports="exact";'})
        package('legacy', {'main': './main.cjs'},
                {'main.cjs': 'module.exports="main";', 'sub.js': 'module.exports="subpath";'})
        package('ordered', {'exports': {'default': './default.cjs', 'node': './node.cjs'}},
                {'default.cjs': 'module.exports="default-first";', 'node.cjs': 'module.exports="WRONG";'})
        package('nested', {'exports': {'node': {'browser': './wrong.cjs'},
                                      'require': './cjs.cjs', 'import': './esm.mjs'}},
                {'cjs.cjs': 'module.exports="nested-require";', 'esm.mjs': 'export default "nested-import";'})
        package('array', {'exports': [123, '../outside.cjs', None, {'browser': './wrong.cjs'}, './ok.cjs']},
                {'ok.cjs': 'module.exports="array";'})
        package('array-missing', {'exports': ['./missing.cjs', './ok.cjs']},
                {'ok.cjs': 'module.exports="WRONG";'})
        package('nullable', {'exports': None, 'main': './ok.cjs'}, {'ok.cjs': 'module.exports="nullable";'})
        package('locked', {'exports': {
            '.': './public.cjs', './blocked': None, './bad': '../outside.cjs',
            './dots': './%2e%2e/outside.cjs', './modules': './node_modules/evil.cjs',
            './encoded-modules': './%6eode_modules/evil.cjs', './features/*': './lib/*.cjs',
            './missing': './extensionless', './bad-type': 42,
        }}, {'public.cjs': 'module.exports="public";', 'private.cjs': 'module.exports="WRONG";',
             'extensionless.js': 'module.exports="WRONG extension fallback";',
             'lib/a/b.cjs': 'module.exports="WRONG encoded separator";'})
        package('root-type', {'exports': 42}, {})
        package('mixed', {'exports': {'.': './ok.cjs', 'default': './ok.cjs'}}, {})
        package('numeric', {'exports': {'0': './ok.cjs', 'default': './ok.cjs'}}, {})
        package('nearest', {'main': './main.cjs'}, {'main.cjs': 'module.exports="outer";'})
        config('src/node_modules/nearest/package.json', {'main': './main.cjs'})
        write('src/node_modules/nearest/main.cjs', 'module.exports="inner";')
        package('shadow', {'main': './main.cjs'}, {'main.cjs': 'module.exports="WRONG outer fallback";'})
        config('src/node_modules/shadow/package.json', {'exports': {'.': None}})
        config('node_modules/broken/package.json', {'main': './index.cjs'})
        write('node_modules/broken/package.json', '{bad JSON')

        write('node_modules/no-config/index.js', "module.exports = () => { try { require('#local'); } catch(e) { return e.code; } };")
        write('main.cjs', '''
console.log(require('dual'), require('@test/scoped'));
console.log(require('@test/scoped/features/foo'), require('@test/scoped/features/foo.js'));
console.log(require('@test/scoped/features/special/foo'), require('@test/scoped/features/exact'));
console.log(require('no-config')());
console.log(require('legacy'), require('legacy/sub'));
console.log(require('#local'), require('#wild/x'), require('#dual'));
console.log(require('app-self'), require('app-self/read'));
console.log(require('ordered'), require('nested'), require('array'), require('nullable'));
console.log(require.resolve('dual').endsWith('cjs.cjs'), module.paths[0] === __dirname + '/node_modules');
''')
        write('main.mjs', '''
import dual from 'dual';
import scoped from '@test/scoped';
import specific from '@test/scoped/features/foo.js';
import legacy from 'legacy';
import subpath from 'legacy/sub.js';
import local from '#local';
import alias from '#wild/x';
import external from '#dual';
import self from 'app-self';
import read from 'app-self/read';
import nested from 'nested';
import array from 'array';
import ordered from 'ordered';
console.log(dual, scoped, specific, legacy, subpath);
console.log(local, alias, external, self, read);
console.log(nested, array, ordered);
''')
        write('src/deep/main.cjs', '''
console.log(require('nearest'));
try { require('shadow'); } catch (e) { console.log(e.code); }
''')
        write('src/deep/main.mjs', '''
import nearest from 'nearest';
console.log(nearest);
try { await import('shadow'); } catch (e) { console.log(e.code); }
''')
        negative = ['locked/private.cjs', 'locked/blocked', 'locked/bad', 'locked/dots',
                    'locked/modules', 'locked/encoded-modules', 'locked/features/../secret',
                    'locked/features/a%2fb', 'locked/missing', 'locked/bad-type',
                    'mixed', 'numeric', 'broken', 'root-type', 'array-missing', '#blocked', '#unknown', '#', '#/bad']
        write('errors.cjs', 'for (const request of ' + json.dumps(negative) + ''') {
try { require(request); console.log('WRONG accepted'); }
catch (error) { console.log(error.name, error.code); }
}''')
        write('errors.mjs', 'for (const request of ' + json.dumps(negative) + ''') {
try { await import(request); console.log('WRONG accepted'); }
catch (error) { console.log(error.name, error.code); }
}''')
        error_codes = ['ERR_PACKAGE_PATH_NOT_EXPORTED'] * 2 + ['ERR_INVALID_PACKAGE_TARGET'] * 4
        error_codes += ['ERR_INVALID_MODULE_SPECIFIER'] * 2 + ['MISSING', 'ERR_INVALID_PACKAGE_TARGET']
        error_codes += ['ERR_INVALID_PACKAGE_CONFIG'] * 3 + ['MISSING', 'MISSING']
        error_codes += ['ERR_PACKAGE_IMPORT_NOT_DEFINED'] * 2 + ['ERR_INVALID_MODULE_SPECIFIER', 'ERR_PACKAGE_IMPORT_NOT_DEFINED']
        def error_output(missing):
            return ''.join(('TypeError' if code in {'ERR_INVALID_MODULE_SPECIFIER', 'ERR_PACKAGE_IMPORT_NOT_DEFINED'} else 'Error')
                           + ' ' + (missing if code == 'MISSING' else code) + '\n' for code in error_codes)

        cases = [
            ('main.cjs', 'require scoped\ngeneral specific\nprefix exact\nMODULE_NOT_FOUND\nmain subpath\nlocal alias require\n'
             'self read\ndefault-first nested-require array nullable\ntrue true\n'),
            ('main.mjs', 'import scoped specific main subpath\nlocal alias import self read\n'
             'nested-import array default-first\n'),
            ('src/deep/main.cjs', 'inner\nERR_PACKAGE_PATH_NOT_EXPORTED\n'),
            ('src/deep/main.mjs', 'inner\nERR_PACKAGE_PATH_NOT_EXPORTED\n'),
            ('errors.cjs', error_output('MODULE_NOT_FOUND')),
            ('errors.mjs', error_output('ERR_MODULE_NOT_FOUND')),
        ]
        failures = 0
        for name, expected in cases:
            node = run(args.node, root / name)
            mite = run(args.mite, root / name)
            passed = (node.returncode == mite.returncode == 0 and node.stdout == mite.stdout == expected
                      and node.stderr == mite.stderr == '')
            print(f"{'PASS' if passed else 'FAIL'} packages/{name}")
            if not passed:
                failures += 1
                print(f'Node: {node}\nMite: {mite}\nExpected: {expected!r}', file=sys.stderr)
        package('fs', {'main': './index.cjs'}, {'index.cjs': 'module.exports="WRONG shadow";'})
        write('builtin.cjs', "try { console.log(typeof require('fs').readFileSync); } catch(e) { console.log(e.code); }")
        write('builtin.mjs', "try { console.log(typeof (await import('fs')).readFileSync); } catch(e) { console.log(e.code); }")
        for name in ['builtin.cjs', 'builtin.mjs']:
            node = run(args.node, root / name)
            mite = run(args.mite, root / name)
            passed = (node.returncode == mite.returncode == 0 and node.stdout == 'function\n'
                      and mite.stdout == 'ERR_MITE_UNSUPPORTED_BUILTIN\n' and node.stderr == mite.stderr == '')
            print(f"{'PASS' if passed else 'FAIL'} explicit unsupported built-in: {name}")
            if not passed:
                failures += 1
                print(f'Node: {node}\nMite: {mite}', file=sys.stderr)
        return int(bool(failures))


if __name__ == '__main__':
    sys.exit(main())
