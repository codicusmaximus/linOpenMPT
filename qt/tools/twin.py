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
	(r'(\w+)\((?:LP)?NMHDR\s*\*?\s*(?:/\*\s*\w*\s*\*/|\w*)\s*(?:=\s*nullptr)?\s*,\s*(?:LRESULT|intptr_t)\s*\*\s*(?:/\*\s*\w*\s*\*/|\w*)\s*(?:=\s*nullptr)?\)', r'\1()'),
	(r'\bLPDRAWITEMSTRUCT\b', 'mfcless::DrawItemInfo *'),
	(r'\bDRAWITEMSTRUCT\b', 'mfcless::DrawItemInfo'),
	(r'\bODS_(SELECTED|GRAYED|DISABLED|CHECKED|FOCUS|DEFAULT)\b', lambda m: 'mfcless::ods::' + m.group(1).capitalize()),
	(r'\bODT_(MENU|LISTBOX|COMBOBOX|BUTTON|STATIC)\b', lambda m: 'mfcless::odt::' + {'MENU': 'Menu', 'LISTBOX': 'ListBox', 'COMBOBOX': 'ComboBox', 'BUTTON': 'Button', 'STATIC': 'Static'}[m.group(1)]),
	(r'\bCF_TEXT\b', 'mfcless::clip::Text'),
	(r'\bCF_UNICODETEXT\b', 'mfcless::clip::UnicodeText'),
	(r'\bCF_WAVE\b', 'mfcless::clip::Wave'),
	(r'\bIsClipboardFormatAvailable\(', 'mfcless::clip::IsFormatAvailable('),
	(r'\bRegisterClipboardFormat\(_T\(("[^"]*")\)\)', r'mfcless::clip::RegisterFormat(QStringLiteral(\1))'),
	(r'\bTVI_ROOT\b', 'nullptr'),
	(r'\bWINDOWPLACEMENT\b', 'mfcless::WindowPlacement'),
	(r'\bSBPS_(STRETCH|NORMAL|NOBORDERS|POPOUT|DISABLED)\b', lambda m: 'mfcless::pane::' + {'STRETCH': 'Stretch', 'NORMAL': 'Normal', 'NOBORDERS': 'NoBorders', 'POPOUT': 'PopOut', 'DISABLED': 'Disabled'}[m.group(1)]),
	(r'\bGetCurrentThreadId\(\)', 'mfcless::CurrentThreadId()'),
	(r'\bCStatusBar\b', 'mfcless::StatusBar'),
	# Path literals: Windows' separators become '/'.
	(r'\bP_\("((?:[^"\\]|\\.)*)"\)', lambda m: 'P_("' + m.group(1).replace('\\\\', '/') + '")'),
	(r'dynamic_cast<CPropertySheet \*>\(GetParent\(\)\)', 'GetSheet()'),
	(r'\bCPropertySheet\b(?!\s*\{)', 'PropertySheet'),
	(r'\bSleep\(', 'mfcless::SleepMs('),
	(r'\bASSERT_VALID\((\w+)\)', r'MPT_ASSERT(\1)'),
	(r'\b(\w+)\.Format\((U_|_T|UL_)\(', r'\1 = mfcless::PrintF(UL_('),
	(r'\b(\w+)\.AppendFormat\((U_|_T|UL_)\(', r'\1 += mfcless::PrintF(UL_('),
	(r'\b(\w+)\.AppendChar\(', r'\1.push_back('),
	(r"\b_T\('(\\?.)'\)", r"u8'\1'"),
	(r'\.GetString\(\)', '.c_str()'),
	(r'\bCMDIChildWnd\s*\*', 'CChildFrame *'),
	(r'\bmpt::PathString::FromCString\(', 'mpt::PathString::FromUnicode('),
	(r'\.ToCString\(\)', '.ToUnicode()'),
	(r'\bPOSITION (\w+) = GetFirstViewPosition\(\);', r'mfcless::ViewPosition \1 = GetFirstViewPosition();'),
	(r'\bwhile\s*\((pos)\s*!=\s*(?:NULL|nullptr)\)', r'while(\1)'),
	(r'\bBeginWaitCursor\(\);', 'BeginWaitCursor();'),
	(r'\bNORM_IGNORE(CASE|NONSPACE|SYMBOLS|KANATYPE|WIDTH)\b', lambda m: 'mfcless::compare::Ignore' + {'CASE': 'Case', 'NONSPACE': 'NonSpace', 'SYMBOLS': 'Symbols', 'KANATYPE': 'KanaType', 'WIDTH': 'Width'}[m.group(1)]),
	(r'\bSORT_DIGITSASNUMBERS\b', 'mfcless::compare::DigitsAsNumbers'),
	(r'\bCImageList\b(?!Ex)', 'CImageListEx'),
	(r'\bHWND HWND\b', 'QWidget *'),
	(r'\bCOleDataObject\s*\*\s*', 'const QMimeData *'),
	(r'\bDROPEFFECT_(NONE|COPY|MOVE|LINK|SCROLL)\b', lambda m: 'mfcless::drop::' + m.group(1).capitalize()),
	(r'\bDROPEFFECT\b', 'uint32'),
	(r'\bHANDLE\b', 'void *'),
	(r'\bCView\s*\*', 'mfcless::DocumentView *'),
	(r'\bMSG (\w+) = \{(\w+), (\w+), (\w+), (\w+), 0, \{\}\};', r'mfcless::Message \1{\2, \3, \4, \5, nullptr};'),
	(r'\btimeGetTime\(\)', 'mfcless::TickCount()'),
	(r'\bNCCALCSIZE_PARAMS\s*\*\s*', 'mfcless::Rect *'),
	(r'HRESULT get_accName\(VARIANT \w+, BSTR \*\w+\) override;', 'QString GetAccessibleName() override;'),
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
	(r'\bLPC?RECT\b', 'mfcless::Rect *'),
	(r'\bLPPOINT\b', 'mfcless::Point *'),
	(r'\b(?:CRect|RECT)\b', 'mfcless::Rect'),
	(r'\b(?:CPoint|POINT)\b', 'mfcless::Point'),
	(r'\b(?:CSize|SIZE)\b', 'mfcless::Size'),
	(r'\b(?:CDC|CMemDC)\b', 'mfcless::DC'),
	(r'\b(?:CPaintDC|CClientDC|CWindowDC)\b', 'mfcless::PaintDC'),
	(r'\bHDC\b', 'mfcless::DC *'),
	(r'\bCPen\b', 'mfcless::Pen'),
	(r'\bHPEN\b', 'mfcless::Pen'),
	(r'\bCBrush\b', 'mfcless::Brush'),
	(r'\bHBRUSH\b', 'mfcless::Brush'),
	(r'\bCBitmap\b', 'mfcless::Bitmap'),
	(r'\bHBITMAP\b', 'mfcless::Bitmap *'),
	(r'\bHFONT\b', 'QFont'),
	(r'\bHICON\b', 'QIcon'),
	(r'\bHCURSOR\b', 'QCursor'),
	(r'\bGetRValue\(', 'mfcless::RedOf('),
	(r'\bGetGValue\(', 'mfcless::GreenOf('),
	(r'\bGetBValue\(', 'mfcless::BlueOf('),
	(r'\bDT_(LEFT|CENTER|RIGHT|VCENTER|BOTTOM|TOP|WORDBREAK|SINGLELINE|EXPANDTABS|NOCLIP|CALCRECT|NOPREFIX|END_ELLIPSIS|PATH_ELLIPSIS|WORD_ELLIPSIS|HIDEPREFIX|EDITCONTROL)\b', lambda m: 'mfcless::dt::' + {'LEFT': 'Left', 'CENTER': 'Center', 'RIGHT': 'Right', 'VCENTER': 'VCenter', 'BOTTOM': 'Bottom', 'TOP': 'Top', 'WORDBREAK': 'WordBreak', 'SINGLELINE': 'SingleLine', 'EXPANDTABS': 'ExpandTabs', 'NOCLIP': 'NoClip', 'CALCRECT': 'CalcRect', 'NOPREFIX': 'NoPrefix', 'END_ELLIPSIS': 'EndEllipsis', 'PATH_ELLIPSIS': 'PathEllipsis', 'WORD_ELLIPSIS': 'WordEllipsis', 'HIDEPREFIX': 'HidePrefix', 'EDITCONTROL': 'EditControl'}[m.group(1)]),
	(r'\b(SRCCOPY|SRCPAINT|SRCAND|SRCINVERT|NOTSRCCOPY|PATCOPY|PATINVERT|DSTINVERT|BLACKNESS|WHITENESS)\b', lambda m: 'mfcless::rop::' + {'SRCCOPY': 'SrcCopy', 'SRCPAINT': 'SrcPaint', 'SRCAND': 'SrcAnd', 'SRCINVERT': 'SrcInvert', 'NOTSRCCOPY': 'NotSrcCopy', 'PATCOPY': 'PatCopy', 'PATINVERT': 'PatInvert', 'DSTINVERT': 'DstInvert', 'BLACKNESS': 'Blackness', 'WHITENESS': 'Whiteness'}[m.group(1)]),
	(r'\bPS_(SOLID|DASH|DOT|DASHDOT|DASHDOTDOT|NULL|INSIDEFRAME)\b', lambda m: 'mfcless::pen::' + {'SOLID': 'Solid', 'DASH': 'Dash', 'DOT': 'Dot', 'DASHDOT': 'DashDot', 'DASHDOTDOT': 'DashDotDot', 'NULL': 'Null', 'INSIDEFRAME': 'InsideFrame'}[m.group(1)]),
	(r'\bSetBkMode\(TRANSPARENT\)', 'SetBkMode(mfcless::bk::Transparent)'),
	(r'\bSetBkMode\(OPAQUE\)', 'SetBkMode(mfcless::bk::Opaque)'),
	(r'\bETO_(OPAQUE|CLIPPED)\b', lambda m: 'mfcless::eto::' + m.group(1).capitalize()),
	(r'\b(BDR_RAISEDOUTER|BDR_SUNKENOUTER|BDR_RAISEDINNER|BDR_SUNKENINNER|EDGE_RAISED|EDGE_SUNKEN|EDGE_ETCHED|EDGE_BUMP|BF_LEFT|BF_TOP|BF_RIGHT|BF_BOTTOM|BF_RECT|BF_MIDDLE|BF_SOFT|BF_ADJUST|BF_FLAT|BF_MONO)\b', lambda m: 'mfcless::edge::' + {'BDR_RAISEDOUTER': 'RaisedOuter', 'BDR_SUNKENOUTER': 'SunkenOuter', 'BDR_RAISEDINNER': 'RaisedInner', 'BDR_SUNKENINNER': 'SunkenInner', 'EDGE_RAISED': 'Raised', 'EDGE_SUNKEN': 'Sunken', 'EDGE_ETCHED': 'Etched', 'EDGE_BUMP': 'Bump', 'BF_LEFT': 'Left', 'BF_TOP': 'Top', 'BF_RIGHT': 'Right', 'BF_BOTTOM': 'Bottom', 'BF_RECT': 'Rect', 'BF_MIDDLE': 'Middle', 'BF_SOFT': 'Soft', 'BF_ADJUST': 'Adjust', 'BF_FLAT': 'Flat', 'BF_MONO': 'Mono'}[m.group(1)]),
	(r'\bSelectStockObject\((NULL_PEN|BLACK_PEN|WHITE_PEN|DC_PEN|NULL_BRUSH|BLACK_BRUSH|WHITE_BRUSH|GRAY_BRUSH|DC_BRUSH|DEFAULT_GUI_FONT|HOLLOW_BRUSH)\)', lambda m: 'SelectStockObject(mfcless::Stock::' + {'NULL_PEN': 'NullPen', 'BLACK_PEN': 'BlackPen', 'WHITE_PEN': 'WhitePen', 'DC_PEN': 'DCPen', 'NULL_BRUSH': 'NullBrush', 'HOLLOW_BRUSH': 'NullBrush', 'BLACK_BRUSH': 'BlackBrush', 'WHITE_BRUSH': 'WhiteBrush', 'GRAY_BRUSH': 'GrayBrush', 'DC_BRUSH': 'DCBrush', 'DEFAULT_GUI_FONT': 'DefaultGuiFont'}[m.group(1)] + ')'),
	(r'\bSIZE_(RESTORED|MINIMIZED|MAXIMIZED)\b', lambda m: 'mfcless::size::' + m.group(1).capitalize()),
	(r'\bCFont\b', 'QFont'),
	(r'\b(?:const\s+)?CRuntimeClass\s*\*', 'const mfcless::ViewClass *'),
	(r'\bRUNTIME_CLASS\((\w+)\)', r'(&\1::class\1)'),
	(r'\bTOOLINFO\b', 'mfcless::ToolInfo'),
	(r'\bSetScrollSizes\(MM_TEXT,\s*', 'SetScrollSizes('),
	(r'\b(QFont|QIcon|QCursor|mfcless::Pen|mfcless::Brush)(\s+\w+)\s*=\s*(?:nullptr|NULL)\s*;', r'\1\2;'),
	(r'\b(QFont|QIcon|QCursor|mfcless::Pen|mfcless::Brush)(\s+\w+)\s*=\s*(?:nullptr|NULL)\s*,\s*(\w+)\s*=\s*(?:nullptr|NULL)\s*;', r'\1\2, \3;'),
	(r'\b(?:CMenu|HMENU)\s*\*?\s*(\w+)\s*=\s*::CreatePopupMenu\(\)', r'mfcless::Menu \1 = mfcless::Menu::CreatePopup()'),
	(r'\bCMenu\s*\*\s*', 'mfcless::Menu '),
	(r'\bHMENU\b', 'mfcless::Menu'),
	(r'\bCMenu\b', 'mfcless::Menu'),
	(r'::(AppendMenu|InsertMenu|DeleteMenu|RemoveMenu|ModifyMenu|CheckMenuItem|EnableMenuItem|GetMenuItemCount|GetMenuItemID|GetSubMenu|SetMenuDefaultItem)\((\w+),\s*', lambda m: m.group(2) + '.' + {'SetMenuDefaultItem': 'SetDefaultItem'}.get(m.group(1), m.group(1)) + '('),
	(r'(?<![.>\w])(AppendMenu|InsertMenu|CheckMenuItem|EnableMenuItem)\((h\w*Menu\w*|\w*[mM]enu),\s*', r'\2.\1('),
	(r'::TrackPopupMenu\((\w+),\s*([^,]+),\s*([^,]+),\s*([^,]+),\s*0,\s*(?:m_hWnd|\*?this|GetSafeHwnd\(\)),\s*(?:NULL|nullptr)\)', r'\1.TrackPopupMenu(\2, \3, \4, this)'),
	(r'::DestroyMenu\((\w+)\)', r'\1.DestroyMenu()'),
	(r'reinterpret_cast<(?:UINT_PTR|uintptr_t)>\((\w+)(?:\.m_hMenu|->m_hMenu|\.GetSafeHmenu\(\)|->GetSafeHmenu\(\))?\)', r'\1'),
	(r'\bMF_(STRING|ENABLED|UNCHECKED|BYCOMMAND|GRAYED|DISABLED|CHECKED|POPUP|MENUBARBREAK|MENUBREAK|BYPOSITION|SEPARATOR)\b', lambda m: 'mfcless::menu::' + {'STRING': 'String', 'ENABLED': 'Enabled', 'UNCHECKED': 'Unchecked', 'BYCOMMAND': 'ByCommand', 'GRAYED': 'Grayed', 'DISABLED': 'Disabled', 'CHECKED': 'Checked', 'POPUP': 'Popup', 'MENUBARBREAK': 'MenuBarBreak', 'MENUBREAK': 'MenuBreak', 'BYPOSITION': 'ByPosition', 'SEPARATOR': 'Separator'}[m.group(1)]),
	(r'\bTPM_(LEFTALIGN|LEFTBUTTON|TOPALIGN|RIGHTBUTTON|CENTERALIGN|RIGHTALIGN|VCENTERALIGN|BOTTOMALIGN|NONOTIFY|RETURNCMD)\b', lambda m: 'mfcless::menu::' + {'LEFTALIGN': 'LeftAlign', 'LEFTBUTTON': 'LeftButton', 'TOPALIGN': 'TopAlign', 'RIGHTBUTTON': 'RightButton', 'CENTERALIGN': 'CenterAlign', 'RIGHTALIGN': 'RightAlign', 'VCENTERALIGN': 'VCenterAlign', 'BOTTOMALIGN': 'BottomAlign', 'NONOTIFY': 'NoNotify', 'RETURNCMD': 'ReturnCmd'}[m.group(1)]),
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
	'CListCtrl': 'mfcless::ListCtrl', 'CTreeCtrl': 'QTreeWidget', 'CTabCtrl': 'mfcless::TabControl', 'CCheckListBox': 'QListWidget',
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
	'CFormView': 'DialogBase, public mfcless::DocumentView', 'CPropertySheet': 'PropertySheet',
	'CWnd': 'mfcless::Window<QWidget>', 'CView': 'mfcless::Window<QWidget>, public mfcless::DocumentView',
	'CScrollView': 'mfcless::Window<QAbstractScrollArea>, public mfcless::DocumentView',
	'CMDIChildWnd': 'mfcless::Window<QMdiSubWindow>', 'CMDIFrameWnd': 'mfcless::Window<QMainWindow>', 'CFrameWnd': 'mfcless::Window<QMainWindow>',
	'CEdit': 'mfcless::Window<QLineEdit>', 'CStatic': 'mfcless::Window<QLabel>', 'CButton': 'mfcless::Button',
	'CComboBox': 'mfcless::Window<QComboBox>', 'CListBox': 'mfcless::Window<QListWidget>', 'CTreeCtrl': 'mfcless::Window<QTreeWidget>',
	'CListCtrl': 'mfcless::ListCtrl', 'CMFCListCtrl': 'mfcless::ListCtrl', 'CMFCListCtrlEx': 'CMFCListCtrlEx',
	'CTabCtrl': 'mfcless::Window<QTabBar>', 'CSliderCtrl': 'mfcless::Window<QSlider>', 'CSpinButtonCtrl': 'mfcless::UpDown',
	'CToolBar': 'mfcless::Window<QToolBar>', 'CToolBarCtrl': 'mfcless::Window<QToolBar>', 'CDialogBar': 'mfcless::Window<QWidget>',
	'CReBar': 'mfcless::Window<QWidget>',
	'CToolTipCtrl': 'QObject', 'COleDropTarget': 'mfcless::DropTarget', 'CMultiDocTemplate': 'QObject',
	'CDocManager': 'QObject', 'CDocument': 'mfcless::Document', 'CImageList': 'QObject',
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

