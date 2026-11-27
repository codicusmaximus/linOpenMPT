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
	(r'(\w+)\(UINT\s*(\w*),\s*NMHDR\s*\*\s*\w*,\s*LRESULT\s*\*\s*\w*\)', r'\1(uint32 \2)'),
	(r'\bRGB\(', 'mfcless::Rgb('),
	(r'(?:BOOL|bool) PreTranslateMessage\(MSG\s*\*\s*(\w*)\)( override)?;', r'bool PreTranslateMessage(mfcless::Message *\1) override;'),
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

# OpenMPT's own control classes, members of dialogs: pointers too.
CUSTOM_CONTROLS = [
	'AccessibleButton', 'AccessibleComboBox', 'AccessibleEdit', 'CAdvancedSettingsList', 'CColourEdit',
	'CCustEdit', 'CEQSlider', 'CKeyboardControl', 'CListCtrlEx', 'CMFCListCtrlEx', 'CModControlBar',
	'CModTabCtrl', 'CModTree', 'CNoteMapWnd', 'COctaveEdit', 'COrderList', 'CRippleBitmap', 'CStereoVU',
	'CTuningRatioMapWnd', 'CTuningTreeCtrl', 'CVuMeter', 'ColorPickerButton', 'PluginComboBox', 'CNumberEdit',
	'CInlineEdit', 'CSplitterWnd',
]

# Base classes: MFC windows become the Qt widget that plays the same role.
# Windows become mfcless::Window<the Qt class>, which calls upstream's
# handlers (OnLButtonDown, OnPaint, ...) for the Qt events.
BASES = {
	'CDialog': 'DialogBase', 'CDialogEx': 'DialogBase', 'CPropertyPage': 'PropertyPage',
	'CFormView': 'DialogBase, public mfcless::DocumentView', 'CPropertySheet': 'mfcless::Window<QDialog>',
	'CWnd': 'mfcless::Window<QWidget>', 'CView': 'mfcless::Window<QWidget>, public mfcless::DocumentView',
	'CScrollView': 'mfcless::Window<QAbstractScrollArea>, public mfcless::DocumentView',
	'CMDIChildWnd': 'mfcless::Window<QMdiSubWindow>', 'CMDIFrameWnd': 'mfcless::Window<QMainWindow>', 'CFrameWnd': 'mfcless::Window<QMainWindow>',
	'CEdit': 'mfcless::Window<QLineEdit>', 'CStatic': 'mfcless::Window<QLabel>', 'CButton': 'mfcless::Window<QPushButton>',
	'CComboBox': 'mfcless::Window<QComboBox>', 'CListBox': 'mfcless::Window<QListWidget>', 'CTreeCtrl': 'mfcless::Window<QTreeWidget>',
	'CListCtrl': 'mfcless::Window<QTreeWidget>', 'CMFCListCtrl': 'mfcless::Window<QTreeWidget>', 'CMFCListCtrlEx': 'mfcless::Window<QTreeWidget>',
	'CTabCtrl': 'mfcless::Window<QTabBar>', 'CSliderCtrl': 'mfcless::Window<QSlider>', 'CSpinButtonCtrl': 'mfcless::UpDown',
	'CToolBar': 'mfcless::Window<QToolBar>', 'CToolBarCtrl': 'mfcless::Window<QToolBar>', 'CDialogBar': 'mfcless::Window<QWidget>',
	'CReBar': 'mfcless::Window<QWidget>',
	'CToolTipCtrl': 'QObject', 'COleDropTarget': 'QObject', 'CMultiDocTemplate': 'QObject',
	'CDocManager': 'QObject', 'CDocument': 'QObject', 'CImageList': 'QObject',
	'CSplitterWnd': 'QSplitter', 'CMFCDynamicLayout': 'QObject',
}

