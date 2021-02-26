#!/usr/bin/env python3

from pathlib import Path
import re
import os
import io
import sys

dest_path = Path(sys.argv[1])
exceptions = set(sys.argv[2].split(",") if 2 < len(sys.argv) else [])

cxx_suffixes = {".cpp", ".cxx", ".c", ".hpp", ".hxx", ".h"}
out_paths = set()
for dir_, _, files in os.walk(os.getcwd()):
    for f in files:
        f = Path(dir_, f)
        if f == dest_path or f.suffix not in cxx_suffixes:
            continue
        for line in open(f, mode='r'):
            s = re.split(r'#include.*<(.*?)>', line)
            if len(s) == 1:
                continue
            p = s[1]
            if all(not p.startswith(e) for e in exceptions):
                out_paths.add(p)
buf = io.StringIO()
dest_reader = open(dest_path, mode='r') if dest_path.exists() else None
for p in sorted(out_paths):
    s = f'#include <{p}>\n'
    buf.write(s)
    if not dest_reader:
        continue
    try:
        if next(dest_reader) == s:
            continue
    except StopIteration:
        pass
    dest_reader.close()
    dest_reader = None
if dest_reader:
    dest_reader.close()
    print(f"PCH file is up to date: {dest_path}")
else:
    with open(dest_path, mode='w') as f:
        f.write(buf.getvalue())
        print(f"updated PCH file: {dest_path}")
