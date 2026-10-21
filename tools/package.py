"""Create and verify ASI-only and optional-loader ZIPs; standard library only."""
import hashlib
import json
from pathlib import Path
import urllib.request
import zipfile

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / 'build'

def sha(data):
    return hashlib.sha256(data).hexdigest()

def main():
    meta = json.loads((ROOT / 'tools/loader.json').read_text())
    archive = BUILD / 'loader-upstream.zip'
    if not archive.exists():
        urllib.request.urlretrieve(meta['url'], archive)
    if sha(archive.read_bytes()) != meta['archive_sha256']:
        raise ValueError('Loader archive SHA256 mismatch')
    with zipfile.ZipFile(archive) as z:
        loader = z.read('dinput8.dll')
    if sha(loader) != meta['dll_sha256']:
        raise ValueError('Loader DLL SHA256 mismatch')
    asi = (BUILD / 'SendToStash.asi').read_bytes()
    audit = json.loads((BUILD / 'static-verification.json').read_text())
    if sha(asi) != audit['asi_sha256']:
        raise ValueError('ASI differs from audited build')
    out = BUILD / 'packages'
    out.mkdir(exist_ok=True)
    common = {
        'SendToStash.asi': asi,
        'README.md': (ROOT / 'README.md').read_bytes(),
        'MinHook-LICENSE.txt': (ROOT / 'third_party/minhook/LICENSE.txt').read_bytes(),
    }
    archives = []
    for variant, bundle in [('ASI-only', False), ('ASI-with-loader', True)]:
        files = dict(common)
        if bundle:
            files['winhttp.dll'] = loader
            files['Ultimate-ASI-Loader-LICENSE.txt'] = (ROOT / 'third_party/ultimate-asi-loader/LICENSE.txt').read_bytes()
            files['Ultimate-ASI-Loader-UPSTREAM.md'] = (ROOT / 'third_party/ultimate-asi-loader/UPSTREAM.md').read_bytes()
        files['SHA256SUMS.txt'] = ''.join(sha(data) + '  ' + name + '\n' for name, data in sorted(files.items())).encode('ascii')
        path = out / ('SendToStash-' + variant + '.zip')
        with zipfile.ZipFile(path, 'w', zipfile.ZIP_DEFLATED) as z:
            for name, data in sorted(files.items()):
                info = zipfile.ZipInfo(name, date_time=(2026, 10, 21, 0, 0, 0))
                info.compress_type = zipfile.ZIP_DEFLATED
                info.external_attr = 0o644 << 16
                z.writestr(info, data)
        with zipfile.ZipFile(path) as z:
            expected_runtime = {'SendToStash.asi', 'winhttp.dll'} if bundle else {'SendToStash.asi'}
            actual_runtime = {name for name in z.namelist() if name.endswith(('.asi', '.dll'))}
            if z.testzip() or set(z.namelist()) != set(files) or actual_runtime != expected_runtime:
                raise ValueError('ZIP content verification failed')
            for name, data in files.items():
                if z.read(name) != data:
                    raise ValueError('ZIP payload mismatch: ' + name)
        archives.append(path)
        print('Verified ' + path.name + ': ' + ', '.join(sorted(expected_runtime)))
    (out / 'SHA256SUMS.txt').write_text(''.join(sha(path.read_bytes()) + '  ' + path.name + '\n' for path in archives), encoding='ascii')

if __name__ == '__main__':
    main()
