#!/usr/bin/env python3
"""A resource compiler for upstream's mptrack.rc: builds the same binary
.res file as rc.exe on Windows (the format FindResource() and friends read).

Neither llvm-rc nor windres reads the whole script (llvm-rc rejects MENU in
DIALOGEX, windres rejects DLGINIT), so this covers the resource statements
that Visual Studio writes:

  DIALOG, DIALOGEX (with every control statement), MENU, MENUEX,
  ACCELERATORS, STRINGTABLE, ICON, CURSOR, BITMAP, VERSIONINFO, file
  resources of any named type (PNG, RCDATA), and user-defined resources with
  inline data (DLGINIT, AFX_DIALOG_LAYOUT).

The script is run through the C preprocessor first (the compiler given with
--cpp), with RC_INVOKED defined, as rc.exe does.

Usage: rc.py --cpp <compiler> [-I dir]... [-D name[=value]]... <input.rc> <output.res>
"""
import argparse
import os
import re
import struct
import subprocess
import sys

# ---- resource types ---------------------------------------------------------------
RT_CURSOR, RT_BITMAP, RT_ICON, RT_MENU, RT_DIALOG, RT_STRING = 1, 2, 3, 4, 5, 6
RT_ACCELERATOR, RT_RCDATA, RT_GROUP_CURSOR, RT_GROUP_ICON, RT_VERSION = 9, 10, 12, 14, 16
RT_DLGINIT, RT_HTML, RT_MANIFEST = 240, 23, 24

MOVEABLE, PURE, DISCARDABLE = 0x0010, 0x0020, 0x1000

# Window and control styles the statements imply (winuser.h values).
WS_POPUP, WS_CHILD, WS_VISIBLE = 0x80000000, 0x40000000, 0x10000000
WS_CAPTION, WS_BORDER, WS_SYSMENU = 0x00C00000, 0x00800000, 0x00080000
WS_GROUP, WS_TABSTOP, WS_VSCROLL = 0x00020000, 0x00010000, 0x00200000
DS_SETFONT = 0x40
SS_LEFT, SS_CENTER, SS_RIGHT, SS_ICON = 0, 1, 2, 3
BS_PUSHBUTTON, BS_DEFPUSHBUTTON, BS_CHECKBOX, BS_AUTOCHECKBOX = 0, 1, 2, 3
BS_RADIOBUTTON, BS_3STATE, BS_AUTO3STATE, BS_GROUPBOX, BS_AUTORADIOBUTTON = 4, 5, 6, 7, 9
BS_PUSHBOX = 0xA
ES_LEFT, CBS_SIMPLE, LBS_NOTIFY, SBS_HORZ = 0, 1, 1, 0

CONTROL_CLASSES = {'button': 0x80, 'edit': 0x81, 'static': 0x82, 'listbox': 0x83, 'scrollbar': 0x84, 'combobox': 0x85}

# Control statements: class, default style (beyond WS_CHILD | WS_VISIBLE),
# and whether the statement has text.
CONTROL_STATEMENTS = {
    'LTEXT': ('static', SS_LEFT | WS_GROUP, True),
    'RTEXT': ('static', SS_RIGHT | WS_GROUP, True),
    'CTEXT': ('static', SS_CENTER | WS_GROUP, True),
    'PUSHBUTTON': ('button', BS_PUSHBUTTON | WS_TABSTOP, True),
    'DEFPUSHBUTTON': ('button', BS_DEFPUSHBUTTON | WS_TABSTOP, True),
    'PUSHBOX': ('button', BS_PUSHBOX | WS_TABSTOP, True),
    'CHECKBOX': ('button', BS_CHECKBOX | WS_TABSTOP, True),
    'AUTOCHECKBOX': ('button', BS_AUTOCHECKBOX | WS_TABSTOP, True),
    'RADIOBUTTON': ('button', BS_RADIOBUTTON, True),
    'AUTORADIOBUTTON': ('button', BS_AUTORADIOBUTTON, True),
    'STATE3': ('button', BS_3STATE | WS_TABSTOP, True),
    'AUTO3STATE': ('button', BS_AUTO3STATE | WS_TABSTOP, True),
    'GROUPBOX': ('button', BS_GROUPBOX, True),
    'EDITTEXT': ('edit', ES_LEFT | WS_BORDER | WS_TABSTOP, False),
    'COMBOBOX': ('combobox', CBS_SIMPLE | WS_TABSTOP, False),
    'LISTBOX': ('listbox', LBS_NOTIFY | WS_BORDER, False),
    'SCROLLBAR': ('scrollbar', SBS_HORZ, False),
}