POINTER_HANDLES = {'HTREEITEM': 'QTreeWidgetItem', 'HWND': 'QWidget', 'HDC': 'mfcless::DC', 'HBITMAP': 'mfcless::Bitmap'}

def pointer_handles(text):
	# "HWND a = nullptr, b = nullptr;" -> "QWidget *a = nullptr, *b = nullptr;"
	def repl(m):
		decls = [d.strip() for d in m.group(3).split(',')]
		return '%s%s %s;' % (m.group(1), POINTER_HANDLES[m.group(2)], ', '.join('*' + d for d in decls))
	return re.sub(r'^(\s*)(' + '|'.join(POINTER_HANDLES) + r')\s+(\w+(?:\s*=\s*\w+)?(?:\s*,\s*\w+(?:\s*=\s*\w+)?)+);', repl, text, flags=re.M)

def twin(text):
	text = pointer_handles(text)
	for k, v in BASES.items():
		text = re.sub(r'(:\s*(?:public|protected|private)\s+)' + k + r'\b', r'\1' + v, text)
	# Classes now based on a Qt class get Q_OBJECT.
	text = re.sub(r'^(class \w+\s*:\s*public\s+(?:Q\w+|mfcless::Window<\w+>|mfcless::Button|mfcless::ListCtrl|mfcless::UpDown|CListCtrlEx|CMFCListCtrlEx|DialogBase|PropertyPage|ResizableDialog|CAbstractVstEditor|CModScrollView|CModControlDlg|CModControlView|CModTabCtrl)[^\n{]*\n\{\n)', r'\1\tQ_OBJECT\n', text, flags=re.M)
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
	# Base classes in constructors' initialiser lists.
	for k, v in BASES.items():
		base = v.split(',')[0]
		text = re.sub(r'([:,]\s*)' + k + r'(\s*[({])', lambda m: m.group(1) + base + m.group(2), text)
	text = keys(text)
	for a, b in TYPES:
		text = re.sub(a, b, text)
	text = dialog_helpers(text)
	text = member_access(text, member_types(text))
	text = re.sub(r'\bafx_msg\s+', '', text)
	text = re.sub(r'^\s*DECLARE_(?:DYNCREATE|SERIAL)\((\w+)\)\s*\n', r'public:\n\tstatic const mfcless::ViewClass class\1;\n\tconst mfcless::ViewClass *GetRuntimeClass() const override { return &class\1; }\n', text, flags=re.M)
	text = re.sub(r'^\s*DECLARE_DYNAMIC\(\w+\)\s*\n', '', text, flags=re.M)
	text = re.sub(r'^\s*//\{\{AFX_\w+(\(\w+\))?\s*\n', '', text, flags=re.M)
	text = re.sub(r'^\s*//\}\}AFX_\w+\s*\n', '', text, flags=re.M)
	text = re.sub(r'void DoDataExchange\(CDataExchange\s*\*\s*\w*\)( override)?;', 'void DoDataExchange(DataExchange &dx) override;', text)
	text = re.sub(r'^(\s*)DECLARE_MESSAGE_MAP\(\)', r'\1void RegisterMessageMap() override;', text, flags=re.M)
	text = re.sub(r'void (On[HV]Scroll)\(uint32\s*\w*,\s*uint32\s*\w*,\s*CScrollBar\s*\*\s*\w*\)( override)?;', r'void \1(mfcless::ScrollCode code, int pos, QWidget *control);', text)
	text = re.sub(r'void (\w+)\(CCmdUI\s*\*\s*\w*\)', r'void \1(mfcless::CommandState &state)', text)
	# Forward declarations of the Qt classes now named, outside the namespace.
	# Controls that a dialog binds are made by it with a parent: classes of
	# controls without a constructor of their own take the base's.
	def add_ctor(m):
		name, base, body = m.group(1), m.group(2), m.group(3)
		if re.search(r'\b' + name + r'\s*\(', body):
			return m.group(0)
		return m.group(0).replace('\tQ_OBJECT\n', '\tQ_OBJECT\npublic:\n\tusing ' + base + '::' + base.split('::')[-1] + ';\n', 1)
	text = re.sub(r'^class (\w+)\s*:\s*public\s+(mfcless::Button|mfcless::ListCtrl|CListCtrlEx|CMFCListCtrlEx)\s*\n\{\n(.*?)\n\};', add_ctor, text, flags=re.M | re.S)
	used = sorted(set(re.findall(r'\b(Q[A-Z][A-Za-z]+)\b', text)) - {'Q_OBJECT', 'QStringList'})
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
	if re.search(r'mfcless::(DC|PaintDC|Pen|Brush|Bitmap|dt|rop|pen|bk|eto|edge|Stock)\b', text) and '"mfcless/Graphics.h"' not in text:
		text = text.replace('#include "openmpt/all/BuildSettings.hpp"\n', '#include "openmpt/all/BuildSettings.hpp"\n#include "mfcless/Graphics.h"\n', 1)
	elif re.search(r'mfcless::(Rect|Point|Size)\b', text) and '"mfcless/Geometry.h"' not in text and '"mfcless/Window.h"' not in text:
		text = text.replace('#include "openmpt/all/BuildSettings.hpp"\n', '#include "openmpt/all/BuildSettings.hpp"\n#include "mfcless/Geometry.h"\n', 1)
	if re.search(r'mfcless::(RedOf|GreenOf|BlueOf|Rgb)\b', text) and '"mfcless/System.h"' not in text:
		text = text.replace('#include "openmpt/all/BuildSettings.hpp"\n', '#include "openmpt/all/BuildSettings.hpp"\n#include "mfcless/System.h"\n', 1)
	if re.search(r'mfcless::(Menu|menu)\b', text) and '"mfcless/Menus.h"' not in text:
		text = text.replace('#include "openmpt/all/BuildSettings.hpp"\n', '#include "openmpt/all/BuildSettings.hpp"\n#include "mfcless/Menus.h"\n', 1)
	if re.search(r'mfcless::(UpDown|TabControl|GroupFrame)', text) and '"mfcless/Controls.h"' not in text:
		text = text.replace('#include "openmpt/all/BuildSettings.hpp"\n', '#include "openmpt/all/BuildSettings.hpp"\n#include "mfcless/Controls.h"\n', 1)
	if 'mfcless::StatusBar' in text and '"mfcless/StatusBar.h"' not in text:
		text = text.replace('#include "openmpt/all/BuildSettings.hpp"\n', '#include "openmpt/all/BuildSettings.hpp"\n#include "mfcless/StatusBar.h"\n', 1)
	if 'mfcless::Button' in text and '"mfcless/Button.h"' not in text:
		text = text.replace('#include "openmpt/all/BuildSettings.hpp"\n', '#include "openmpt/all/BuildSettings.hpp"\n#include "mfcless/Button.h"\n', 1)
	if re.search(r'mfcless::(DrawItemInfo|ods|odt)\b', text) and '"mfcless/Graphics.h"' not in text:
		text = text.replace('#include "openmpt/all/BuildSettings.hpp"\n', '#include "openmpt/all/BuildSettings.hpp"\n#include "mfcless/Graphics.h"\n', 1)
	if re.search(r'mfcless::(DropTarget|drop::)', text) and '"mfcless/DropTarget.h"' not in text:
		text = text.replace('#include "openmpt/all/BuildSettings.hpp"\n', '#include "openmpt/all/BuildSettings.hpp"\n#include "mfcless/DropTarget.h"\n', 1)
	if 'CImageListEx' in text and '"CImageListEx.h"' not in text and not re.search(r'class CImageListEx\b', text):
		text = text.replace('#include "openmpt/all/BuildSettings.hpp"\n', '#include "openmpt/all/BuildSettings.hpp"\n#include "CImageListEx.h"\n', 1)
	if 'mfcless::PrintF' in text and '"mfcless/QtString.h"' not in text:
		text = text.replace('#include "openmpt/all/BuildSettings.hpp"\n', '#include "openmpt/all/BuildSettings.hpp"\n#include "mfcless/QtString.h"\n', 1)
	if 'mfcless::compare::' in text and '"mfcless/Text.h"' not in text:
		text = text.replace('#include "openmpt/all/BuildSettings.hpp"\n', '#include "openmpt/all/BuildSettings.hpp"\n#include "mfcless/Text.h"\n', 1)
	if 'mfcless::clip::' in text and '"mfcless/Clipboard.h"' not in text:
		text = text.replace('#include "openmpt/all/BuildSettings.hpp"\n', '#include "openmpt/all/BuildSettings.hpp"\n#include "mfcless/Clipboard.h"\n', 1)
	if re.search(r'mfcless::(TickCount|CurrentThreadId|SleepMs)', text) and '"mfcless/System.h"' not in text:
		text = text.replace('#include "openmpt/all/BuildSettings.hpp"\n', '#include "openmpt/all/BuildSettings.hpp"\n#include "mfcless/System.h"\n', 1)
	if 'mfcless::ListCtrl' in text and '"mfcless/ListCtrl.h"' not in text:
		text = text.replace('#include "openmpt/all/BuildSettings.hpp"\n', '#include "openmpt/all/BuildSettings.hpp"\n#include "mfcless/ListCtrl.h"\n', 1)
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
	base = BASES.get(base, base).split(',')[0]
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

