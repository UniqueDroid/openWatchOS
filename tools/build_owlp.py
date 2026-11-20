#!/usr/bin/env python3
"""Builds an OWLP program (launcher/*.c) in the format that launch() of the watch's JS engine accepts.

OWLP = 20-byte header (little endian) + code:
    "OWLP" | version | code length | CRC-32 of the code (zlib) | entry offset
Version 1: the watch turns interrupts off and the program never returns (it restarts the chip).
Version 2: interrupts stay on and the program returns to the app (launch() gives 0).
Versions 3-5 are diagnostics of the launcher (dry run, internal RAM, uncached PSRAM alias).
The code must be position independent: the watch copies it into a block of its PSRAM heap (malloc_psram) and
jumps to the entry (offset of owl_entry, in the header). So: one section .text.entry, no relocations, no data
sections. Constant data goes into the code section (launcher/owl_font.h, OWL_BLOB). The script refuses
anything else.

    JL_TOOLCHAIN=/path/to/jieli-toolchain/bin python3 tools/build_owlp.py launcher/hw_ui.c --version 2
        [--out x.owl] [-D NAME] [--app app.js --name P_UI]

Needs the JieLi LLVM toolchain for pi32v2 (clang, objdump, objcopy); JL_TOOLCHAIN points to its bin directory.

Writes <name>.owl next to the source and prints the hex string; with --app it replaces the line
`var PAYLOAD = '...';` in that JS file.
"""
import argparse, re, struct, subprocess, sys, tempfile, zlib
from pathlib import Path
import os

TOOLS = Path(os.environ.get('JL_TOOLCHAIN', '/opt/jieli/common/bin'))
MAX_CODE = 16384   # launcher limit of the modified firmware 4.50.86 and later (2048 before)


def build(source, version=1, defines=()):
    with tempfile.TemporaryDirectory() as tmp:
        obj, raw = Path(tmp) / 'p.o', Path(tmp) / 'p.bin'
        subprocess.run([TOOLS / 'clang', '-target', 'pi32v2', '-mcpu=r3', '-Os', '-ffreestanding', '-fno-builtin',
                        '-ffunction-sections', '-fdata-sections', '-Wall', '-Wextra', '-Werror', *[f'-D{d}' for d in defines], '-c', str(source),
                        '-o', str(obj)], check=True)
        heads = subprocess.run([TOOLS / 'objdump', '-h', '-r', str(obj)], check=True, capture_output=True, text=True).stdout
        sections = {m.group(1): int(m.group(2), 16) for m in re.finditer(r'^\s*\d+\s+(\.\S+)\s+([0-9a-f]{8})', heads, re.M)}
        loaded = {k: v for k, v in sections.items() if v and k.startswith(('.text', '.data', '.rodata', '.bss'))}
        if set(loaded) != {'.text.entry'}:
            sys.exit(f'nur .text.entry erlaubt, gefunden: {loaded}')
        if 'RELOCATION RECORDS' in heads:
            sys.exit('Relokationen gefunden: der Code ist nicht lageunabhängig\n' + heads)
        symbols = subprocess.run([TOOLS / 'objdump', '-t', str(obj)], check=True, capture_output=True, text=True).stdout
        entry = re.search(r'^([0-9a-f]{8})\s.*\.text\.entry\s+[0-9a-f]+\s+owl_entry$', symbols, re.M)
        if not entry: sys.exit('owl_entry nicht in .text.entry gefunden\n' + symbols)
        subprocess.run([TOOLS / 'objcopy', '-O', 'binary', '-j', '.text.entry', str(obj), str(raw)], check=True)
        code = raw.read_bytes()
    if not 0 < len(code) <= MAX_CODE: sys.exit(f'Code {len(code)} Byte, erlaubt 1..{MAX_CODE}')
    return struct.pack('<4sIIII', b'OWLP', version, len(code), zlib.crc32(code), int(entry.group(1), 16)) + code


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('source', type=Path)
    ap.add_argument('--out', type=Path)
    ap.add_argument('--version', type=int, choices=(1, 2, 3, 4, 5), default=1)
    ap.add_argument('-D', dest='defines', action='append', default=[])
    ap.add_argument('--app', type=Path, help="JS-Datei mit der Zeile var NAME = '...';")
    ap.add_argument('--name', default='PAYLOAD')
    a = ap.parse_args()
    blob = build(a.source, a.version, a.defines)
    out = a.out or a.source.with_suffix('.owl'); out.write_bytes(blob)
    text = blob.hex()
    print(f'{out}: {len(blob)} Byte, CRC {zlib.crc32(blob[20:]):08x}')
    if a.app:
        js = a.app.read_text(); new, n = re.subn(rf"var {a.name} = '[0-9a-f]*';", f"var {a.name} = '{text}';", js)
        if n != 1: sys.exit(f"{a.app}: Zeile var {a.name} = '...'; nicht genau einmal gefunden")
        a.app.write_text(new); print(f'{a.app}: {a.name} eingesetzt')
    else:
        print(text)


if __name__ == '__main__':
    main()