MF_GRAYED, MF_INACTIVE, MF_CHECKED, MF_POPUP = 0x1, 0x2, 0x8, 0x10
MF_MENUBARBREAK, MF_MENUBREAK, MF_END, MF_HELP = 0x20, 0x40, 0x80, 0x4000
MENU_OPTIONS = {'GRAYED': MF_GRAYED, 'INACTIVE': MF_INACTIVE, 'CHECKED': MF_CHECKED,
                'MENUBARBREAK': MF_MENUBARBREAK, 'MENUBREAK': MF_MENUBREAK, 'HELP': MF_HELP}

FVIRTKEY, FNOINVERT, FSHIFT, FCONTROL, FALT = 0x01, 0x02, 0x04, 0x08, 0x10


KEEP_CLASS_NAMES = os.environ.get('RC_PY_KEEP_CLASS_NAMES') == '1'  # for comparing with llvm-rc


class RcError(Exception):
    pass


# ---- tokens -----------------------------------------------------------------------
TOKEN = re.compile(r'''
    (?P<space>\s+)
  | (?P<string>L?"(?:[^"]|"")*")
  | (?P<number>(?:0[xX][0-9a-fA-F]+|\d+)[lLuU]*)
  | (?P<name>[A-Za-z_][A-Za-z0-9_.$]*)
  | (?P<punct>[,|&+\-~(){}*/!])
''', re.VERBOSE)


class Token:
    __slots__ = ('kind', 'text', 'line')

    def __init__(self, kind, text, line):
        self.kind, self.text, self.line = kind, text, line

    def __repr__(self):
        return '%s(%r)' % (self.kind, self.text)


def tokenize(text):
    tokens = []
    line = 1
    pos = 0
    for raw_line in text.split('\n'):
        stripped = raw_line.strip()
        if stripped.startswith('#'):
            # Line markers and pragmas the preprocessor leaves in.
            m = re.match(r'#\s*(\d+)', stripped)
            if m:
                line = int(m.group(1))
                continue
            m = re.match(r'#\s*pragma\s+code_page\s*\(\s*(\d+)\s*\)', stripped)
            if m and m.group(1) not in ('65001', 'DEFAULT'):
                raise RcError('code page %s is not supported (only UTF-8)' % m.group(1))
            line += 1
            continue
        pos = 0
        while pos < len(raw_line):
            m = TOKEN.match(raw_line, pos)
            if not m:
                raise RcError('line %d: unexpected character %r' % (line, raw_line[pos]))
            pos = m.end()
            if m.lastgroup != 'space':
                tokens.append(Token(m.lastgroup, m.group(), line))
        line += 1
    return tokens


def unquote(token):
    """The text of a string token, with rc.exe's escapes; returns (text, wide)."""
    s = token.text
    wide = s.startswith('L')
    if wide:
        s = s[1:]
    s = s[1:-1].replace('""', '"')
    out = []
    i = 0
    while i < len(s):
        c = s[i]
        if c != '\\' or i + 1 >= len(s):
            out.append(c)
            i += 1
            continue
        e = s[i + 1]
        i += 2
        simple = {'n': '\n', 't': '\t', 'r': '\r', 'a': '\b', 'b': '\b', '\\': '\\', '"': '"', "'": "'", 'f': '\f', 'v': '\v'}
        if e in simple:
            out.append(simple[e])
        elif e in 'xX':
            m = re.match(r'[0-9a-fA-F]{1,4}' if wide else r'[0-9a-fA-F]{1,2}', s[i:])
            out.append(chr(int(m.group(), 16)) if m else 'x')
            i += m.end() if m else 0
        elif e in '01234567':
            m = re.match(r'[0-7]{0,2}', s[i:])
            out.append(chr(int(e + m.group(), 8)))
            i += m.end()
        else:
            out.append('\\' + e)
    return ''.join(out), wide


def parse_number(text):
    t = text.rstrip('lLuU')
    return int(t, 16) if t.lower().startswith('0x') else int(t, 10)