def dialog_helpers(text):
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
	text = re.sub(r'\bShowWindow\(SW_SHOW\)', 'ShowWindow(true)', text)
	text = re.sub(r'\bShowWindow\(SW_HIDE\)', 'ShowWindow(false)', text)
	text = re.sub(r'\bSW_(HIDE|SHOWNORMAL|NORMAL|SHOWMINIMIZED|SHOWMAXIMIZED|MAXIMIZE|SHOWNOACTIVATE|SHOW|MINIMIZE|SHOWMINNOACTIVE|SHOWNA|RESTORE)\b',
		lambda m: 'mfcless::show::' + {'HIDE': 'Hide', 'SHOWNORMAL': 'ShowNormal', 'NORMAL': 'Normal', 'SHOWMINIMIZED': 'ShowMinimized',
			'SHOWMAXIMIZED': 'ShowMaximized', 'MAXIMIZE': 'Maximize', 'SHOWNOACTIVATE': 'ShowNoActivate', 'SHOW': 'Show',
			'MINIMIZE': 'Minimize', 'SHOWMINNOACTIVE': 'ShowMinNoActive', 'SHOWNA': 'ShowNA', 'RESTORE': 'Restore'}[m.group(1)], text)
	return text

def member_types(header):
	types = {}
	for m in re.finditer(r'^\s*([\w:]+)\s+((?:\*\s*\w+(?:\[[^\]]*\])?\s*=\s*(?:nullptr|\{\})\s*,?\s*)+);', header, flags=re.M):
		for n in re.findall(r'\*\s*(\w+)', m.group(2)):
			types[n] = m.group(1)
	return types

