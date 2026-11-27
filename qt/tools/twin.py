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
	(r'(\w+)\(NMHDR\s*\*\s*\w*,\s*LRESULT\s*\*\s*\w*\)', r'\1()'),
	(r'(?:BOOL|bool) PreTranslateMessage\(MSG\s*\*\s*\w*\)( override)?;', 'bool eventFilter(QObject *watched, QEvent *e) override;'),
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
	(r'\bHTREEITEM\b', 'QTreeWidgetItem *'),
	(r'\bCComboBoxEx\b', 'QComboBox'),
	(r'BOOL PreTranslateMessage\(MSG\s*\*\s*\w*\)\s*override;', 'bool eventFilter(QObject *watched, QEvent *e) override;'),
	(r'BOOL OnInitDialog\(\)', 'bool OnInitDialog()'),
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
	'CEdit': 'QLineEdit', 'CStatic': 'QLabel', 'CComboBoxEx': 'QComboBox',
	'CButton': 'QAbstractButton', 'CComboBox': 'QComboBox', 'CListBox': 'QListWidget',
	'CSliderCtrl': 'QSlider', 'CSpinButtonCtrl': 'mfcless::UpDown', 'CScrollBar': 'QScrollBar',
	'CListCtrl': 'QTreeWidget', 'CTreeCtrl': 'QTreeWidget', 'CTabCtrl': 'mfcless::TabControl', 'CCheckListBox': 'QListWidget',
	'CProgressCtrl': 'QProgressBar', 'CRichEditCtrl': 'QTextEdit',
}

# Base classes: MFC windows become the Qt widget that plays the same role.
BASES = {
	'CDialog': 'DialogBase', 'CDialogEx': 'DialogBase', 'CPropertyPage': 'PropertyPage',
	'CFormView': 'DialogBase', 'CPropertySheet': 'QDialog',
	'CWnd': 'QWidget', 'CView': 'QAbstractScrollArea', 'CScrollView': 'QAbstractScrollArea',
	'CMDIChildWnd': 'QMdiSubWindow', 'CMDIFrameWnd': 'QMainWindow', 'CFrameWnd': 'QMainWindow',
	'CEdit': 'QLineEdit', 'CStatic': 'QLabel', 'CButton': 'QPushButton', 'CComboBox': 'QComboBox',
	'CListBox': 'QListWidget', 'CTreeCtrl': 'QTreeWidget', 'CListCtrl': 'QTreeWidget',
	'CMFCListCtrl': 'QTreeWidget', 'CMFCListCtrlEx': 'QTreeWidget', 'CTabCtrl': 'QTabBar',
	'CSliderCtrl': 'QSlider', 'CSpinButtonCtrl': 'mfcless::UpDown', 'CToolBar': 'QToolBar',
	'CToolBarCtrl': 'QToolBar', 'CDialogBar': 'QWidget', 'CReBar': 'QWidget',
	'CToolTipCtrl': 'QObject', 'COleDropTarget': 'QObject', 'CMultiDocTemplate': 'QObject',
	'CDocManager': 'QObject', 'CDocument': 'QObject', 'CImageList': 'QObject',
	'CSplitterWnd': 'QSplitter', 'CMFCDynamicLayout': 'QObject',
}