# ---- parser -----------------------------------------------------------------------
class Parser:
    def __init__(self, tokens, base_dir):
        self.tokens = tokens
        self.pos = 0
        self.base_dir = base_dir
        self.language = 0x0409
        self.resources = []  # (type, name, language, flags, data)
        self.strings = {}    # (language, block) -> [16 strings]
        self.next_image_id = 1  # icons and cursors share the numbering, as with rc.exe

    # -- token helpers --
    def peek(self, offset=0):
        i = self.pos + offset
        return self.tokens[i] if i < len(self.tokens) else None

    def next(self):
        t = self.peek()
        if t is None:
            raise RcError('unexpected end of file')
        self.pos += 1
        return t

    def error(self, message, token=None):
        token = token or self.peek()
        raise RcError('line %s: %s' % (token.line if token else '?', message))

    def accept(self, text):
        t = self.peek()
        if t is not None and t.kind in ('punct', 'name') and t.text.upper() == text.upper():
            self.pos += 1
            return True
        return False

    def expect(self, text):
        if not self.accept(text):
            self.error('expected %s, got %r' % (text, self.peek().text if self.peek() else 'end of file'))

    def accept_comma(self):
        return self.accept(',')

    def is_begin(self):
        t = self.peek()
        return t is not None and (t.text == '{' or (t.kind == 'name' and t.text.upper() == 'BEGIN'))

    def is_end(self):
        t = self.peek()
        return t is not None and (t.text == '}' or (t.kind == 'name' and t.text.upper() == 'END'))

    def begin(self):
        if not self.is_begin():
            self.error('expected BEGIN')
        self.pos += 1

    def end(self):
        if not self.is_end():
            self.error('expected END')
        self.pos += 1

    # -- expressions (rc.exe: | & + - ~ NOT and parentheses) --
    def expression(self):
        """Returns (value, cleared): NOT x clears x from the default style."""
        value, cleared = self.term()
        while True:
            t = self.peek()
            if t is None or t.kind != 'punct' or t.text not in '|&+-*/':
                return value, cleared
            self.pos += 1
            rhs, rhs_cleared = self.term()
            cleared |= rhs_cleared
            op = t.text
            if op == '|':
                value |= rhs
            elif op == '&':
                value &= rhs
            elif op == '+':
                value += rhs
            elif op == '-':
                value -= rhs
            elif op == '*':
                value *= rhs
            elif op == '/':
                value //= rhs

    def term(self):
        t = self.next()
        if t.kind == 'name' and t.text.upper() == 'NOT':
            value, cleared = self.term()
            return 0, cleared | value
        if t.text == '~':
            value, cleared = self.term()
            return ~value & 0xFFFFFFFF, cleared
        if t.text == '-':
            value, cleared = self.term()
            return -value, cleared
        if t.text == '(':
            value, cleared = self.expression()
            self.expect(')')
            return value, cleared
        if t.kind == 'number':
            return parse_number(t.text), 0
        self.error('expected a number, got %r (an undefined symbol?)' % t.text, t)

    def number(self):
        value, cleared = self.expression()
        return value & ~cleared

    def style(self, default):
        value, cleared = self.expression()
        return ((default | value) & ~cleared) & 0xFFFFFFFF

    def string_or_number(self):
        t = self.peek()
        if t.kind == 'string':
            self.pos += 1
            return unquote(t)[0]
        return self.number()

    def text(self):
        t = self.next()
        if t.kind != 'string':
            self.error('expected a string', t)
        return unquote(t)[0]

    # -- resource names --
    def name_or_id(self):
        t = self.next()
        if t.kind == 'number':
            return parse_number(t.text) & 0xFFFF
        if t.kind == 'string':
            return unquote(t)[0].upper()
        if t.kind == 'name':
            return t.text.upper()
        self.error('expected a resource name', t)

    def add(self, rtype, name, data, flags=MOVEABLE | PURE | DISCARDABLE):
        self.resources.append((rtype, name, self.language, flags, bytes(data)))

    def file_path(self, token):
        text = unquote(token)[0]
        path = text if os.path.isabs(text) else os.path.join(self.base_dir, text)
        if not os.path.exists(path):
            self.error('file not found: %s' % text, token)
        return path

    # -- top level --
    def parse(self):
        while self.peek() is not None:
            t = self.peek()
            word = t.text.upper() if t.kind == 'name' else None
            if word == 'LANGUAGE':
                self.pos += 1
                primary = self.number()
                self.accept_comma()
                sub = self.number()
                self.language = (sub << 10) | primary
                continue
            if word == 'STRINGTABLE':
                self.pos += 1
                self.stringtable()
                continue
            if word in ('VERSION', 'CHARACTERISTICS'):
                self.pos += 1
                self.number()
                continue
            name = self.name_or_id()
            kind = self.next()
            if kind.kind == 'number':
                rtype = parse_number(kind.text)
            elif kind.kind == 'name':
                rtype = kind.text.upper()
            elif kind.kind == 'string':
                rtype = unquote(kind)[0].upper()
            else:
                self.error('expected a resource type', kind)
            self.resource(name, rtype)
        self.flush_strings()

    def skip_memory_options(self):
        while self.peek() is not None and self.peek().kind == 'name' and self.peek().text.upper() in (
                'PRELOAD', 'LOADONCALL', 'FIXED', 'MOVEABLE', 'DISCARDABLE', 'PURE', 'IMPURE', 'SHARED', 'NONSHARED'):
            self.pos += 1

    def common_options(self):
        """LANGUAGE, VERSION and CHARACTERISTICS inside a resource."""
        language = self.language
        while True:
            if self.accept('LANGUAGE'):
                primary = self.number()
                self.accept_comma()
                language = (self.number() << 10) | primary
            elif self.accept('VERSION') or self.accept('CHARACTERISTICS'):
                self.number()
            else:
                return language

    def resource(self, name, rtype):
        self.skip_memory_options()
        handlers = {
            'DIALOG': lambda: self.dialog(name, False),
            'DIALOGEX': lambda: self.dialog(name, True),
            'MENU': lambda: self.menu(name, False),
            'MENUEX': lambda: self.menu(name, True),
            'ACCELERATORS': lambda: self.accelerators(name),
            'ICON': lambda: self.icon(name, False),
            'CURSOR': lambda: self.icon(name, True),
            'BITMAP': lambda: self.bitmap(name),
            'VERSIONINFO': lambda: self.versioninfo(name),
            'TEXTINCLUDE': lambda: self.user(name, rtype),
        }
        if rtype in handlers:
            handlers[rtype]()
            return
        numeric = {'RCDATA': RT_RCDATA, 'DLGINIT': RT_DLGINIT, 'HTML': RT_HTML, 'MANIFEST': RT_MANIFEST}
        self.user(name, numeric.get(rtype, rtype))

    # -- user-defined resources: a file, or inline data --
    def user(self, name, rtype):
        saved_language = self.language
        self.language = self.common_options()
        t = self.peek()
        if t.kind == 'string' and not self.is_begin():
            self.pos += 1
            with open(self.file_path(t), 'rb') as f:
                self.add(rtype, name, f.read())
        else:
            self.begin()
            data = bytearray()
            while not self.is_end():
                t = self.peek()
                if t.kind == 'string':
                    self.pos += 1
                    text, wide = unquote(t)
                    data += text.encode('utf-16-le') if wide else text.encode('utf-8')
                else:
                    long_value = t.kind == 'number' and t.text.lower().endswith('l')
                    value = self.number()
                    data += struct.pack('<I' if long_value else '<H', value & (0xFFFFFFFF if long_value else 0xFFFF))
                self.accept_comma()
            self.end()
            self.add(rtype, name, data)
        self.language = saved_language

    # -- strings --
    def stringtable(self):
        self.skip_memory_options()
        language = self.common_options()
        self.begin()
        while not self.is_end():
            sid = self.number() & 0xFFFF
            self.accept_comma()
            text = self.text()
            block = self.strings.setdefault((language, (sid >> 4) + 1), [None] * 16)
            block[sid & 15] = text
        self.end()

    def flush_strings(self):
        for (language, block), strings in sorted(self.strings.items()):
            data = bytearray()
            for s in strings:
                encoded = (s or '').encode('utf-16-le')
                data += struct.pack('<H', len(encoded) // 2) + encoded
            self.resources.append((RT_STRING, block, language, MOVEABLE | PURE | DISCARDABLE, bytes(data)))

    # -- dialogs --
    def dialog(self, name, extended):
        x = self.number(); self.accept_comma()
        y = self.number(); self.accept_comma()
        cx = self.number(); self.accept_comma()
        cy = self.number()
        help_id = 0
        if self.accept_comma():
            help_id = self.number()
        style = WS_POPUP | WS_BORDER | WS_SYSMENU
        style_given = False
        ex_style = 0
        caption = ''
        menu = None
        cls = None
        font = None
        language = self.language
        while not self.is_begin():
            t = self.next()
            word = t.text.upper()
            if word == 'STYLE':
                style = self.style(0)
                style_given = True
            elif word == 'EXSTYLE':
                ex_style = self.number()
            elif word == 'CAPTION':
                caption = self.text()
                if not style_given:
                    style |= WS_CAPTION
            elif word == 'MENU':
                menu = self.name_or_id()
            elif word == 'CLASS':
                cls = self.name_or_id()
            elif word == 'FONT':
                size = self.number(); self.accept_comma()
                face = self.text()
                weight, italic, charset = 0, 0, 1
                if self.accept_comma():
                    weight = self.number()
                    if self.accept_comma():
                        italic = self.number()
                        if self.accept_comma():
                            charset = self.number()
                font = (size, face, weight, italic, charset)
            elif word == 'LANGUAGE':
                primary = self.number(); self.accept_comma()
                language = (self.number() << 10) | primary
            elif word in ('VERSION', 'CHARACTERISTICS'):
                self.number()
            else:
                self.error('unknown dialog statement %s' % t.text, t)
        if font is not None:
            style |= DS_SETFONT
        self.begin()
        controls = []
        while not self.is_end():
            controls.append(self.control(extended))
        self.end()

        data = bytearray()
        if extended:
            data += struct.pack('<HHIIIHhhhh', 1, 0xFFFF, help_id, ex_style, style, len(controls), x, y, cx, cy)
        else:
            data += struct.pack('<IIHhhhh', style, ex_style, len(controls), x, y, cx, cy)
        data += sz_or_ord(menu)
        data += sz_or_ord(cls)
        data += sz(caption)
        if style & DS_SETFONT:
            size, face, weight, italic, charset = font
            if extended:
                data += struct.pack('<HHBB', size, weight, italic, charset)
            else:
                data += struct.pack('<H', size)
            data += sz(face)
        for c in controls:
            align(data)
            (cid, cclass, text, cstyle, cex, chelp, cx_, cy_, cw, ch, cdata) = c
            if extended:
                data += struct.pack('<IIIhhhhI', chelp, cex, cstyle, cx_, cy_, cw, ch, cid & 0xFFFFFFFF)
            else:
                data += struct.pack('<IIhhhhH', cstyle, cex, cx_, cy_, cw, ch, cid & 0xFFFF)
            data += sz_or_ord(cclass)
            data += sz_or_ord(text)
            data += struct.pack('<H', len(cdata)) + cdata
        saved = self.language
        self.language = language
        self.add(RT_DIALOG, name, data)
        self.language = saved

    def control(self, extended):
        t = self.next()
        word = t.text.upper()
        base = WS_CHILD | WS_VISIBLE
        if word == 'CONTROL':
            text = self.string_or_number(); self.accept_comma()
            cid = self.number(); self.accept_comma()
            ct = self.next()
            if ct.kind == 'string':
                class_name = unquote(ct)[0]
            else:
                class_name = ct.text
            self.accept_comma()
            cstyle = self.style(base)
            self.accept_comma()
            x = self.number(); self.accept_comma()
            y = self.number(); self.accept_comma()
            w = self.number(); self.accept_comma()
            h = self.number()
            cclass = class_name if KEEP_CLASS_NAMES else CONTROL_CLASSES.get(class_name.lower(), class_name)
        elif word == 'ICON':
            text = self.string_or_number(); self.accept_comma()
            cid = self.number(); self.accept_comma()
            x = self.number(); self.accept_comma()
            y = self.number()
            w = h = 0
            cstyle = base | SS_ICON
            if self.accept_comma():
                w = self.number(); self.accept_comma()
                h = self.number()
                if self.accept_comma():
                    cstyle = self.style(base | SS_ICON)
            cclass = CONTROL_CLASSES['static']
            return self.control_tail(cid, cclass, text, cstyle, x, y, w, h, extended)
        elif word in CONTROL_STATEMENTS:
            class_name, default, has_text = CONTROL_STATEMENTS[word]
            text = ''
            if has_text:
                text = self.string_or_number(); self.accept_comma()
            cid = self.number(); self.accept_comma()
            x = self.number(); self.accept_comma()
            y = self.number(); self.accept_comma()
            w = self.number(); self.accept_comma()
            h = self.number()
            cstyle = base | default
            if self.accept_comma():
                # A given style is combined with the statement's default,
                # except for combo boxes, whose default (CBS_SIMPLE |
                # WS_TABSTOP) applies only without one (as rc.exe and llvm-rc).
                if word == 'COMBOBOX':
                    default = 0
                cstyle = self.style(base | default)
            cclass = CONTROL_CLASSES[class_name]
        else:
            self.error('unknown control statement %s' % t.text, t)
        return self.control_tail(cid, cclass, text, cstyle, x, y, w, h, extended)

    def control_tail(self, cid, cclass, text, cstyle, x, y, w, h, extended):
        ex, help_id = 0, 0
        if self.accept_comma():
            ex = self.number()
            if self.accept_comma():
                help_id = self.number()
        cdata = b''
        if self.is_begin():
            # Inline creation data.
            self.begin()
            raw = bytearray()
            while not self.is_end():
                t = self.peek()
                if t.kind == 'string':
                    self.pos += 1
                    s, wide = unquote(t)
                    raw += s.encode('utf-16-le') if wide else s.encode('utf-8')
                else:
                    raw += struct.pack('<H', self.number() & 0xFFFF)
                self.accept_comma()
            self.end()
            cdata = bytes(raw)
        if isinstance(text, int):
            text = text & 0xFFFF
        return (cid, cclass, text, cstyle, ex, help_id, x, y, w, h, cdata)

    # -- menus --
    def menu(self, name, extended):
        language = self.common_options()
        data = bytearray()
        if extended:
            data += struct.pack('<HHI', 1, 4, 0)
            self.menuex_items(data, top=True)
        else:
            data += struct.pack('<HH', 0, 0)
            self.menu_items(data)
        saved = self.language
        self.language = language
        self.add(RT_MENU, name, data)
        self.language = saved

    def menu_items(self, data):
        self.begin()
        items = []
        while not self.is_end():
            t = self.next()
            word = t.text.upper()
            if word == 'MENUITEM':
                if self.accept('SEPARATOR'):
                    items.append(('item', 0, 0, ''))
                    continue
                text = self.text(); self.accept_comma()
                mid = self.number()
                flags = 0
                while self.accept_comma():
                    opt = self.next().text.upper()
                    flags |= MENU_OPTIONS.get(opt, 0)
                while self.peek() is not None and self.peek().kind == 'name' and self.peek().text.upper() in MENU_OPTIONS:
                    flags |= MENU_OPTIONS[self.next().text.upper()]
                items.append(('item', flags, mid, text))
            elif word == 'POPUP':
                text = self.text()
                flags = MF_POPUP
                while self.accept_comma():
                    flags |= MENU_OPTIONS.get(self.next().text.upper(), 0)
                while self.peek() is not None and self.peek().kind == 'name' and self.peek().text.upper() in MENU_OPTIONS:
                    flags |= MENU_OPTIONS[self.next().text.upper()]
                sub = bytearray()
                self.menu_items(sub)
                items.append(('popup', flags, sub, text))
            else:
                self.error('unknown menu statement %s' % t.text, t)
        self.end()
        for i, item in enumerate(items):
            last = MF_END if i == len(items) - 1 else 0
            kind, flags, payload, text = item
            if kind == 'item':
                data += struct.pack('<HH', flags | last, payload & 0xFFFF) + sz(text)
            else:
                data += struct.pack('<H', flags | last) + sz(text) + payload

    def menuex_items(self, data, top=False):
        self.begin()
        items = []
        while not self.is_end():
            t = self.next()
            word = t.text.upper()
            popup = word == 'POPUP'
            if word not in ('MENUITEM', 'POPUP'):
                self.error('unknown menu statement %s' % t.text, t)
            text = self.text()
            values = []
            while self.accept_comma():
                values.append(0 if self.peek().text == ',' else self.number())
            mid, mtype, state, help_id = (values + [0, 0, 0, 0])[:4]
            sub = None
            if popup:
                sub = bytearray()
                self.menuex_items(sub)
            items.append((popup, mid, mtype, state, help_id, text, sub))
        self.end()
        for i, (popup, mid, mtype, state, help_id, text, sub) in enumerate(items):
            align(data)
            flags = (0x80 if i == len(items) - 1 else 0) | (0x01 if popup else 0)
            data += struct.pack('<IIIH', mtype, state, mid & 0xFFFFFFFF, flags) + sz(text)
            if popup:
                align(data)
                data += struct.pack('<I', help_id) + sub

    # -- accelerators --
    def accelerators(self, name):
        language = self.common_options()
        self.begin()
        entries = []
        while not self.is_end():
            t = self.peek()
            if t.kind == 'string':
                self.pos += 1
                key_text = unquote(t)[0]
                if key_text.startswith('^') and len(key_text) == 2:
                    key = ord(key_text[1].upper()) - 0x40
                else:
                    key = ord(key_text[0])
                ascii_key = True
            else:
                key = self.number()
                ascii_key = False
            self.accept_comma()
            cmd = self.number()
            flags = 0
            while self.accept_comma() or (self.peek() is not None and self.peek().kind == 'name' and self.peek().text.upper() in ('VIRTKEY', 'ASCII', 'NOINVERT', 'SHIFT', 'CONTROL', 'ALT')):
                if self.peek().kind != 'name':
                    continue
                opt = self.next().text.upper()
                flags |= {'VIRTKEY': FVIRTKEY, 'ASCII': 0, 'NOINVERT': FNOINVERT, 'SHIFT': FSHIFT, 'CONTROL': FCONTROL, 'ALT': FALT}[opt]
            if ascii_key and flags & FVIRTKEY:
                key = ord(chr(key).upper())
            entries.append((flags, key, cmd))
        self.end()
        data = bytearray()
        for i, (flags, key, cmd) in enumerate(entries):
            data += struct.pack('<HHHH', flags | (0x80 if i == len(entries) - 1 else 0), key & 0xFFFF, cmd & 0xFFFF, 0)
        saved = self.language
        self.language = language
        self.add(RT_ACCELERATOR, name, data)
        self.language = saved

    # -- icons, cursors, bitmaps --
    def icon(self, name, cursor):
        language = self.common_options()
        t = self.next()
        with open(self.file_path(t), 'rb') as f:
            blob = f.read()
        reserved, kind, count = struct.unpack_from('<HHH', blob, 0)
        saved = self.language
        self.language = language
        group = bytearray(struct.pack('<HHH', 0, 2 if cursor else 1, count))
        for i in range(count):
            # In a .ico file the two words are planes and bit count; in a
            # .cur file they are the hotspot.
            w, h, colors, _, word1, word2, size, offset = struct.unpack_from('<BBBBHHII', blob, 6 + 16 * i)
            image = blob[offset:offset + size]
            rid = self.next_image_id
            self.next_image_id += 1
            # Planes and bit count come from the image itself, as rc.exe
            # reads them; .ico directories are often wrong about them.
            if image[:4] == b'\x28\x00\x00\x00':
                planes, bits = struct.unpack_from('<HH', image, 12)
            elif image[:8] == b'\x89PNG\r\n\x1a\n':
                depth, color_type = image[24], image[25]
                channels = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}.get(color_type, 4)
                planes, bits = 1, depth * channels
            else:
                planes, bits = word1, word2
            if cursor:
                # RT_CURSOR data starts with the hotspot.
                self.add(RT_CURSOR, rid, struct.pack('<HH', word1, word2) + image, MOVEABLE | DISCARDABLE)
                group += struct.pack('<HHHHIH', w or 256, (h or 256) * 2, planes, bits, size + 4, rid)
            else:
                self.add(RT_ICON, rid, image, MOVEABLE | DISCARDABLE)
                group += struct.pack('<BBBBHHIH', w, h, colors, 0, planes, bits, size, rid)
        self.add(RT_GROUP_CURSOR if cursor else RT_GROUP_ICON, name, group)
        self.language = saved

    def bitmap(self, name):
        language = self.common_options()
        t = self.next()
        with open(self.file_path(t), 'rb') as f:
            blob = f.read()
        if blob[:2] != b'BM':
            self.error('not a .bmp file: %s' % t.text, t)
        saved = self.language
        self.language = language
        self.add(RT_BITMAP, name, blob[14:])  # without BITMAPFILEHEADER
        self.language = saved

    # -- version information --
    def versioninfo(self, name):
        fixed = {'FILEVERSION': [0, 0, 0, 0], 'PRODUCTVERSION': [0, 0, 0, 0], 'FILEFLAGSMASK': 0,
                 'FILEFLAGS': 0, 'FILEOS': 0, 'FILETYPE': 0, 'FILESUBTYPE': 0}
        while not self.is_begin():
            t = self.next()
            word = t.text.upper()
            if word in ('FILEVERSION', 'PRODUCTVERSION'):
                parts = [self.number()]
                while self.accept_comma():
                    parts.append(self.number())
                fixed[word] = (parts + [0, 0, 0, 0])[:4]
            elif word in fixed:
                fixed[word] = self.number()
            else:
                self.error('unknown VERSIONINFO statement %s' % t.text, t)
        fv, pv = fixed['FILEVERSION'], fixed['PRODUCTVERSION']
        ffi = struct.pack('<13I', 0xFEEF04BD, 0x00010000, (fv[0] << 16) | fv[1], (fv[2] << 16) | fv[3],
                          (pv[0] << 16) | pv[1], (pv[2] << 16) | pv[3], fixed['FILEFLAGSMASK'], fixed['FILEFLAGS'],
                          fixed['FILEOS'], fixed['FILETYPE'], fixed['FILESUBTYPE'], 0, 0)
        children = self.version_block_children()
        self.add(RT_VERSION, name, version_node('VS_VERSION_INFO', ffi, 0, children))

    def version_block_children(self):
        self.begin()
        children = []
        while not self.is_end():
            t = self.next()
            word = t.text.upper()
            key = self.text()
            if word == 'BLOCK':
                children.append(version_node(key, b'', 1, self.version_block_children()))
            elif word == 'VALUE':
                values = []
                text_value = False
                while self.accept_comma():
                    v = self.peek()
                    if v.kind == 'string':
                        self.pos += 1
                        # Adjacent strings are concatenated, as by the preprocessor.
                        s = unquote(v)[0]
                        while self.peek() is not None and self.peek().kind == 'string':
                            s += unquote(self.next())[0]
                        values.append(s)
                        text_value = True
                    else:
                        long_value = v.kind == 'number' and v.text.lower().endswith('l')
                        values.append(('L' if long_value else 'H', self.number()))
                if text_value:
                    text = ''.join(v for v in values if isinstance(v, str))
                    value = (text + '\0').encode('utf-16-le')
                    children.append(version_node(key, value, 1, [], value_length=len(text) + 1))
                else:
                    raw = b''.join(struct.pack('<' + k, n & (0xFFFFFFFF if k == 'L' else 0xFFFF)) for k, n in values)
                    children.append(version_node(key, raw, 0, []))
            else:
                self.error('unknown VERSIONINFO block statement %s' % t.text, t)
        self.end()
        return children