def member_access(text, types):
	"""Members that became pointers: m_a.F() -> m_a->F(), and references to
	them (return m_a; from a function returning a reference, a reference
	bound to one of two) dereference them."""
	item_lists = ('QComboBox', 'QListWidget', 'mfcless::Window<QComboBox>', 'mfcless::Window<QListWidget>')
	def call(m):
		name, method = m.group(1), m.group(2)
		t = types.get(name)
		if t is None:
			return m.group(0)
		repl = CONTROL_METHODS.get(t, {}).get(method) or CONTROL_METHODS['*'].get(method)
		if t in item_lists:
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
	text = re.sub(r'\(&(m_\w+)\)', lambda m: '(' + m.group(1) + ')' if m.group(1) in types else m.group(0), text)
	def deref(name):
		return '*' + name if name in types else name
	text = re.sub(r'(&\s*\w+\([^)]*\)\s*(?:const\s*)?\{\s*return )(m_\w+)(;)', lambda m: m.group(1) + deref(m.group(2)) + m.group(3), text)
	text = re.sub(r'(\w+\s*&\s*\w+\s*=\s*[^;?]+\?\s*)(m_\w+)(\s*:\s*)(m_\w+)(;)', lambda m: m.group(1) + deref(m.group(2)) + m.group(3) + deref(m.group(4)) + m.group(5), text)
	return text