def twin(text):
	for k, v in BASES.items():
		text = re.sub(r'(:\s*(?:public|protected|private)\s+)' + k + r'\b', r'\1' + v, text)
	# Classes now based on a Qt class get Q_OBJECT.
	text = re.sub(r'^(class \w+\s*:\s*public\s+(?:Q\w+|DialogBase|PropertyPage|ResizableDialog|CAbstractVstEditor|CModScrollView|CModControlDlg|CModControlView|CModTabCtrl)\b[^\n{]*\n\{\n)', r'\1\tQ_OBJECT\n', text, flags=re.M)
	for k, v in CONTROLS.items():
		# member declarations: "\tCEdit m_a, m_b;" -> "\tQLineEdit *m_a = nullptr, *m_b = nullptr;"
		def members(m):
			names = [n.strip() for n in m.group(2).split(',')]
			return '%s%s %s;' % (m.group(1), v, ', '.join('*%s = nullptr' % n for n in names))
		text = re.sub(r'^(\s*)' + k + r'\s+((?:m_\w+|\w+)(?:\s*,\s*\w+)*);', members, text, flags=re.M)
		# arrays: "\tCComboBox m_a[N];" -> "\tQComboBox *m_a[N] = {};"
		text = re.sub(r'^(\s*)' + k + r'\s+(\w+\[[^\]]+\]);', r'\1' + v + r' *\2 = {};', text, flags=re.M)
		text = re.sub(r'\b' + k + r'\s*&', v + ' &', text)
		text = re.sub(r'\b' + k + r'\s*\*', v + ' *', text)
	for a, b in TYPES:
		text = re.sub(a, b, text)
	text = re.sub(r'\bafx_msg\s+', '', text)
	text = re.sub(r'^\s*DECLARE_(DYNAMIC|DYNCREATE|SERIAL)\(\w+\)\s*\n', '\tQ_OBJECT\n', text, flags=re.M)
	text = re.sub(r'^\s*//\{\{AFX_\w+(\(\w+\))?\s*\n', '', text, flags=re.M)
	text = re.sub(r'^\s*//\}\}AFX_\w+\s*\n', '', text, flags=re.M)
	text = re.sub(r'void DoDataExchange\(CDataExchange\s*\*\s*\w*\)( override)?;', 'void DoDataExchange(DataExchange &dx) override;', text)
	text = re.sub(r'^(\s*)DECLARE_MESSAGE_MAP\(\)', r'\1void RegisterMessageMap() override;', text, flags=re.M)
	text = re.sub(r'void (On[HV]Scroll)\(uint32\s*\w*,\s*uint32\s*\w*,\s*CScrollBar\s*\*\s*\w*\)( override)?;', r'void \1(mfcless::ScrollCode code, int pos, QWidget *control);', text)
	text = re.sub(r'void (\w+)\(CCmdUI\s*\*\s*\w*\)', r'void \1(mfcless::CommandState &state)', text)
	# Forward declarations of the Qt classes now named, outside the namespace.
	used = sorted(set(re.findall(r'\b(Q[A-Z][A-Za-z]+)\b', text)) - {'Q_OBJECT'})
	# Base classes must be complete: include them rather than declare them.
	bases = sorted(set(re.findall(r':\s*public\s+(Q[A-Z][A-Za-z]+)\b', text)))
	used = [q for q in used if q not in bases]
	if bases and '#include "openmpt/all/BuildSettings.hpp"' in text:
		incs = ''.join('#include <%s/%s>\n' % ('QtCore' if q in ('QObject',) else 'QtWidgets', q) for q in bases)
		text = text.replace('#include "openmpt/all/BuildSettings.hpp"\n', '#include "openmpt/all/BuildSettings.hpp"\n' + incs, 1)
	if used and 'OPENMPT_NAMESPACE_BEGIN' in text:
		decls = ''.join('class %s;\n' % q for q in used)
		text = text.replace('OPENMPT_NAMESPACE_BEGIN', decls + '\nOPENMPT_NAMESPACE_BEGIN', 1)
	text = text.replace(' * Notes  : (currently none)', ' * Notes  : mfcless twin of upstream\'s mptrack/' + NAME + '.\n *          ' + DRAFT_MARK)
	if DRAFT_MARK not in text:
		text = '// ' + DRAFT_MARK + '\n' + text
	# Dialog bases need their header.
	if re.search(r'public\s+(DialogBase|PropertyPage)\b', text) and '"DialogBase.h"' not in text and '"ResizableDialog.h"' not in text:
		text = text.replace('#include "openmpt/all/BuildSettings.hpp"\n', '#include "openmpt/all/BuildSettings.hpp"\n#include "DialogBase.h"\n', 1)
	return text

# Drafts carry this mark; a twin that has been edited by hand drops it, and
# qt/tools/twin.py --refresh leaves it alone.
DRAFT_MARK = 'Draft from qt/tools/twin.py, not yet edited by hand.'

# ---- .cpp files ------------------------------------------------------------