def version_node(key, value, value_type, children, value_length=None):
    body = bytearray(struct.pack('<HHH', 0, 0, value_type))
    body += sz(key)
    align(body)
    body += value
    for child in children:
        align(body)
        body += child
    length = value_length if value_length is not None else len(value)
    struct.pack_into('<HH', body, 0, len(body), length)
    return bytes(body)


def sz(text):
    return (text + '\0').encode('utf-16-le')


def sz_or_ord(value):
    if value is None:
        return b'\0\0'
    if isinstance(value, int):
        return struct.pack('<HH', 0xFFFF, value & 0xFFFF)
    return sz(value)


def align(data, n=4):
    while len(data) % n:
        data.append(0)


# ---- .res output --------------------------------------------------------------------
def name_field(value):
    if isinstance(value, int):
        return struct.pack('<HH', 0xFFFF, value)
    return sz(value)


def write_res(resources, path):
    out = bytearray()
    # The empty entry every .res starts with.
    out += struct.pack('<IIHHHHIHHII', 0, 32, 0xFFFF, 0, 0xFFFF, 0, 0, 0, 0, 0, 0)
    for rtype, name, language, flags, data in resources:
        header = bytearray()
        header += name_field(rtype)
        header += name_field(name)
        align(header)
        header += struct.pack('<IHHII', 0, flags, language, 0, 0)
        out += struct.pack('<II', len(data), len(header) + 8) + header
        out += data
        align(out)
    content = bytes(out)
    if os.path.exists(path):
        with open(path, 'rb') as f:
            if f.read() == content:
                return
    with open(path, 'wb') as f:
        f.write(content)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cpp', required=True, help='C compiler used as the preprocessor')
    ap.add_argument('-I', dest='includes', action='append', default=[])
    ap.add_argument('-D', dest='defines', action='append', default=[])
    ap.add_argument('--depfile')
    ap.add_argument('input')
    ap.add_argument('output')
    args = ap.parse_args()

    cmd = [args.cpp, '-E', '-x', 'c', '-DRC_INVOKED', '-D_WIN32']
    cmd += ['-I' + i for i in args.includes]
    cmd += ['-D' + d for d in args.defines]
    if args.depfile:
        cmd += ['-MD', '-MF', args.depfile, '-MT', args.output]
    cmd.append(args.input)
    result = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if result.returncode != 0:
        sys.stderr.write(result.stderr.decode('utf-8', 'replace'))
        sys.exit('rc.py: preprocessing failed')
    text = result.stdout.decode('utf-8')

    parser = Parser(tokenize(text), os.path.dirname(os.path.abspath(args.input)))
    try:
        parser.parse()
    except RcError as e:
        sys.exit('rc.py: %s: %s' % (args.input, e))
    write_res(parser.resources, args.output)


if __name__ == '__main__':
    main()
