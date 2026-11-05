"""Generate the browser + Mite host manifest outside the upstream checkout."""
import json
from pathlib import Path


def write_json(path, value):
    text = json.dumps(value, indent=2) + '\n'
    if not path.is_file() or path.read_text() != text:
        path.write_text(text)


def manifest_data(root: Path):
    upstream = root / 'vendor/ladybird'
    manifest = json.loads((upstream / 'vcpkg.json').read_text())
    mite = json.loads((root / 'vcpkg.json').read_text())
    if manifest['builtin-baseline'] != mite['builtin-baseline']:
        raise ValueError('Browser and Mite dependency baselines must match')
    names = {item if isinstance(item, str) else item['name'] for item in manifest['dependencies']}
    # The browser already provides the shared engine dependencies. These two are
    # Mite's native I/O/protocol services, not additions to Ladybird's source.
    for name in ['libuv', 'llhttp']:
        if name not in names:
            manifest['dependencies'].append(name)
        override = next((item for item in mite.get('overrides', []) if item['name'] == name), None)
        if override:
            overrides = manifest.setdefault('overrides', [])
            existing = next((item for item in overrides if item['name'] == name), None)
            if existing and existing != override:
                raise ValueError(f'Conflicting {name} pins; resolve in our build adapter')
            if not existing:
                overrides.append(override)
    configuration = json.loads((upstream / 'vcpkg-configuration.json').read_text())
    for key in ['overlay-ports', 'overlay-triplets']:
        if key in configuration:
            configuration[key] = [str((upstream / item).resolve()) for item in configuration[key]]
    return {'vcpkg.json':manifest, 'vcpkg-configuration.json':configuration}


def prepare(root: Path, destination: Path):
    files=manifest_data(root)
    destination.mkdir(parents=True, exist_ok=True)
    for name,value in files.items():
        write_json(destination/name,value)
    return destination


def check(root: Path, destination: Path):
    for name,expected in manifest_data(root).items():
        path=destination/name
        if not path.is_file() or json.loads(path.read_text())!=expected:
            raise ValueError('Stale Firefly dependency manifest; run tooling/build_renderer.py --target firefly')


if __name__=='__main__':
    import argparse
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check',required=True,type=Path)
    args=parser.parse_args()
    check(Path(__file__).resolve().parents[1],args.check)
    print('Current browser + Mite dependency manifest verified')