# The handlers mfcless::Window calls; a call of an MFC class's one is the
# Window base's (Window::OnPaint()).
WINDOW_HANDLERS = r'On(?:[LRMX]Button(?:Down|Up|DblClk)|MouseMove|MouseLeave|MouseWheel|MouseHWheel|ContextMenu|KeyDown|KeyUp|SysKeyDown|SysKeyUp|Char|SysChar|SetFocus|KillFocus|Paint|Size|Move|ShowWindow|Timer|DropFiles|Close)'
WINDOW_MESSAGES = ['LBUTTONDOWN', 'LBUTTONUP', 'LBUTTONDBLCLK', 'RBUTTONDOWN', 'RBUTTONUP', 'RBUTTONDBLCLK',
	'MBUTTONDOWN', 'MBUTTONUP', 'MBUTTONDBLCLK', 'XBUTTONDOWN', 'XBUTTONUP', 'XBUTTONDBLCLK', 'MOUSEMOVE', 'MOUSELEAVE',
	'MOUSEWHEEL', 'MOUSEHWHEEL', 'CONTEXTMENU', 'KEYDOWN', 'KEYUP', 'SYSKEYDOWN', 'SYSKEYUP', 'CHAR', 'SYSCHAR',
	'SETFOCUS', 'KILLFOCUS', 'PAINT', 'SIZE', 'MOVE', 'SHOWWINDOW', 'TIMER', 'DROPFILES', 'CLOSE']

INPUT_MESSAGES = {'KEYDOWN': 'KeyDown', 'KEYUP': 'KeyUp', 'SYSKEYDOWN': 'SysKeyDown', 'SYSKEYUP': 'SysKeyUp',
	'CHAR': 'Char', 'SYSCHAR': 'SysChar', 'MOUSEMOVE': 'MouseMove', 'LBUTTONDOWN': 'LButtonDown', 'LBUTTONUP': 'LButtonUp',
	'LBUTTONDBLCLK': 'LButtonDblClk', 'RBUTTONDOWN': 'RButtonDown', 'RBUTTONUP': 'RButtonUp', 'RBUTTONDBLCLK': 'RButtonDblClk',
	'MBUTTONDOWN': 'MButtonDown', 'MBUTTONUP': 'MButtonUp', 'MOUSEWHEEL': 'MouseWheel', 'XBUTTONDOWN': 'XButtonDown',
	'XBUTTONUP': 'XButtonUp', 'MOUSEHWHEEL': 'MouseHWheel', 'KEYFIRST': 'KeyFirst', 'KEYLAST': 'KeyLast',
	'MOUSEFIRST': 'MouseFirst', 'MOUSELAST': 'MouseLast'}

MOUSE_KEYS = {'MK_LBUTTON': 'Left', 'MK_RBUTTON': 'Right', 'MK_MBUTTON': 'Middle', 'MK_SHIFT': 'Shift',
	'MK_CONTROL': 'Control', 'MK_XBUTTON1': 'X1', 'MK_XBUTTON2': 'X2'}

def keys(text):
	text = re.sub(r'\bVK_([A-Z0-9_]+)\b', r'mfcless::vk::\1', text)
	text = re.sub(r'\b(MK_[A-Z0-9]+)\b', lambda m: 'mfcless::MouseKey::' + MOUSE_KEYS[m.group(1)] if m.group(1) in MOUSE_KEYS else m.group(0), text)
	text = re.sub(r'\bWHEEL_DELTA\b', 'mfcless::WheelDelta', text)
	text = re.sub(r'\bWM_(' + '|'.join(INPUT_MESSAGES) + r')\b', lambda m: 'mfcless::msg::' + INPUT_MESSAGES[m.group(1)], text)
	text = re.sub(r'\bMSG\s*\*\s*(\w+)', r'mfcless::Message *\1', text)
	text = re.sub(r'\bconst MSG\s*&', 'const mfcless::Message &', text)
	text = re.sub(r'void OnDropFiles\(HDROP\s*\w*\)', 'void OnDropFiles(const QStringList &files)', text)
	return text

