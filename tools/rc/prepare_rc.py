#!/usr/bin/env python3
"""Make upstream's mptrack.rc readable by llvm-rc on Linux.

The script is written for rc.exe on Windows, where paths use backslashes and
file names are case-insensitive ("res\\mptrack.ico" is res/MPTRACK.ICO). This
copies the script and rewrites every file path in it (#include lines and
file resources such as ICON, PNG, CURSOR) to the absolute path of the real
file. One macro is adjusted for the standard C preprocessor (see
MSVC_MACROS). Nothing else changes, so the .res is what rc.exe builds.

Usage: prepare_rc.py <input.rc> <output.rc> [<depfile>]
"""
import os
import re
import sys


def resolve(base, path):
    """Finds path (Windows spelling) under base, ignoring case."""
    parts = [p for p in path.replace('\\\\', '/').replace('\\', '/').split('/') if p]
    current = base
    for part in parts:
        if part == '..':
            current = os.path.dirname(current)
            continue
        if part == '.':
            continue
        candidate = os.path.join(current, part)
        if not os.path.exists(candidate):
            matches = [e for e in os.listdir(current) if e.lower() == part.lower()] if os.path.isdir(current) else []
            if not matches:
                return None
            candidate = os.path.join(current, matches[0])
        current = candidate
    return current


# rc.exe uses MSVC's preprocessor, which expands a macro argument before
# pasting it with ##; the standard preprocessor (clang, used by llvm-rc)
# pastes first. Give these macros the usual extra level of expansion.
MSVC_MACROS = {
    '#define MAKEHEX(c) 0x##c': '#define MAKEHEX_PASTE(c) 0x##c\n#define MAKEHEX(c) MAKEHEX_PASTE(c)',
}

INCLUDE = re.compile(r'^(\s*#\s*include\s+)"([^"]+)"(.*)$')
FILE_RESOURCE = re.compile(r'^(\s*\w+\s+(?:ICON|CURSOR|BITMAP|PNG|RCDATA|HTML|FONT|MESSAGETABLE|TEXTFILE|\d+)\s+(?:DISCARDABLE\s+)?)"([^"]+)"(.*)$')


def process(path, out_lines, deps, seen):
    base = os.path.dirname(os.path.abspath(path))
    with open(path, encoding='utf-8-sig', errors='surrogateescape') as f:
        lines = f.read().splitlines()
    for line in lines:
        for macro, replacement in MSVC_MACROS.items():
            if line.startswith(macro):
                line = replacement + line[len(macro):]
        m = INCLUDE.match(line)
        if m:
            target = resolve(base, m.group(2))
            if target is not None and target.lower().endswith('.rc2') and target not in seen:
                # Inline .rc2 files, so their own relative paths resolve
                # against their directory.
                seen.add(target)
                deps.append(target)
                out_lines.append('#line 1 "%s"' % target)
                process(target, out_lines, deps, seen)
                out_lines.append('')
                continue
            if target is not None:
                deps.append(target)
                line = '%s"%s"%s' % (m.group(1), target, m.group(3))
            out_lines.append(line)
            continue
        m = FILE_RESOURCE.match(line)
        if m:
            target = resolve(base, m.group(2))
            if target is None:
                sys.exit('prepare_rc.py: %s: resource file not found: %s' % (path, m.group(2)))
            deps.append(target)
            line = '%s"%s"%s' % (m.group(1), target, m.group(3))
        out_lines.append(line)


def main():
    source, output = sys.argv[1], sys.argv[2]
    out_lines, deps = [], [os.path.abspath(source)]
    process(source, out_lines, deps, set())
    text = '\n'.join(out_lines) + '\n'
    if not os.path.exists(output) or open(output, encoding='utf-8', errors='surrogateescape').read() != text:
        with open(output, 'w', encoding='utf-8', errors='surrogateescape') as f:
            f.write(text)
    if len(sys.argv) > 3:
        with open(sys.argv[3], 'w') as f:
            f.write('%s: %s\n' % (output, ' '.join(d.replace(' ', '\\ ') for d in deps)))


main()