NOTIFY = {
	'CBN_SELCHANGE': 'SelChange', 'CBN_EDITCHANGE': 'EditChange', 'CBN_EDITUPDATE': 'EditUpdate',
	'CBN_DROPDOWN': 'DropDown', 'CBN_CLOSEUP': 'CloseUp', 'CBN_SETFOCUS': 'SetFocus', 'CBN_KILLFOCUS': 'KillFocus',
	'EN_CHANGE': 'EditChange', 'EN_UPDATE': 'EditUpdate', 'EN_SETFOCUS': 'SetFocus', 'EN_KILLFOCUS': 'KillFocus',
	'LBN_SELCHANGE': 'SelChange', 'LBN_DBLCLK': 'DoubleClicked', 'CLBN_CHKCHANGE': 'CheckChange',
	'BN_DOUBLECLICKED': 'DoubleClicked',
	'UDN_DELTAPOS': 'DeltaPos', 'NM_DBLCLK': 'DoubleClicked', 'NM_CLICK': 'Click', 'NM_RCLICK': 'RightClick',
	'LVN_ITEMCHANGED': 'ItemChanged', 'TVN_SELCHANGED': 'SelChange', 'TCN_SELCHANGE': 'SelChange',
	'TCN_SELCHANGING': 'SelChanging', 'LVN_BEGINLABELEDIT': 'BeginLabelEdit', 'LVN_ENDLABELEDIT': 'EndLabelEdit',
	'TVN_BEGINLABELEDIT': 'BeginLabelEdit', 'TVN_ENDLABELEDIT': 'EndLabelEdit', 'LVN_BEGINDRAG': 'BeginDrag',
	'TVN_BEGINDRAG': 'BeginDrag', 'LVN_KEYDOWN': 'KeyDown', 'TVN_KEYDOWN': 'KeyDown',
}

def message_map(m):
	cls, base, body = m.group(1), m.group(2), m.group(3)
	base = BASES.get(base, base)
	out = ['void %s::RegisterMessageMap()' % cls, '{', '\t%s::RegisterMessageMap();' % base]
	for line in body.split('\n'):
		t = line.strip()
		if not t or t.startswith('//{{') or t.startswith('//}}'):
			continue
		if t.startswith('//'):
			out.append('\t' + t)
			continue
		mm = re.match(r'(\w+)\((.*)\)\s*(//.*)?$', t)
		if not mm:
			out.append('\t// ' + t)
			continue
		macro, args = mm.group(1), [a.strip() for a in mm.group(2).split(',')] if mm.group(2).strip() else []
		comment = ('  ' + mm.group(3)) if mm.group(3) else ''
		if macro in ('ON_COMMAND', 'ON_COMMAND_EX', 'ON_BN_CLICKED'):
			out.append('\tAddCommand(%s, %s);%s' % (args[0], args[1], comment))
		elif macro in ('ON_COMMAND_RANGE', 'ON_COMMAND_EX_RANGE', 'ON_CONTROL_RANGE') and macro != 'ON_CONTROL_RANGE':
			out.append('\tAddCommandRange(%s, %s, %s);%s' % (args[0], args[1], args[2], comment))
		elif macro == 'ON_UPDATE_COMMAND_UI':
			out.append('\tAddCommandUpdate(%s, %s);%s' % (args[0], args[1], comment))
		elif macro == 'ON_UPDATE_COMMAND_UI_RANGE':
			out.append('\tAddCommandUpdateRange(%s, %s, %s);%s' % (args[0], args[1], args[2], comment))
		elif macro.startswith('ON_') and macro[3:] in NOTIFY and len(args) == 2:
			out.append('\tAddNotify(%s, mfcless::Notify::%s, %s);%s' % (args[0], NOTIFY[macro[3:]], args[1], comment))
		elif macro in ('ON_NOTIFY', 'ON_NOTIFY_EX') and args and args[0] in NOTIFY:
			out.append('\tAddNotify(%s, mfcless::Notify::%s, %s);%s' % (args[1], NOTIFY[args[0]], args[2], comment))
		elif macro in ('ON_NOTIFY_RANGE', 'ON_NOTIFY_EX_RANGE') and args and args[0] in NOTIFY:
			out.append('\tAddNotifyRange(%s, %s, mfcless::Notify::%s, %s);%s' % (args[1], args[2], NOTIFY[args[0]], args[3], comment))
		elif macro == 'ON_CONTROL_RANGE' and args and args[0] in NOTIFY:
			out.append('\tAddNotifyRange(%s, %s, mfcless::Notify::%s, %s);%s' % (args[1], args[2], NOTIFY[args[0]], args[3], comment))
		elif macro == 'ON_WM_HSCROLL':
			out.append('\tAddScroll(Qt::Horizontal, &%s::OnHScroll);%s' % (cls, comment))
		elif macro == 'ON_WM_VSCROLL':
			out.append('\tAddScroll(Qt::Vertical, &%s::OnVScroll);%s' % (cls, comment))
		elif macro in ('ON_MESSAGE', 'ON_REGISTERED_MESSAGE'):
			out.append('\tAddMessage(%s, %s);%s' % (args[0], args[1], comment))
		else:
			out.append('\t// %s: by hand (Qt event override)%s' % (t, comment))
	out.append('}')
	return '\n'.join(out)