def twin(text):
	for k, v in BASES.items():
		text = re.sub(r'(:\s*(?:public|protected|private)\s+)' + k + r'\b', r'\1' + v, text)
	# Classes now based on a Qt class get Q_OBJECT.
	text = re.sub(r'^(class \w+\s*:\s*public\s+(?:Q\w+|mfcless::Window<\w+>|DialogBase|PropertyPage|ResizableDialog|CAbstractVstEditor|CModScrollView|CModControlDlg|CModControlView|CModTabCtrl)[^\n{]*\n\{\n)', r'\1\tQ_OBJECT\n', text, flags=re.M)
	# Control members become pointers to the widgets the dialog template
	# creates (or that replace them): "\tCEdit m_a, m_b[4];" ->
	# "\tQLineEdit *m_a = nullptr, *m_b[4] = {};"
	def members(qt_type):
		def repl(m):
			out = []
			for n in m.group(2).split(','):
				n = n.strip()
				out.append('*%s = {}' % n if '[' in n else '*%s = nullptr' % n)
			return '%s%s %s;' % (m.group(1), qt_type, ', '.join(out))
		return repl
	for k, v in list(CONTROLS.items()) + [(c, c) for c in CUSTOM_CONTROLS]:
		text = re.sub(r'^(\s*)' + re.escape(k) + r'\s+((?:\w+(?:\[[^\]]+\])?)(?:\s*,\s*\w+(?:\[[^\]]+\])?)*);', members(v), text, flags=re.M)
		if k in CONTROLS:
			text = re.sub(r'\b' + k + r'\s*&', v + ' &', text)
			text = re.sub(r'\b' + k + r'\s*\*', v + ' *', text)
	text = keys(text)
	for a, b in TYPES:
		text = re.sub(a, b, text)
	text = re.sub(r'\bafx_msg\s+', '', text)
	text = re.sub(r'^\s*DECLARE_(?:DYNCREATE|SERIAL)\((\w+)\)\s*\n', r'public:\n\tstatic const mfcless::ViewClass class\1;\n', text, flags=re.M)
	text = re.sub(r'^\s*DECLARE_DYNAMIC\(\w+\)\s*\n', '', text, flags=re.M)
	text = re.sub(r'^\s*//\{\{AFX_\w+(\(\w+\))?\s*\n', '', text, flags=re.M)
	text = re.sub(r'^\s*//\}\}AFX_\w+\s*\n', '', text, flags=re.M)
	text = re.sub(r'void DoDataExchange\(CDataExchange\s*\*\s*\w*\)( override)?;', 'void DoDataExchange(DataExchange &dx) override;', text)
	text = re.sub(r'^(\s*)DECLARE_MESSAGE_MAP\(\)', r'\1void RegisterMessageMap() override;', text, flags=re.M)
	text = re.sub(r'void (On[HV]Scroll)\(uint32\s*\w*,\s*uint32\s*\w*,\s*CScrollBar\s*\*\s*\w*\)( override)?;', r'void \1(mfcless::ScrollCode code, int pos, QWidget *control);', text)
	text = re.sub(r'void (\w+)\(CCmdUI\s*\*\s*\w*\)', r'void \1(mfcless::CommandState &state)', text)
	# Forward declarations of the Qt classes now named, outside the namespace.
	used = sorted(set(re.findall(r'\b(Q[A-Z][A-Za-z]+)\b', text)) - {'Q_OBJECT'})
	# Base classes must be complete: include them rather than declare them.
	bases = sorted(set(re.findall(r':\s*public\s+(?:mfcless::Window<)?(Q[A-Z][A-Za-z]+)\b', text)))
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
	# Qt-based classes with a message map also take commands and notifications.
	def add_map(m):
		head, body = m.group(1), m.group(2)
		if 'RegisterMessageMap' in body and not re.search(r'public\s+(DialogBase|PropertyPage|ResizableDialog|CModControlDlg|CModControlView|CModScrollView|CAbstractVstEditor|mfcless::Window)\b', head) and 'MessageMap' not in head and 'DocumentView' not in head:
			head = head.rstrip() + ', public mfcless::MessageMap'
			return head + '\n{' + body
		return m.group(0)
	text = re.sub(r'^(class \w+\s*:\s*public\s+Q\w+[^\n{]*)\n\{(.*?\n\};)', add_map, text, flags=re.M | re.S)
	if ('mfcless::DocumentView' in text or 'mfcless::ViewClass' in text) and '"DocumentView.h"' not in text:
		text = text.replace('#include "openmpt/all/BuildSettings.hpp"\n', '#include "openmpt/all/BuildSettings.hpp"\n#include "DocumentView.h"\n', 1)
	if 'mfcless::MessageMap' in text and '"mfcless/MessageMap.h"' not in text:
		text = text.replace('#include "openmpt/all/BuildSettings.hpp"\n', '#include "openmpt/all/BuildSettings.hpp"\n#include "mfcless/MessageMap.h"\n', 1)
	if 'mfcless::Window<' in text and '"mfcless/Window.h"' not in text:
		text = text.replace('#include "openmpt/all/BuildSettings.hpp"\n', '#include "openmpt/all/BuildSettings.hpp"\n#include "mfcless/Window.h"\n', 1)
	elif re.search(r'mfcless::(vk|MouseKey|msg|Message)\b', text) and '"mfcless/Keyboard.h"' not in text:
		text = text.replace('#include "openmpt/all/BuildSettings.hpp"\n', '#include "openmpt/all/BuildSettings.hpp"\n#include "mfcless/Keyboard.h"\n', 1)
	if re.search(r'mfcless::(UpDown|TabControl|GroupFrame)', text) and '"mfcless/Controls.h"' not in text:
		text = text.replace('#include "openmpt/all/BuildSettings.hpp"\n', '#include "openmpt/all/BuildSettings.hpp"\n#include "mfcless/Controls.h"\n', 1)
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
		elif macro.startswith('ON_WM_') and macro[6:] in WINDOW_MESSAGES:
			out.append('\t// %s (mfcless::Window)%s' % (t, comment))
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
	text = re.sub(r'\b(?:' + '|'.join(BASES) + r')::(' + WINDOW_HANDLERS + r')\(', r'Window::\1(', text)
	text = re.sub(r'(?:BOOL|bool) (\w+)::PreTranslateMessage\(MSG\s*\*\s*(\w*)\)', r'bool \1::PreTranslateMessage(mfcless::Message *\2)', text)
	text = re.sub(r'\b(?:' + '|'.join(BASES) + r')::PreTranslateMessage\((\w+)\)', r'WindowBase::PreTranslateMessage(\1)', text)
	for k, v in BASES.items():
		text = re.sub(r'\b' + k + r'::', v.split(',')[0] + '::', text)
	text = keys(text)
	for a, b in TYPES:
		text = re.sub(a, b, text)
	if not PATCH_MODE:
		text = text.replace(' * Notes  : (currently none)', ' * Notes  : mfcless twin of upstream\'s mptrack/' + NAME + '.\n *          ' + DRAFT_MARK)
		if DRAFT_MARK not in text:
			text = '// ' + DRAFT_MARK + '\n' + text
	return text

# --patch: rewrite an upstream file in place in the mfcless worktree (for
# files patched rather than twinned): no draft mark.
PATCH_MODE = '--patch' in sys.argv

if __name__ == '__main__':
	import os
	args = [a for a in sys.argv[1:] if not a.startswith('--')]
	path = args[0]
	NAME = path.split('mptrack/', 1)[-1]
	src = open(path, encoding='utf-8', errors='surrogateescape').read()
	if path.endswith('.cpp'):
		hdr = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'mptrack', NAME[:-4] + '.h')
		header = open(hdr, encoding='utf-8').read() if os.path.exists(hdr) else ''
		out = twin_cpp(src, header)
		if PATCH_MODE:
			open(path, 'w', encoding='utf-8', errors='surrogateescape').write(out)
		else:
			sys.stdout.write(out)
	else:
		sys.stdout.write(twin(src))
