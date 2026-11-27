#!/usr/bin/env python3
"""First draft of a Qt twin from an upstream mptrack/ file.

    qt/tools/twin.py upstream/mptrack/Foo.h > qt/mptrack/Foo.h

Applies the mechanical part of the MFC to Qt mapping the twins follow
(docs/MFCLESS.md, "Writing a twin"): types, controls, message handler
declarations. The result still needs a read-through and hand edits; lines it
could not map keep their MFC names, so the compiler points at them.
Also useful on an upstream diff, to see what a change looks like in twin terms.
"""
import re
import sys

TYPES = [
	(r'\bCString[AW]?\b', 'mpt::ustring'),
	(r'\bmpt::tstring\b', 'mpt::ustring'),
	(r'\bmpt::winstring\b', 'mpt::ustring'),
	(r'\bDWORD_PTR\b', 'uintptr_t'),
	(r'\bUINT_PTR\b', 'uintptr_t'),
	(r'\bINT_PTR\b', 'intptr_t'),
	(r'\bDWORD\b', 'uint32'),
	(r'\bUINT\b', 'uint32'),
	(r'\bLONG\b', 'int32'),
	(r'\bWORD\b', 'uint16'),
	(r'\bBYTE\b', 'uint8'),
	(r'\bBOOL\b', 'bool'),
	(r'\bTRUE\b', 'true'),
	(r'\bFALSE\b', 'false'),
	(r'\bCOLORREF\b', 'uint32'),
	(r'\bWPARAM\b', 'uintptr_t'),
	(r'\bLPARAM\b', 'intptr_t'),
	(r'\bLRESULT\b', 'intptr_t'),
	(r'\bTCHAR\b', 'mpt::uchar'),
	(r'\bLPCTSTR\b', 'const mpt::uchar *'),
	(r'\bHWND\b', 'QWidget *'),
	(r'\bCWnd\b', 'QWidget'),
	(r'\bCRect\b', 'QRect'),
	(r'\bCPoint\b', 'QPoint'),
	(r'\bCSize\b', 'QSize'),
	(r'\bCFont\b', 'QFont'),
	(r'\bCMenu\b', 'QMenu'),
	(r'\bCObject\b', 'QObject'),
	(r'\bASSERT\(', 'MPT_ASSERT('),
	(r'\b_T\(("(?:[^"\\]|\\.)*")\)', r'U_(\1)'),
	(r'\bMPT_CFORMAT\b', 'MPT_UFORMAT'),
	(r'\bMPT_TFORMAT\b', 'MPT_UFORMAT'),
	(r'\bmpt::cfmt\b', 'mpt::ufmt'),
	(r'\bmpt::tfmt\b', 'mpt::ufmt'),
	(r'\bmpt::ToCString\(', 'mpt::ToUnicode('),
	(r'\bmpt::ToWin\(', 'mpt::ToUnicode('),
]

# Controls become pointers to Qt widgets that the dialog template creates.
CONTROLS = {
	'CEdit': 'QLineEdit', 'CNumberEdit': 'QLineEdit', 'CStatic': 'QLabel',
	'CButton': 'QAbstractButton', 'CComboBox': 'QComboBox', 'CListBox': 'QListWidget',
	'CSliderCtrl': 'QSlider', 'CSpinButtonCtrl': 'QSpinBox', 'CScrollBar': 'QScrollBar',
	'CListCtrl': 'QTreeWidget', 'CTreeCtrl': 'QTreeWidget', 'CTabCtrl': 'QTabBar',
	'CProgressCtrl': 'QProgressBar', 'CRichEditCtrl': 'QTextEdit',
}

def twin(text):
	for k, v in CONTROLS.items():
		# member declarations: "\tCEdit m_a, m_b;" -> "\tQLineEdit *m_a = nullptr, *m_b = nullptr;"
		def members(m):
			names = [n.strip() for n in m.group(2).split(',')]
			return '%s%s %s;' % (m.group(1), v, ', '.join('*%s = nullptr' % n for n in names))
		text = re.sub(r'^(\s*)' + k + r'\s+((?:m_\w+|\w+)(?:\s*,\s*\w+)*);', members, text, flags=re.M)
		text = re.sub(r'\b' + k + r'\s*&', v + ' &', text)
		text = re.sub(r'\b' + k + r'\s*\*', v + ' *', text)
	for a, b in TYPES:
		text = re.sub(a, b, text)
	text = re.sub(r'\bafx_msg\s+', '', text)
	text = re.sub(r'^\s*DECLARE_MESSAGE_MAP\(\)\s*\n', '', text, flags=re.M)
	text = re.sub(r'^\s*DECLARE_(DYNAMIC|DYNCREATE|SERIAL)\(\w+\)\s*\n', '\tQ_OBJECT\n', text, flags=re.M)
	text = re.sub(r'^\s*//\{\{AFX_\w+(\(\w+\))?\s*\n', '', text, flags=re.M)
	text = re.sub(r'^\s*//\}\}AFX_\w+\s*\n', '', text, flags=re.M)
	text = text.replace('void DoDataExchange(CDataExchange *pDX) override;', 'void DoDataExchange();')
	text = text.replace('void DoDataExchange(CDataExchange* pDX) override;', 'void DoDataExchange();')
	# Forward declarations of the Qt classes now named, outside the namespace.
	used = sorted(set(re.findall(r'\b(Q[A-Z][A-Za-z]+)\b', text)) - {'Q_OBJECT'})
	if used and 'OPENMPT_NAMESPACE_BEGIN' in text:
		decls = ''.join('class %s;\n' % q for q in used)
		text = text.replace('OPENMPT_NAMESPACE_BEGIN', decls + '\nOPENMPT_NAMESPACE_BEGIN', 1)
	text = text.replace(' * Notes  : (currently none)', ' * Notes  : mfcless twin of upstream\'s mptrack/' + NAME + '.')
	return text

if __name__ == '__main__':
	path = sys.argv[1]
	NAME = path.split('mptrack/', 1)[-1]
	sys.stdout.write(twin(open(path, encoding='utf-8', errors='surrogateescape').read()))
