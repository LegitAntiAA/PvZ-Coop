"""Verify that staged Fusion resources preserve the runtime package bytes."""
import hashlib
from pathlib import Path
import subprocess


def main():
    root = Path(__file__).resolve().parents[2]
    object_format = subprocess.check_output(
        ['git', 'rev-parse', '--show-object-format'], cwd=root, text=True).strip()
    if object_format != 'sha1':
        raise RuntimeError(f'Unsupported Git object format: {object_format}')
    index = subprocess.check_output(
        ['git', 'ls-files', '--stage', '-z', '--', 'assets/fusion'], cwd=root)
    checked = 0
    mismatches = []
    for entry in index.split(b'\0'):
        if not entry:
            continue
        metadata, encoded_path = entry.split(b'\t', 1)
        mode, expected, stage = metadata.split()
        path = encoded_path.decode('utf-8')
        if stage != b'0' or mode not in (b'100644', b'100755'):
            raise RuntimeError(f'Unmerged or non-regular Fusion asset: {path}')
        data = (root / path).read_bytes()
        digest = hashlib.sha1(usedforsecurity=False)
        digest.update(f'blob {len(data)}\0'.encode('ascii'))
        digest.update(data)
        checked += 1
        if digest.hexdigest() != expected.decode('ascii'):
            mismatches.append(path)
    if not checked:
        raise RuntimeError('No staged Fusion assets were checked')
    print(f'Fusion asset Git bytes: checked={checked} mismatches={len(mismatches)}')
    for path in mismatches[:20]:
        print(f'  staged bytes differ: {path}')
    return int(bool(mismatches))


if __name__ == '__main__':
    raise SystemExit(main())