DDX = {
	'DDX_Text': 'ExchangeText', 'DDX_Check': 'ExchangeCheck', 'DDX_Radio': 'ExchangeRadio',
	'DDX_Slider': 'ExchangeSlider', 'DDX_CBIndex': 'ExchangeComboIndex',
}

# Methods of MFC controls, by the Qt type of the member, as Qt calls.
# Values: a replacement for ".Method(" (the arguments follow unchanged).
CONTROL_METHODS = {
	'*': {
		'EnableWindow': '->setEnabled(', 'ShowWindow': '->setVisible(', 'SetFocus': '->setFocus(',
		'Invalidate': '->update(', 'SetRedraw': '->setUpdatesEnabled(', 'IsWindowEnabled': '->isEnabled(',
		'IsWindowVisible': '->isVisible(',
	},
	'QComboBox': {
		'GetCurSel': '->currentIndex(', 'SetCurSel': '->setCurrentIndex(', 'ResetContent': '->clear(',
		'GetCount': '->count(', 'DeleteString': '->removeItem(',
	},
	'QListWidget': {
		'GetCurSel': '->currentRow(', 'SetCurSel': '->setCurrentRow(', 'ResetContent': '->clear(',
		'GetCount': '->count(',
	},
	'QSlider': {
		'SetPos': '->setValue(', 'GetPos': '->value(', 'SetRange': '->setRange(', 'SetTicFreq': '->setTickInterval(',
		'SetPageSize': '->setPageStep(', 'SetLineSize': '->setSingleStep(', 'GetRangeMin': '->minimum(',
		'GetRangeMax': '->maximum(', 'SetRangeMin': '->setMinimum(', 'SetRangeMax': '->setMaximum(',
	},
	'QScrollBar': {
		'SetScrollPos': '->setValue(', 'GetScrollPos': '->value(', 'SetScrollRange': '->setRange(',
	},
	'mfcless::UpDown': {
		'SetPos32': '->SetPos(', 'GetPos32': '->GetPos(', 'SetRange32': '->SetRange(',
	},
	'QAbstractButton': {
		'SetCheck': '->setChecked(', 'GetCheck': '->isChecked(',
	},
	'QLineEdit': {
		'SetLimitText': '->setMaxLength(', 'LimitText': '->setMaxLength(', 'SetReadOnly': '->setReadOnly(',
	},
	'QProgressBar': {
		'SetPos': '->setValue(', 'SetRange32': '->setRange(', 'SetRange': '->setRange(',
	},
}

def member_types(header):
	types = {}
	for m in re.finditer(r'^\s*([\w:]+)\s+((?:\*\s*\w+(?:\[[^\]]*\])?\s*=\s*(?:nullptr|\{\})\s*,?\s*)+);', header, flags=re.M):
		for n in re.findall(r'\*\s*(\w+)', m.group(2)):
			types[n] = m.group(1)
	return types