def twin_cpp(text, header):
	text = pointer_handles(text)
	text = re.sub(r'^#include "stdafx.h"\n', '', text, flags=re.M)
	text = re.sub(r'^BEGIN_MESSAGE_MAP\((\w+),\s*(\w+)\)\n(.*?)^END_MESSAGE_MAP\(\)', message_map, text, flags=re.M | re.S)
	text = re.sub(r'^IMPLEMENT_DYNCREATE\((\w+),\s*\w+\)\s*\n', r'const mfcless::ViewClass \1::class\1{"\1", [](QWidget *parent) -> mfcless::DocumentView * { auto *view = new \1(); if(parent) view->setParent(parent); return view; }};\n', text, flags=re.M)
	text = re.sub(r'^IMPLEMENT_(DYNAMIC|SERIAL)\(.*\)\s*\n', '', text, flags=re.M)
	# Data exchange
	text = re.sub(r'void (\w+)::DoDataExchange\(CDataExchange\s*\*\s*\w*\)', r'void \1::DoDataExchange(DataExchange &dx)', text)
	text = re.sub(r'(\w+)::DoDataExchange\(pDX\)', lambda m: BASES.get(m.group(1), m.group(1)).split(',')[0] + '::DoDataExchange(dx)', text)
	text = re.sub(r'DDX_Control\(pDX,\s*([^,]+),\s*([^)]+)\)', r'ExchangeControl(dx, \1, \2)', text)
	for k, v in DDX.items():
		text = re.sub(k + r'\(pDX,', v + '(dx,', text)
	text = re.sub(r'DDV_MinMax\w+\(pDX,', 'ValidateMinMax(dx,', text)
	text = re.sub(r'pDX->m_bSaveAndValidate', 'dx.save', text)
	text = dialog_helpers(text)
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
	text = member_access(text, types)
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
		twins = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'mptrack')
		hdr = os.path.join(twins, NAME[:-4] + '.h')
		if not os.path.exists(hdr):
			# Upstream's names differ in case now and then (MainFrm.cpp, Mainfrm.h).
			want = (NAME[:-4] + '.h').lower()
			for root, _, files in os.walk(twins):
				for f in files:
					if os.path.relpath(os.path.join(root, f), twins).lower() == want:
						hdr = os.path.join(root, f)
		header = open(hdr, encoding='utf-8').read() if os.path.exists(hdr) else ''
		out = twin_cpp(src, header)
		if PATCH_MODE:
			open(path, 'w', encoding='utf-8', errors='surrogateescape').write(out)
		else:
			sys.stdout.write(out)
	else:
		sys.stdout.write(twin(src))
