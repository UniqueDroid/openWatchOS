#!/bin/sh
# Regenerates the engine tables from hw3_stdlib.c: generated/pi32v2 (watch, 32 bit) and generated/host64 (PC tests).
set -e
cd "$(dirname "$0")"
# The tool prints "Too many properties, consider increasing ATOM_ALIGN" (so does the unchanged table of the engine).
tool=$(mktemp)
cc -O1 -D_GNU_SOURCE -Imquickjs hw3_stdlib.c mquickjs/mquickjs_build.c -o "$tool"
"$tool" -m32 > generated/pi32v2/hw3_stdlib.h
"$tool" -a -m32 > generated/pi32v2/mquickjs_atom.h
"$tool" > generated/host64/hw3_stdlib.h
"$tool" -a > generated/host64/mquickjs_atom.h
rm -f "$tool"