def twin_cpp(text, header):
	text = re.sub(r'^#include "stdafx.h"\n', '', text, flags=re.M)
	text = re.sub(r'^BEGIN_MESSAGE_MAP\((\w+),\s*(\w+)\)\n(.*?)^END_MESSAGE_MAP\(\)', message_map, text, flags=re.M | re.S)
	text = re.sub(r'^IMPLEMENT_(DYNAMIC|DYNCREATE|SERIAL)\(.*\)\s*\n', '', text, flags=re.M)
	# Data exchange
	text = re.sub(r'void (\w+)::DoDataExchange\(CDataExchange\s*\*\s*\w*\)', r'void \1::DoDataExchange(DataExchange &dx)', text)
	text = re.sub(r'(\w+)::DoDataExchange\(pDX\)', lambda m: BASES.get(m.group(1), m.group(1)) + '::DoDataExchange(dx)', text)
	text = re.sub(r'DDX_Control\(pDX,\s*([^,]+),\s*([^)]+)\)', r'Bind(\1, \2)', text)
	for k, v in DDX.items():
		text = re.sub(k + r'\(pDX,', v + '(dx,', text)
	text = re.sub(r'DDV_MinMax\w+\(pDX,', 'ValidateMinMax(dx,', text)
	text = re.sub(r'pDX->m_bSaveAndValidate', 'dx.save', text)
	# Dialog item helpers
	text = re.sub(r'GetDlgItem\(([^()]+)\)->EnableWindow\(', r'EnableItem(\1, ', text)
	text = re.sub(r'GetDlgItem\(([^()]+)\)->ShowWindow\(', r'ShowItem(\1, ', text)
	text = re.sub(r'\bSetDlgItemText\(', 'SetItemText(', text)
	text = re.sub(r'\bGetDlgItemText\(([^,()]+),\s*([^)]+)\)', r'\2 = GetItemText(\1)', text)
	text = re.sub(r'\bSetDlgItemInt\(([^,()]+),\s*([^,()]+(?:\([^()]*\))?)(?:,\s*\w+)?\)', r'SetItemInt(\1, \2)', text)
	text = re.sub(r'\bGetDlgItemInt\(([^,()]+)(?:,[^)]*)?\)', r'GetItemInt(\1)', text)
	text = re.sub(r'\bCheckDlgButton\(', 'SetChecked(', text)
	text = re.sub(r'\bIsDlgButtonChecked\(', 'IsChecked(', text)
	text = re.sub(r'\bCheckRadioButton\(', 'CheckRadio(', text)
	text = re.sub(r'\bGetCheckedRadioButton\(', 'GetCheckedRadio(', text)
	text = re.sub(r'\bBST_CHECKED\b', '1', text)
	text = re.sub(r'\bBST_UNCHECKED\b', '0', text)
	text = re.sub(r'\bBST_INDETERMINATE\b', '2', text)
	text = re.sub(r'\bSW_SHOW(NA|NOACTIVATE|NORMAL)?\b', 'true', text)
	text = re.sub(r'\bSW_HIDE\b', 'false', text)
	# Handler signatures
	text = re.sub(r'void (\w+)::(On[HV]Scroll)\(UINT\s*\w*,\s*UINT\s*\w*,\s*CScrollBar\s*\*\s*(\w*)\)', r'void \1::\2(mfcless::ScrollCode code, int pos, QWidget *\3)', text)
	text = re.sub(r'void (\w+)::(\w+)\(CCmdUI\s*\*\s*(\w+)\)', r'void \1::\2(mfcless::CommandState &state)', text)
	text = re.sub(r'\bpCmdUI->', 'state.', text)
	text = re.sub(r'(\w+)::(\w+)\(NMHDR\s*\*\s*\w*,\s*LRESULT\s*\*\s*\w*\)', r'\1::\2()', text)
	text = re.sub(r'\*pResult\s*=', 'CurrentNotify().result =', text)
	text = re.sub(r'reinterpret_cast<(?:const )?NMUPDOWN\s*\*>\(pNMHDR\)', '(&CurrentNotify())', text)
	text = re.sub(r'(\w+)->iDelta\b', 'CurrentNotify().delta', text)
	text = re.sub(r'(\w+)->iPos\b', 'CurrentNotify().pos', text)
	# Controls
	types = member_types(header)
	def call(m):
		name, method = m.group(1), m.group(2)
		t = types.get(name)
		if t is None:
			return m.group(0)
		repl = CONTROL_METHODS.get(t, {}).get(method) or CONTROL_METHODS['*'].get(method)
		if method in ('AddString',):
			return 'mfcless::AddItem(%s, ' % name
		if method == 'InsertString':
			return 'mfcless::InsertItem(%s, ' % name
		if method == 'SetItemData':
			return 'mfcless::SetItemData(%s, ' % name
		if method == 'GetItemData':
			return 'mfcless::GetItemData(%s, ' % name
		return name + (repl if repl else '->' + method + '(')
	text = re.sub(r'\b(m_\w+)\.(\w+)\(', call, text)
	text = re.sub(r'\b(m_\w+)\.(\w+)\b', lambda m: (m.group(1) + '->' + m.group(2)) if m.group(1) in types else m.group(0), text)
	for k, v in BASES.items():
		text = re.sub(r'\b' + k + r'::', v + '::', text)
	for a, b in TYPES:
		text = re.sub(a, b, text)
	text = text.replace(' * Notes  : (currently none)', ' * Notes  : mfcless twin of upstream\'s mptrack/' + NAME + '.\n *          ' + DRAFT_MARK)
	if DRAFT_MARK not in text:
		text = '// ' + DRAFT_MARK + '\n' + text
	return text

if __name__ == '__main__':
	import os
	path = sys.argv[1]
	NAME = path.split('mptrack/', 1)[-1]
	src = open(path, encoding='utf-8', errors='surrogateescape').read()
	if path.endswith('.cpp'):
		hdr = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'mptrack', NAME[:-4] + '.h')
		header = open(hdr, encoding='utf-8').read() if os.path.exists(hdr) else ''
		sys.stdout.write(twin_cpp(src, header))
	else:
		sys.stdout.write(twin(src))
