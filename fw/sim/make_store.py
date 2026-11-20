#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Builds an app store folder from .oapps packages: the packages as <id>.oapps and catalog.json (the format the
watch's store reads, fw/apps/builtin/store.c). The phone app serves the same catalogue.

    make_store.py OUT PACKAGE.oapps ...
"""
import io, json, shutil, sys, tarfile
from pathlib import Path

def main(argv):
    if len(argv) < 3:
        print(__doc__); return 2
    out = Path(argv[1]); out.mkdir(parents=True, exist_ok=True); apps = []
    for p in map(Path, argv[2:]):
        data = p.read_bytes()
        with tarfile.open(fileobj=io.BytesIO(data), mode='r:') as t:
            app = json.loads(t.extractfile('app.json').read().decode('utf-8'))
        if app.get('type', 'app') != 'app': continue
        entry = {k: app[k] for k in ('id', 'name', 'version', 'author', 'description') if k in app}
        entry['size'] = len(data); entry['permissions'] = app.get('permissions', [])
        apps.append(entry); (out / (app['id'] + '.oapps')).write_bytes(data)
    apps.sort(key=lambda a: a['name'].lower())
    (out / 'catalog.json').write_text(json.dumps({'apps': apps}, ensure_ascii=False, indent=1), encoding='utf-8')
    print(f'{len(apps)} Apps in {out}/catalog.json')
    return 0

if __name__ == '__main__':
    sys.exit(main(sys.argv))
