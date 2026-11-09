// The dialog manager and the standard controls (buttons, statics, edits,
// list boxes, combo boxes), run offscreen: hand-built templates for the
// behaviour, and every dialog template of upstream's mptrack.rc.
#include <afxwin.h>

#include "MptrackResources.h"
#include "QtWin.h"
#include "resource.h"

#include <QtCore/QTimer>
#include <QtGui/QKeyEvent>
#include <QtWidgets/QApplication>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPushButton>

#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <cstring>
#include <functional>
#include <set>
#include <string>
#include <vector>

using namespace linopenmpt::qtwin;

namespace
{
int g_failures = 0;

void Check(bool ok, const char *what)
{
	std::printf("%s %s\n", ok ? "ok  " : "FAIL", what);
	if(!ok)
		g_failures++;
}

void Pump()
{
	for(int i = 0; i < 5; i++)
		QApplication::processEvents();
}

// ---- building templates --------------------------------------------------------

class TemplateBuilder
{
public:
	TemplateBuilder(DWORD style, int cx, int cy, const wchar_t *title)
	{
		Dword(style | DS_SETFONT);
		Dword(0);
		countAt = data.size();
		Word(0);
		Word(0);
		Word(0);
		Word(static_cast<WORD>(cx));
		Word(static_cast<WORD>(cy));
		Word(0); // menu
		Word(0); // class
		String(title);
		Word(8);
		String(L"MS Shell Dlg");
	}

	// cls: an atom (0x80 Button ... 0x85 ComboBox).
	void Item(WORD cls, const wchar_t *text, WORD id, DWORD style, int x, int y, int cx, int cy, DWORD exStyle = 0)
	{
		while(data.size() % 4)
			data.push_back(0);
		Dword(style | WS_CHILD | WS_VISIBLE);
		Dword(exStyle);
		Word(static_cast<WORD>(x));
		Word(static_cast<WORD>(y));
		Word(static_cast<WORD>(cx));
		Word(static_cast<WORD>(cy));
		Word(id);
		Word(0xFFFF);
		Word(cls);
		String(text);
		Word(0);
		count++;
		data[countAt] = static_cast<BYTE>(count);
		data[countAt + 1] = static_cast<BYTE>(count >> 8);
	}

	LPCDLGTEMPLATE Get()
	{
		aligned.assign((data.size() + 3) / 4, 0);
		std::memcpy(aligned.data(), data.data(), data.size());
		return reinterpret_cast<LPCDLGTEMPLATE>(aligned.data());
	}

private:
	void Word(WORD w)
	{
		data.push_back(static_cast<BYTE>(w));
		data.push_back(static_cast<BYTE>(w >> 8));
	}
	void Dword(DWORD d)
	{
		Word(static_cast<WORD>(d));
		Word(static_cast<WORD>(d >> 16));
	}
	void String(const wchar_t *s)
	{
		for(; s != nullptr && *s; s++)
			Word(static_cast<WORD>(*s));
		Word(0);
	}

	std::vector<BYTE> data;
	std::vector<DWORD> aligned;
	std::size_t countAt = 0;
	int count = 0;
};

enum : WORD
{
	ClsButton = 0x80,
	ClsEdit = 0x81,
	ClsStatic = 0x82,
	ClsListBox = 0x83,
	ClsScrollBar = 0x84,
	ClsComboBox = 0x85,
};

enum : WORD
{
	IdName = 100,
	IdCheck = 101,
	IdRadio1 = 102,
	IdRadio2 = 103,
	IdList = 104,
	IdDropList = 105,
	IdDropDown = 106,
	IdLabel = 107,
	IdGroup = 108,
	IdNumber = 109,
};

struct Notification
{
	WORD id;
	WORD code;
};
std::vector<Notification> g_notes;
int g_initDialogs = 0;
HWND g_initFocus = nullptr;
std::vector<UINT> g_commands;

bool Got(WORD id, WORD code)
{
	for(const auto &n : g_notes)
	{
		if(n.id == id && n.code == code)
			return true;
	}
	return false;
}

int IndexOf(WORD id, WORD code)
{
	for(std::size_t i = 0; i < g_notes.size(); i++)
	{
		if(g_notes[i].id == id && g_notes[i].code == code)
			return static_cast<int>(i);
	}
	return -1;
}

INT_PTR CALLBACK TestDlgProc(HWND dlg, UINT msg, WPARAM wp, LPARAM)
{
	switch(msg)
	{
	case WM_INITDIALOG:
		g_initDialogs++;
		return TRUE;
	case WM_COMMAND:
		g_notes.push_back({LOWORD(wp), HIWORD(wp)});
		if(HIWORD(wp) == BN_CLICKED && (LOWORD(wp) == IDOK || LOWORD(wp) == IDCANCEL))
		{
			g_commands.push_back(LOWORD(wp));
			::EndDialog(dlg, LOWORD(wp));
		}
		return TRUE;
	default:
		return FALSE;
	}
}

TemplateBuilder BuildTestDialog()
{
	TemplateBuilder t(DS_MODALFRAME | WS_POPUP | WS_CAPTION | WS_SYSMENU, 200, 180, L"Test");
	t.Item(ClsStatic, L"&Name:", IdLabel, SS_LEFT, 4, 4, 40, 8);
	t.Item(ClsEdit, L"", IdName, ES_AUTOHSCROLL | WS_TABSTOP | WS_BORDER, 50, 4, 100, 12);
	t.Item(ClsButton, L"&Loop", IdCheck, BS_AUTOCHECKBOX | WS_TABSTOP, 4, 20, 60, 10);
	t.Item(ClsButton, L"Options", IdGroup, BS_GROUPBOX, 4, 32, 120, 30);
	t.Item(ClsButton, L"&First", IdRadio1, BS_AUTORADIOBUTTON | WS_GROUP | WS_TABSTOP, 8, 42, 50, 10);
	t.Item(ClsButton, L"&Second", IdRadio2, BS_AUTORADIOBUTTON, 60, 42, 50, 10);
	t.Item(ClsListBox, L"", IdList, LBS_NOTIFY | LBS_SORT | WS_VSCROLL | WS_BORDER | WS_TABSTOP | WS_GROUP, 4, 66, 80, 50);
	t.Item(ClsComboBox, L"", IdDropList, CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, 90, 66, 80, 80);
	t.Item(ClsComboBox, L"", IdDropDown, CBS_DROPDOWN | CBS_AUTOHSCROLL | WS_VSCROLL | WS_TABSTOP, 90, 84, 80, 80);
	t.Item(ClsEdit, L"42", IdNumber, ES_NUMBER | WS_TABSTOP | WS_BORDER, 90, 102, 40, 12);
	t.Item(ClsButton, L"OK", IDOK, BS_DEFPUSHBUTTON | WS_TABSTOP | WS_GROUP, 90, 160, 50, 14);
	t.Item(ClsButton, L"Cancel", IDCANCEL, BS_PUSHBUTTON | WS_TABSTOP, 144, 160, 50, 14);
	return t;
}

std::wstring Text(HWND hwnd)
{
	wchar_t buf[256] = {};
	::GetWindowText(hwnd, buf, 256);
	return buf;
}

// A key press (and release) to the focused widget, as the user types it.
void Key(int key, Qt::KeyboardModifiers mods = Qt::NoModifier, const QString &text = QString())
{
	QWidget *target = QApplication::activePopupWidget();
	if(target == nullptr)
		target = QApplication::focusWidget();
	if(target == nullptr)
		return;
	QKeyEvent press(QEvent::KeyPress, key, mods, text);
	QApplication::sendEvent(target, &press);
	QKeyEvent release(QEvent::KeyRelease, key, mods, text);
	if(QWidget *again = QApplication::activePopupWidget() ? QApplication::activePopupWidget() : QApplication::focusWidget())
		QApplication::sendEvent(again, &release);
	Pump();
}

// ---- upstream's templates --------------------------------------------------------

struct TemplateItem
{
	std::wstring cls;
	int id;
	DWORD style;
};

const WORD *SkipSz(const WORD *p)
{
	if(*p == 0xFFFF)
		return p + 2;
	while(*p)
		p++;
	return p + 1;
}

std::wstring ReadClass(const WORD *&p)
{
	std::wstring s;
	if(*p == 0xFFFF)
	{
		switch(p[1])
		{
		case 0x80: s = L"Button"; break;
		case 0x81: s = L"Edit"; break;
		case 0x82: s = L"Static"; break;
		case 0x83: s = L"ListBox"; break;
		case 0x84: s = L"ScrollBar"; break;
		case 0x85: s = L"ComboBox"; break;
		default: s = L"#atom"; break;
		}
		p += 2;
		return s;
	}
	while(*p)
		s += static_cast<wchar_t>(*p++);
	p++;
	return s;
}

std::vector<TemplateItem> TemplateItems(const BYTE *data, bool &ex)
{
	std::vector<TemplateItem> items;
	const WORD *p = reinterpret_cast<const WORD *>(data);
	ex = p[0] == 1 && p[1] == 0xFFFF;
	DWORD style;
	int count;
	if(ex)
	{
		style = *reinterpret_cast<const DWORD *>(data + 12);
		count = p[8];
		p += 13;
	} else
	{
		style = *reinterpret_cast<const DWORD *>(data);
		count = p[4];
		p += 9;
	}
	p = SkipSz(p); // menu
	p = SkipSz(p); // class
	p = SkipSz(p); // title
	if(style & (DS_SETFONT | DS_SHELLFONT))
	{
		p += ex ? 3 : 1;
		p = SkipSz(p);
	}
	for(int i = 0; i < count; i++)
	{
		p = reinterpret_cast<const WORD *>((reinterpret_cast<uintptr_t>(p) + 3) & ~uintptr_t(3));
		TemplateItem item;
		if(ex)
		{
			item.style = *reinterpret_cast<const DWORD *>(p + 4);
			item.id = static_cast<int>(*reinterpret_cast<const DWORD *>(p + 10));
			p += 12;
		} else
		{
			item.style = *reinterpret_cast<const DWORD *>(p);
			item.id = static_cast<short>(p[8]);
			p += 9;
		}
		item.cls = ReadClass(p);
		p = SkipSz(p);
		const WORD extra = *p++;
		p = reinterpret_cast<const WORD *>(reinterpret_cast<const BYTE *>(p) + extra);
		items.push_back(item);
	}
	return items;
}

bool IsStandard(const std::wstring &cls)
{
	static const std::set<std::wstring> standard{L"Button", L"Edit", L"Static", L"ListBox", L"ScrollBar", L"ComboBox", L"msctls_trackbar32", L"msctls_updown32", L"msctls_progress32", L"SysTabControl32", L"SysListView32", L"SysHeader32", L"SysTreeView32", L"ToolbarWindow32", L"ComboBoxEx32", L"SysLink"};
	for(const auto &s : standard)
	{
		if(QString::fromStdWString(cls).compare(QString::fromStdWString(s), Qt::CaseInsensitive) == 0)
			return true;
	}
	return false;
}

// With LINOPENMPT_SHOTS=<dir>, pictures of the dialogs, to look at.
void SaveShot(HWND dlg, int id)
{
	const QByteArray dir = qgetenv("LINOPENMPT_SHOTS");
	if(dir.isEmpty())
		return;
	Win32Widget *w = FromHandle(dlg);
	if(w == nullptr)
		return;
	::ShowWindow(dlg, SW_SHOWNA);
	Pump();
	w->grab().save(QString::fromLocal8Bit(dir) + QStringLiteral("/dialog_%1.png").arg(id));
}

INT_PTR CALLBACK PlainDlgProc(HWND, UINT msg, WPARAM, LPARAM)
{
	return msg == WM_INITDIALOG ? TRUE : FALSE;
}

void UpstreamTemplates()
{
	const HINSTANCE instance = AfxGetResourceHandle();
	int dialogs = 0, created = 0, standardOnly = 0, standardOnlyOk = 0, controls = 0, controlsOk = 0;
	std::set<std::wstring> missingClasses;
	std::vector<int> failedIds;
	// Child templates (form views, control bars) need a parent.
	HWND host = ::CreateDialogIndirectParam(instance, TemplateBuilder(WS_POPUP, 400, 300, L"Host").Get(), nullptr, PlainDlgProc, 0);
	for(int id = 1; id < 0x8000; id++)
	{
		HRSRC res = ::FindResource(instance, MAKEINTRESOURCE(id), RT_DIALOG);
		if(res == nullptr)
			continue;
		dialogs++;
		const auto *bytes = static_cast<const BYTE *>(::LockResource(::LoadResource(instance, res)));
		const DWORD size = ::SizeofResource(instance, res);
		bool ex = false;
		const std::vector<TemplateItem> items = TemplateItems(bytes, ex);
		bool onlyStandard = true;
		for(const auto &item : items)
		{
			if(!IsStandard(item.cls))
			{
				onlyStandard = false;
				missingClasses.insert(item.cls);
			}
		}
		// With DS_NOFAILCREATE, controls of classes that are not there yet are
		// left out and the rest is checked.
		std::vector<DWORD> copy((size + 3) / 4);
		std::memcpy(copy.data(), bytes, size);
		auto *styleField = reinterpret_cast<DWORD *>(reinterpret_cast<BYTE *>(copy.data()) + (ex ? 12 : 0));
		*styleField = (*styleField | DS_NOFAILCREATE) & ~WS_VISIBLE;
		if(onlyStandard)
			standardOnly++;
		HWND dlg = ::CreateDialogIndirectParam(instance, reinterpret_cast<LPCDLGTEMPLATE>(copy.data()), (*styleField & WS_CHILD) ? host : nullptr, PlainDlgProc, 0);
		if(dlg == nullptr)
		{
			std::printf("     template %d (%s, style %08X, %zu items) failed\n", id, ex ? "ex" : "classic", static_cast<unsigned>(*styleField), items.size());
			failedIds.push_back(id);
			continue;
		}
		created++;
		bool allThere = true;
		for(const auto &item : items)
		{
			if(!IsStandard(item.cls) || item.id == -1 || item.id == 0xFFFF || item.id == 0)
				continue;
			controls++;
			HWND ctl = ::GetDlgItem(dlg, item.id);
			if(ctl != nullptr)
				controlsOk++;
			else
				allThere = false;
		}
		if(onlyStandard && allThere)
			standardOnlyOk++;
		SaveShot(dlg, id);
		::DestroyWindow(dlg);
	}
	::DestroyWindow(host);
	std::printf("     upstream: %d dialog templates, %d created; %d use only standard controls; %d of %d standard controls found\n", dialogs, created, standardOnly, controlsOk, controls);
	std::printf("     classes still to come:");
	for(const auto &c : missingClasses)
		std::printf(" %ls", c.c_str());
	std::printf("\n");
	Check(dialogs > 70, "upstream's dialog templates are there");
	Check(created == dialogs, "every upstream dialog template creates (DS_NOFAILCREATE)");
	Check(standardOnlyOk == standardOnly, "dialogs with only standard controls have all of them");
	Check(controlsOk == controls, "every standard control of every template is created");
}

HWND g_modeless = nullptr;

// What an application's message loop does for its modeless dialogs.
BOOL ModelessHook(MSG *msg)
{
	return g_modeless != nullptr && ::IsDialogMessage(g_modeless, msg);
}


// ---- MFC dialogs -------------------------------------------------------------------------

enum : WORD
{
	IdChecks = 120,
};

LPCDLGTEMPLATE MfcTemplate()
{
	static TemplateBuilder t = []()
	{
		TemplateBuilder b(DS_MODALFRAME | WS_POPUP | WS_CAPTION | WS_SYSMENU, 200, 170, L"MFC");
		b.Item(ClsEdit, L"", IdName, ES_AUTOHSCROLL | WS_TABSTOP | WS_BORDER, 4, 4, 100, 12);
		b.Item(ClsButton, L"&Loop", IdCheck, BS_AUTOCHECKBOX | WS_TABSTOP, 4, 20, 60, 10);
		b.Item(ClsButton, L"&First", IdRadio1, BS_AUTORADIOBUTTON | WS_GROUP | WS_TABSTOP, 8, 34, 50, 10);
		b.Item(ClsButton, L"&Second", IdRadio2, BS_AUTORADIOBUTTON, 60, 34, 50, 10);
		b.Item(ClsComboBox, L"", IdDropList, CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP | WS_GROUP, 4, 48, 80, 80);
		b.Item(ClsListBox, L"", IdChecks, LBS_OWNERDRAWFIXED | LBS_HASSTRINGS | LBS_NOINTEGRALHEIGHT | LBS_NOTIFY | WS_VSCROLL | WS_BORDER | WS_TABSTOP, 4, 66, 120, 60);
		b.Item(ClsEdit, L"", IdNumber, WS_TABSTOP | WS_BORDER, 130, 4, 40, 12);
		b.Item(ClsButton, L"OK", IDOK, BS_DEFPUSHBUTTON | WS_TABSTOP | WS_GROUP, 90, 150, 50, 14);
		b.Item(ClsButton, L"Cancel", IDCANCEL, BS_PUSHBUTTON | WS_TABSTOP, 144, 150, 50, 14);
		return b;
	}();
	return t.Get();
}

class TestDialog : public CDialog
{
public:
	explicit TestDialog(CWnd *parent = nullptr)
	{
		InitModalIndirect(MfcTemplate(), parent);
	}

	CEdit m_name;
	CComboBox m_combo;
	CCheckListBox m_checks;
	int m_number = 7;
	int m_check = 1;
	int m_radio = 1;
	CString m_text = L"Hello";
	bool initCalled = false;
	bool okCalled = false;
	bool endInInit = false;
	int selChanges = 0;
	int checkChanges = 0;
	std::function<void(TestDialog &)> during;

	void DoDataExchange(CDataExchange *dx) override
	{
		CDialog::DoDataExchange(dx);
		DDX_Control(dx, IdName, m_name);
		DDX_Control(dx, IdDropList, m_combo);
		DDX_Control(dx, IdChecks, m_checks);
		DDX_Text(dx, IdName, m_text);
		DDX_Text(dx, IdNumber, m_number);
		DDV_MinMaxInt(dx, m_number, 0, 100);
		DDX_Check(dx, IdCheck, m_check);
		DDX_Radio(dx, IdRadio1, m_radio);
	}

	BOOL OnInitDialog() override
	{
		CDialog::OnInitDialog();
		initCalled = true;
		for(const wchar_t *s : {L"Sine", L"Square", L"Saw"})
			m_combo.AddString(s);
		m_combo.SetCurSel(0);
		for(const wchar_t *s : {L"Loop", L"Sustain", L"Ping-pong"})
			m_checks.SetItemData(m_checks.AddString(s), 100);
		m_checks.SetCheck(1, BST_CHECKED);
		m_checks.SetItemHeight(0, 0);
		if(endInInit)
			EndDialog(42);
		if(during)
			QTimer::singleShot(0, [this]() { Pump(); during(*this); });
		return TRUE;
	}

	void OnOK() override
	{
		okCalled = true;
		CDialog::OnOK();
	}

	afx_msg void OnComboChange() { selChanges++; }
	afx_msg void OnCheckChange() { checkChanges++; }

	DECLARE_MESSAGE_MAP()
};

BEGIN_MESSAGE_MAP(TestDialog, CDialog)
	ON_CBN_SELCHANGE(IdDropList, &TestDialog::OnComboChange)
	ON_CLBN_CHKCHANGE(IdChecks, &TestDialog::OnCheckChange)
END_MESSAGE_MAP()


// ---- property sheets --------------------------------------------------------------

std::vector<std::pair<int, UINT>> g_pageEvents; // page number, PSN_* code (and lParam for PSN_APPLY in the top bit)
bool g_refuseKill = false;

INT_PTR CALLBACK PageProc(HWND hwnd, UINT msg, WPARAM, LPARAM lp)
{
	if(msg == WM_INITDIALOG)
	{
		const auto *psp = reinterpret_cast<const PROPSHEETPAGE *>(lp);
		::SetWindowLongPtr(hwnd, DWLP_USER, psp->lParam);
		return TRUE;
	}
	if(msg == WM_NOTIFY)
	{
		const auto *nm = reinterpret_cast<const PSHNOTIFY *>(lp);
		const int page = static_cast<int>(::GetWindowLongPtr(hwnd, DWLP_USER));
		g_pageEvents.emplace_back(page, nm->hdr.code == PSN_APPLY && nm->lParam ? (nm->hdr.code | 0x80000000u) : nm->hdr.code);
		if(nm->hdr.code == PSN_KILLACTIVE)
			::SetWindowLongPtr(hwnd, DWLP_MSGRESULT, g_refuseKill ? TRUE : FALSE);
		else
			::SetWindowLongPtr(hwnd, DWLP_MSGRESULT, 0);
		return TRUE;
	}
	return FALSE;
}

bool Saw(int page, UINT code)
{
	return std::find(g_pageEvents.begin(), g_pageEvents.end(), std::make_pair(page, code)) != g_pageEvents.end();
}

HWND ActiveSheet()
{
	QWidget *w = QApplication::activeWindow();
	auto *win = dynamic_cast<Win32Widget *>(w);
	return win != nullptr ? win->hwnd() : nullptr;
}

void Win32PropertySheets(HWND owner)
{
	TemplateBuilder first(WS_CHILD | WS_CAPTION | DS_SETFONT, 160, 80, L"First");
	first.Item(ClsEdit, L"", IdName, WS_TABSTOP | WS_BORDER, 8, 8, 100, 12);
	first.Item(ClsButton, L"Check", IdCheck, BS_AUTOCHECKBOX | WS_TABSTOP, 8, 26, 100, 10);
	TemplateBuilder second(WS_CHILD | WS_CAPTION | DS_SETFONT, 200, 120, L"Second page");
	second.Item(ClsStatic, L"Bigger", IdLabel, SS_LEFT, 8, 8, 100, 10);
	PROPSHEETPAGE pages[2]{};
	for(int i = 0; i < 2; i++)
	{
		pages[i].dwSize = sizeof(PROPSHEETPAGE);
		pages[i].dwFlags = PSP_DLGINDIRECT;
		pages[i].pResource = i == 0 ? first.Get() : second.Get();
		pages[i].pfnDlgProc = PageProc;
		pages[i].lParam = i + 1;
	}
	PROPSHEETHEADER psh{};
	psh.dwSize = sizeof(psh);
	psh.dwFlags = PSH_PROPSHEETPAGE | PSH_NOCONTEXTHELP;
	psh.hwndParent = owner;
	psh.pszCaption = L"Sheet";
	psh.nPages = 2;
	psh.ppsp = pages;

	// OK after Apply: both pages apply, the sheet returns 1.
	g_pageEvents.clear();
	QTimer::singleShot(0, []()
	{
		Pump();
		const HWND sheet = ActiveSheet();
		const HWND tab = PropSheet_GetTabControl(sheet);
		Check(tab != nullptr && TabCtrl_GetItemCount(tab) == 2, "PropertySheet: a tab per page");
		wchar_t text[32] = {};
		TCITEM item{};
		item.mask = TCIF_TEXT;
		item.pszText = text;
		item.cchTextMax = 32;
		TabCtrl_GetItem(tab, 1, &item);
		Check(std::wstring(text) == L"Second page", "PropertySheet: tabs are the pages' captions");
		const HWND page1 = PropSheet_GetCurrentPageHwnd(sheet);
		Check(page1 != nullptr && ::IsWindowVisible(page1) && ::GetParent(page1) == sheet && Saw(1, PSN_SETACTIVE), "PropertySheet: the first page is made and active");
		Check(PropSheet_IndexToHwnd(sheet, 1) == nullptr, "PropertySheet: other pages are made when shown");
		Check((::GetWindowLongPtr(page1, GWL_STYLE) & (WS_CHILD | WS_CAPTION | DS_CONTROL)) == (WS_CHILD | DS_CONTROL), "PropertySheet: pages are child dialogs");
		Check(Text(sheet) == L"Sheet", "PropertySheet: the caption");
		Check(::GetFocus() == ::GetDlgItem(page1, IdName), "PropertySheet: the focus starts on the page");
		// The tab's display area holds the largest page; buttons below it.
		RECT tabRc{}, display{}, ok{}, page2Rc{};
		::GetWindowRect(tab, &tabRc);
		display = tabRc;
		TabCtrl_AdjustRect(tab, FALSE, &display);
		::GetWindowRect(::GetDlgItem(sheet, IDOK), &ok);
		RECT big{0, 0, 200, 120};
		::MapDialogRect(sheet, &big);
		Check(display.right - display.left >= big.right - 1 && display.bottom - display.top >= big.bottom - 1, "PropertySheet: room for the largest page");
		Check(ok.top >= tabRc.bottom && ok.right <= tabRc.right + 1, "PropertySheet: buttons under the tabs, at the right");
		Check(!::IsWindowEnabled(::GetDlgItem(sheet, IDC_APPLY_BUTTON)) && !::IsWindowVisible(::GetDlgItem(sheet, IDHELP)), "PropertySheet: Apply disabled, no Help");

		// Changing pages: the old one is asked, the new one made.
		g_refuseKill = true;
		Check(!PropSheet_SetCurSel(sheet, nullptr, 1) && PropSheet_GetCurrentPageHwnd(sheet) == page1, "PropertySheet: PSN_KILLACTIVE can refuse");
		g_refuseKill = false;
		g_pageEvents.clear();
		Check(PropSheet_SetCurSel(sheet, nullptr, 1) != 0, "PropertySheet: PSM_SETCURSEL");
		const HWND page2 = PropSheet_GetCurrentPageHwnd(sheet);
		Check(page2 != nullptr && page2 != page1 && !::IsWindowVisible(page1) && ::IsWindowVisible(page2), "PropertySheet: the new page shows, the old one hides");
		Check(g_pageEvents.size() == 2 && g_pageEvents[0] == std::make_pair(1, static_cast<UINT>(PSN_KILLACTIVE)) && g_pageEvents[1] == std::make_pair(2, static_cast<UINT>(PSN_SETACTIVE)), "PropertySheet: PSN_KILLACTIVE, then PSN_SETACTIVE");
		::GetWindowRect(page2, &page2Rc);
		Check(page2Rc.left == display.left && page2Rc.top == display.top, "PropertySheet: pages sit in the display area");
		Check(TabCtrl_GetCurSel(tab) == 1, "PropertySheet: the tab follows");
		SaveShot(sheet, 9001);

		// Ctrl+Tab goes round.
		::SetFocus(::GetDlgItem(sheet, IDOK));
		Pump();
		Key(Qt::Key_Tab, Qt::ControlModifier);
		Check(PropSheet_GetCurrentPageHwnd(sheet) == page1, "PropertySheet: Ctrl+Tab, the next page");

		// Apply: enabled by PSM_CHANGED, applies every page made so far.
		PropSheet_Changed(sheet, page1);
		Check(::IsWindowEnabled(::GetDlgItem(sheet, IDC_APPLY_BUTTON)) != FALSE, "PropertySheet: PSM_CHANGED enables Apply");
		g_pageEvents.clear();
		::SendMessage(sheet, WM_COMMAND, MAKEWPARAM(IDC_APPLY_BUTTON, BN_CLICKED), reinterpret_cast<LPARAM>(::GetDlgItem(sheet, IDC_APPLY_BUTTON)));
		Check(Saw(1, PSN_APPLY) && Saw(2, PSN_APPLY) && !::IsWindowEnabled(::GetDlgItem(sheet, IDC_APPLY_BUTTON)), "PropertySheet: Apply applies all pages and disables itself");
		Check(PropSheet_GetCurrentPageHwnd(sheet) == page1, "PropertySheet: Apply keeps the sheet open");
		g_pageEvents.clear();
		::SetFocus(::GetDlgItem(page1, IdName));
		Pump();
		Key(Qt::Key_Return);
	});
	const INT_PTR result = ::PropertySheet(&psh);
	Check(result == 1, "PropertySheet: OK returns 1");
	Check(Saw(1, PSN_KILLACTIVE) && Saw(1, PSN_APPLY | 0x80000000u) && Saw(2, PSN_APPLY | 0x80000000u), "PropertySheet: Enter is OK, which applies with lParam TRUE");

	// Escape: PSN_QUERYCANCEL, PSN_RESET, nothing saved.
	g_pageEvents.clear();
	QTimer::singleShot(0, []()
	{
		Pump();
		Key(Qt::Key_Escape);
	});
	Check(::PropertySheet(&psh) == 0 && Saw(1, PSN_QUERYCANCEL) && Saw(1, PSN_RESET) && !Saw(1, PSN_APPLY), "PropertySheet: Escape cancels and returns 0");

	// Modeless: the sheet stays until the application destroys it.
	psh.dwFlags |= PSH_MODELESS;
	const auto modeless = reinterpret_cast<HWND>(::PropertySheet(&psh));
	Check(modeless != nullptr && ::IsWindowVisible(modeless), "PropertySheet: PSH_MODELESS returns the visible sheet");
	PropSheet_PressButton(modeless, PSBTN_CANCEL);
	Pump();
	Check(::IsWindow(modeless) && PropSheet_GetCurrentPageHwnd(modeless) == nullptr, "PropertySheet: modeless sheets end with no current page");
	::DestroyWindow(modeless);
}

class TestPage : public CPropertyPage
{
public:
	explicit TestPage(UINT id) : CPropertyPage(id) { }
	int inits = 0, setActives = 0, killActives = 0, oks = 0, cancels = 0;
	BOOL OnInitDialog() override
	{
		CPropertyPage::OnInitDialog();
		inits++;
		return TRUE;
	}
	BOOL OnSetActive() override
	{
		setActives++;
		return CPropertyPage::OnSetActive();
	}
	BOOL OnKillActive() override
	{
		killActives++;
		return CPropertyPage::OnKillActive();
	}
	void OnOK() override
	{
		oks++;
		CPropertyPage::OnOK();
	}
	void OnCancel() override
	{
		cancels++;
		CPropertyPage::OnCancel();
	}
};

class TestSheet : public CPropertySheet
{
public:
	using CPropertySheet::CPropertySheet;
	bool initCalled = false;
	std::function<void(TestSheet &)> during;
	BOOL OnInitDialog() override
	{
		initCalled = true;
		ModifyStyleEx(0, WS_EX_CONTEXTHELP);
		const BOOL result = CPropertySheet::OnInitDialog();
		if(during)
			QTimer::singleShot(0, [this]() { Pump(); during(*this); });
		return result;
	}
};

void MfcPropertySheets(HWND owner)
{
	TestPage general(IDD_OPTIONS_GENERAL), mixer(IDD_OPTIONS_MIXER);
	{
		TestSheet sheet(_T("OpenMPT Setup"), CWnd::FromHandle(owner), 0);
		sheet.AddPage(&general);
		sheet.AddPage(&mixer);
		Check(sheet.GetPageCount() == 2 && sheet.GetActiveIndex() == 0, "CPropertySheet: pages before DoModal");
		sheet.during = [&](TestSheet &s)
		{
			Check(s.GetActivePage() == &general && general.inits == 1 && general.setActives == 1, "CPropertySheet: the first page is a CPropertyPage, initialised and active");
			Check(dynamic_cast<CPropertySheet *>(general.GetParent()) == &s && s.GetDlgItem(IDOK) != nullptr, "CPropertyPage: the parent is the sheet (as Mpdlgs.cpp uses it)");
			Check(mixer.m_hWnd == nullptr, "CPropertySheet: other pages wait");
			Check(::IsWindowEnabled(owner) == FALSE, "CPropertySheet: the owner is disabled");
			general.SetModified(TRUE);
			Check(s.GetDlgItem(ID_APPLY_NOW) != nullptr && s.GetDlgItem(ID_APPLY_NOW)->IsWindowEnabled(), "CPropertyPage::SetModified enables Apply");
			s.SetActivePage(1);
			Check(s.GetActivePage() == &mixer && mixer.inits == 1 && general.killActives == 1 && s.GetActiveIndex() == 1, "CPropertySheet::SetActivePage");
			SaveShot(s.m_hWnd, 9002);
			s.SetActivePage(&general);
			Check(general.setActives == 2, "CPropertySheet: back to the first page");
			s.PressButton(PSBTN_OK);
		};
		const INT_PTR result = sheet.DoModal();
		Check(sheet.initCalled, "CPropertySheet::OnInitDialog override");
		Check(result == IDOK, "CPropertySheet::DoModal returns IDOK");
		Check(general.oks == 1 && mixer.oks == 1 && general.cancels == 0, "CPropertyPage::OnOK on every page");
		Check(sheet.m_hWnd == nullptr && general.m_hWnd == nullptr && ::IsWindowEnabled(owner), "CPropertySheet: gone, the owner enabled again");
	}
	{
		TestPage find(IDD_EDIT_FIND), replace(IDD_EDIT_REPLACE);
		TestSheet sheet(_T("Find/Replace"));
		sheet.AddPage(&find);
		sheet.AddPage(&replace);
		sheet.during = [](TestSheet &)
		{
			Key(Qt::Key_Escape);
		};
		Check(sheet.DoModal() == IDCANCEL && find.cancels == 1 && find.oks == 0, "CPropertySheet: Escape is IDCANCEL, pages get OnCancel");
	}
}

// Resizable dialogs: an upstream template with its AFX_DIALOG_LAYOUT, and a
// minimum size from WM_GETMINMAXINFO, as upstream's ResizableDialog sets one.
class LayoutDialog : public CDialog
{
public:
	LayoutDialog() : CDialog(IDD_EDITHISTORY) {}
	SIZE minSize{};
	int minMaxQueries = 0;
	afx_msg void OnGetMinMaxInfo(MINMAXINFO *mmi)
	{
		minMaxQueries++;
		if(minSize.cx)
			mmi->ptMinTrackSize = POINT{minSize.cx, minSize.cy};
		CDialog::OnGetMinMaxInfo(mmi);
	}
	DECLARE_MESSAGE_MAP()
};
BEGIN_MESSAGE_MAP(LayoutDialog, CDialog)
	ON_WM_GETMINMAXINFO()
END_MESSAGE_MAP()

RECT ItemRect(HWND dlg, int id)
{
	RECT rc{};
	::GetWindowRect(::GetDlgItem(dlg, id), &rc);
	::MapWindowPoints(nullptr, dlg, reinterpret_cast<POINT *>(&rc), 2);
	return rc;
}

void MfcDynamicLayouts(HWND owner)
{
	LayoutDialog dlg;
	Check(dlg.Create(IDD_EDITHISTORY, CWnd::FromHandle(owner)) != FALSE, "CDialog::Create with an AFX_DIALOG_LAYOUT template");
	Check(dlg.IsDynamicLayoutEnabled() && dlg.GetDynamicLayout() != nullptr, "CDialog: OnInitDialog loads the template's dynamic layout");
	const RECT ok = ItemRect(dlg.m_hWnd, IDOK), clear = ItemRect(dlg.m_hWnd, IDC_BTN_CLEAR), edit = ItemRect(dlg.m_hWnd, IDC_EDIT_HISTORY), total = ItemRect(dlg.m_hWnd, IDC_TOTAL_EDIT_TIME);
	CRect window;
	dlg.GetWindowRect(&window);
	dlg.SetWindowPos(nullptr, 0, 0, window.Width() + 100, window.Height() + 60, SWP_NOMOVE | SWP_NOZORDER);
	Pump();
	const RECT ok2 = ItemRect(dlg.m_hWnd, IDOK), clear2 = ItemRect(dlg.m_hWnd, IDC_BTN_CLEAR), edit2 = ItemRect(dlg.m_hWnd, IDC_EDIT_HISTORY), total2 = ItemRect(dlg.m_hWnd, IDC_TOTAL_EDIT_TIME);
	Check(ok2.left == ok.left + 100 && ok2.top == ok.top + 60 && ok2.right - ok2.left == ok.right - ok.left, "dynamic layout: OK moves with the corner (100, 100 / 0, 0)");
	Check(clear2.left == clear.left && clear2.top == clear.top + 60, "dynamic layout: Clear moves down only (0, 100 / 0, 0)");
	Check(edit2.left == edit.left && edit2.top == edit.top && edit2.right == edit.right + 100 && edit2.bottom == edit.bottom + 60, "dynamic layout: the history grows both ways (0, 0 / 100, 100)");
	Check(total2.left == total.left && total2.top == total.top + 60 && total2.right == total.right + 100 && total2.bottom - total2.top == total.bottom - total.top, "dynamic layout: the total moves down and grows wider (0, 100 / 100, 0)");
	SaveShot(dlg.m_hWnd, 9003);
	dlg.SetWindowPos(nullptr, 0, 0, window.Width(), window.Height(), SWP_NOMOVE | SWP_NOZORDER);
	Pump();
	const RECT ok3 = ItemRect(dlg.m_hWnd, IDOK), edit3 = ItemRect(dlg.m_hWnd, IDC_EDIT_HISTORY);
	Check(::EqualRect(&ok, &ok3) && ::EqualRect(&edit, &edit3), "dynamic layout: back to the template's size, back to the template's places");
	dlg.DestroyWindow();
	Check(!dlg.IsDynamicLayoutEnabled(), "dynamic layout: gone with the window");

	// The minimum track size, applied when the window shows.
	LayoutDialog sized;
	sized.Create(IDD_EDITHISTORY, CWnd::FromHandle(owner));
	sized.ShowWindow(SW_HIDE);
	sized.GetWindowRect(&window);
	sized.minSize = SIZE{window.Width(), window.Height()};
	sized.ShowWindow(SW_SHOW);
	Pump();
	Check(sized.minMaxQueries > 0, "WM_GETMINMAXINFO reaches the window when it shows");
	sized.SetWindowPos(nullptr, 0, 0, 50, 40, SWP_NOMOVE | SWP_NOZORDER);
	Pump();
	CRect small;
	sized.GetWindowRect(&small);
	Check(small.Width() == window.Width() && small.Height() == window.Height(), "the window keeps its minimum track size");
	sized.DestroyWindow();
}

// Accelerator tables: virtual keys with modifiers, and characters.
std::vector<WPARAM> g_accelCommands;
LRESULT CALLBACK AccelProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
	if(msg == WM_COMMAND)
		g_accelCommands.push_back(wp);
	return ::DefWindowProc(hwnd, msg, wp, lp);
}

void Accelerators()
{
	WNDCLASS wc{};
	wc.lpfnWndProc = AccelProc;
	wc.lpszClassName = L"AccelTest";
	::RegisterClass(&wc);
	HWND hwnd = ::CreateWindowEx(0, L"AccelTest", L"Accelerators", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, 200, 100, nullptr, nullptr, nullptr, nullptr);
	::SetFocus(hwnd);
	Pump();
	ACCEL entries[] = {
		{FVIRTKEY, VK_F5, 500},
		{FVIRTKEY | FCONTROL, 'S', 501},
		{0, 'x', 502},
	};
	HACCEL table = ::CreateAcceleratorTable(entries, 3);
	Check(table != nullptr && ::CopyAcceleratorTable(table, nullptr, 0) == 3, "CreateAcceleratorTable / CopyAcceleratorTable");
	MSG f5{hwnd, WM_KEYDOWN, VK_F5, 0};
	MSG s{hwnd, WM_KEYDOWN, 'S', 0};
	MSG x{hwnd, WM_CHAR, 'x', 0};
	Check(::TranslateAccelerator(hwnd, table, &f5) == 1 && g_accelCommands == std::vector<WPARAM>{MAKEWPARAM(500, 1)}, "TranslateAccelerator: F5 is WM_COMMAND 500 from an accelerator");
	Check(::TranslateAccelerator(hwnd, table, &s) == 0, "TranslateAccelerator: S without Ctrl is not Ctrl+S");
	Check(::TranslateAccelerator(hwnd, table, &x) == 1 && g_accelCommands.back() == MAKEWPARAM(502, 1), "TranslateAccelerator: a character accelerator");
	QWidget *focus = QApplication::focusWidget();
	if(focus != nullptr)
	{
		QKeyEvent press(QEvent::KeyPress, Qt::Key_Control, Qt::ControlModifier);
		QApplication::sendEvent(focus, &press);
	}
	Check(focus != nullptr && ::TranslateAccelerator(hwnd, table, &s) == 1 && g_accelCommands.back() == MAKEWPARAM(501, 1), "TranslateAccelerator: Ctrl+S while Ctrl is down");
	if(focus != nullptr)
	{
		QKeyEvent release(QEvent::KeyRelease, Qt::Key_Control, Qt::NoModifier);
		QApplication::sendEvent(focus, &release);
	}
	Check(::DestroyAcceleratorTable(table) && ::TranslateAccelerator(hwnd, table, &f5) == 0, "DestroyAcceleratorTable");
	::DestroyWindow(hwnd);
}

// Closes the next message box with its default button, once it is open.
void AnswerNextMessageBox(QString *text)
{
	QTimer::singleShot(0, [text]()
	{
		Pump();
		auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
		if(box == nullptr)
			return;
		if(text != nullptr)
			*text = box->text();
		box->done(0);
	});
}

void MfcDialogs()
{
	// MFC's message routing (PreTranslateMessage) belongs to the application.
	CWinApp theApp(L"Dialog Test");
	// An owner, disabled while the dialog runs (but not the dialog itself).
	WNDCLASS wc{};
	wc.lpfnWndProc = ::DefWindowProc;
	wc.lpszClassName = L"DialogTestOwner";
	::RegisterClass(&wc);
	HWND owner = ::CreateWindowEx(0, L"DialogTestOwner", L"Owner", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, 400, 300, nullptr, nullptr, nullptr, nullptr);

	{
		TestDialog dlg(CWnd::FromHandle(owner));
		bool ownerDisabled = false, dialogEnabled = false, visible = false, checkSet = false, radioSet = false, textSet = false, comboFilled = false, checkListOk = false;
		bool keyCheck = false, checkHeight = false;
		dlg.during = [&](TestDialog &d)
		{
			ownerDisabled = !::IsWindowEnabled(owner);
			dialogEnabled = d.IsWindowEnabled() != FALSE;
			visible = d.IsWindowVisible() != FALSE;
			checkSet = d.IsDlgButtonChecked(IdCheck) == BST_CHECKED;
			radioSet = d.IsDlgButtonChecked(IdRadio2) == BST_CHECKED && d.IsDlgButtonChecked(IdRadio1) == BST_UNCHECKED;
			CString text;
			d.GetDlgItemText(IdName, text);
			textSet = text == L"Hello" && d.GetDlgItemInt(IdNumber) == 7;
			comboFilled = d.m_combo.GetCount() == 3 && d.m_combo.GetCurSel() == 0 && d.selChanges == 0;
			checkListOk = d.m_checks.GetCount() == 3 && d.m_checks.GetCheck(1) == BST_CHECKED && d.m_checks.GetCheck(0) == BST_UNCHECKED && d.m_checks.GetItemData(0) == 100;
			checkHeight = d.m_checks.GetItemHeight(0) >= 13;
			// The user: picks another wave form, ticks a box, types a number.
			d.m_combo.SetFocus();
			Pump();
			Key(Qt::Key_Down);
			d.m_checks.SetFocus();
			Pump();
			d.m_checks.SetCurSel(0);
			Key(Qt::Key_Space);
			keyCheck = d.m_checks.GetCheck(0) == BST_CHECKED && d.checkChanges == 1;
			d.SetDlgItemText(IdNumber, L"12");
			d.CheckDlgButton(IdCheck, BST_UNCHECKED);
			d.GetDlgItem(IdName)->SetFocus();
			Pump();
			Key(Qt::Key_Return);
		};
		const INT_PTR result = dlg.DoModal();
		Check(result == IDOK, "DoModal returns IDOK after Enter");
		Check(dlg.initCalled, "OnInitDialog");
		Check(visible, "the dialog shows once its loop is idle");
		Check(ownerDisabled, "the owner is disabled while the dialog runs");
		Check(dialogEnabled, "... but the dialog it owns is not");
		Check(::IsWindowEnabled(owner) != FALSE, "the owner is enabled again afterwards");
		Check(checkSet && radioSet && textSet, "UpdateData(FALSE) puts the members into the controls");
		Check(comboFilled, "DDX_Control subclasses the controls, and they work through the classes");
		Check(dlg.selChanges == 1, "ON_CBN_SELCHANGE reaches the dialog's message map");
		Check(checkListOk, "CCheckListBox keeps checks and item data apart");
		Check(checkHeight, "CCheckListBox items are at least as tall as their check boxes");
		Check(keyCheck, "Space ticks a CCheckListBox item and sends CLBN_CHKCHANGE");
		Check(dlg.okCalled && dlg.m_number == 12 && dlg.m_check == 0 && dlg.m_radio == 1, "OnOK: UpdateData(TRUE) reads the controls back");
		Check(dlg.m_hWnd == nullptr, "the dialog window is gone after DoModal");
	}

	{
		// Bad input: a message box, the focus on the field, the dialog stays.
		TestDialog dlg(CWnd::FromHandle(owner));
		QString message;
		bool stayed = false, focused = false;
		dlg.during = [&](TestDialog &d)
		{
			d.SetDlgItemText(IdNumber, L"250");
			d.GetDlgItem(IdName)->SetFocus();
			Pump();
			AnswerNextMessageBox(&message);
			Key(Qt::Key_Return);
			stayed = ::IsWindow(d.m_hWnd) && d.IsWindowVisible();
			focused = ::GetFocus() == ::GetDlgItem(d.m_hWnd, IdNumber);
			Key(Qt::Key_Escape);
		};
		const INT_PTR result = dlg.DoModal();
		Check(!message.isEmpty() && message.contains(QStringLiteral("100")), "DDV_MinMaxInt tells the user in a message box");
		Check(stayed && focused, "... and the dialog stays open with the focus on the field");
		Check(result == IDCANCEL && dlg.m_check == 1, "Escape cancels without reading the rest of the controls");
	}

	{
		TestDialog dlg;
		dlg.endInInit = true;
		Check(dlg.DoModal() == 42, "EndDialog in OnInitDialog ends DoModal at once");
	}

	{
		// A modeless dialog from an upstream template, with its RT_DLGINIT
		// strings (controls of classes still to come are left out).
		const HINSTANCE instance = AfxGetResourceHandle();
		int found = 0;
		for(int id = 1; id < 0x8000 && found == 0; id++)
		{
			if(::FindResource(instance, MAKEINTRESOURCE(id), RT_DLGINIT) != nullptr && ::FindResource(instance, MAKEINTRESOURCE(id), RT_DIALOG) != nullptr)
				found = id;
		}
		Check(found != 0, "an upstream template with RT_DLGINIT");
		if(found != 0)
		{
			HRSRC res = ::FindResource(instance, MAKEINTRESOURCE(found), RT_DIALOG);
			const auto *bytes = static_cast<const BYTE *>(::LockResource(::LoadResource(instance, res)));
			const DWORD size = ::SizeofResource(instance, res);
			std::vector<DWORD> copy((size + 3) / 4);
			std::memcpy(copy.data(), bytes, size);
			const bool ex = reinterpret_cast<const WORD *>(bytes)[1] == 0xFFFF;
			auto *style = reinterpret_cast<DWORD *>(reinterpret_cast<BYTE *>(copy.data()) + (ex ? 12 : 0));
			*style |= DS_NOFAILCREATE;
			auto *init = static_cast<BYTE *>(::LockResource(::LoadResource(instance, ::FindResource(instance, MAKEINTRESOURCE(found), RT_DLGINIT))));
			const int combo = init[0] | (init[1] << 8);
			CDialog dlg;
			const BOOL created = dlg.CreateIndirect(reinterpret_cast<LPCDLGTEMPLATE>(copy.data()), CWnd::FromHandle(owner), init);
			Check(created && ::SendDlgItemMessage(dlg.m_hWnd, combo, CB_GETCOUNT, 0, 0) > 0, "CDialog::CreateIndirect fills combo boxes from RT_DLGINIT");
			dlg.DestroyWindow();
		}
	}

	{
		// MessageBox: buttons and return values.
		AnswerNextMessageBox(nullptr);
		Check(::MessageBox(owner, L"Save?", L"Test", MB_YESNOCANCEL | MB_ICONQUESTION) == IDCANCEL, "MessageBox: closing a Yes/No/Cancel box is Cancel");
		QTimer::singleShot(0, []()
		{
			Pump();
			if(auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget()))
				box->defaultButton()->click();
		});
		Check(::MessageBox(owner, L"Sure?", L"Test", MB_YESNO | MB_DEFBUTTON2) == IDNO, "MessageBox: MB_DEFBUTTON2 is No");
	}
	Win32PropertySheets(owner);
	MfcPropertySheets(owner);
	MfcDynamicLayouts(owner);
	::DestroyWindow(owner);
	Accelerators();
}
} // namespace

int main(int argc, char **argv)
{
	QApplication app(argc, argv);
	LinOpenMPTRegisterMptrackResources();
	// A modal loop that never ends fails the test instead of hanging it.
	QTimer watchdog;
	watchdog.setSingleShot(true);
	QObject::connect(&watchdog, &QTimer::timeout, []()
	{
		std::printf("FAIL the test hung in a modal loop\n");
		std::fflush(stdout);
		std::_Exit(2);
	});
	watchdog.start(30000);
	const HINSTANCE instance = AfxGetResourceHandle();

	// ---- a modeless dialog, driven by messages ----
	TemplateBuilder t = BuildTestDialog();
	HWND dlg = ::CreateDialogIndirectParam(instance, t.Get(), nullptr, TestDlgProc, 0);
	Check(dlg != nullptr, "CreateDialogIndirectParam");
	g_modeless = dlg;
	SetPreTranslateHook(ModelessHook);
	Check(g_initDialogs == 1, "WM_INITDIALOG");
	::ShowWindow(dlg, SW_SHOW);
	if(Win32Widget *w = FromHandle(dlg))
		w->activateWindow();
	Pump();
	HWND name = ::GetDlgItem(dlg, IdName), check = ::GetDlgItem(dlg, IdCheck), radio1 = ::GetDlgItem(dlg, IdRadio1), radio2 = ::GetDlgItem(dlg, IdRadio2);
	HWND list = ::GetDlgItem(dlg, IdList), dropList = ::GetDlgItem(dlg, IdDropList), dropDown = ::GetDlgItem(dlg, IdDropDown), number = ::GetDlgItem(dlg, IdNumber);
	HWND ok = ::GetDlgItem(dlg, IDOK), cancel = ::GetDlgItem(dlg, IDCANCEL);
	Check(name && check && radio1 && radio2 && list && dropList && dropDown && number && ok && cancel, "every control is created");
	{
		wchar_t cls[32] = {};
		::GetClassName(dropList, cls, 32);
		Check(std::wcscmp(cls, L"ComboBox") == 0, "control classes by atom");
	}
	Check(::GetFocus() == name, "the focus starts on the first tab stop");
	Check(::GetDlgCtrlID(name) == IdName && ::GetParent(name) == dlg, "control ids and parents");

	// Dialog units.
	{
		RECT rc{0, 0, 4, 8};
		::MapDialogRect(dlg, &rc);
		RECT window{};
		::GetWindowRect(ok, &window);
		RECT okUnits{0, 0, 50, 14};
		::MapDialogRect(dlg, &okUnits);
		Check(rc.right > 4 && rc.bottom > 8, "MapDialogRect scales dialog units");
		Check(window.right - window.left == okUnits.right && window.bottom - window.top == okUnits.bottom, "controls are sized in dialog units");
	}

	// Edits.
	g_notes.clear();
	::SetDlgItemText(dlg, IdName, L"Sample");
	Check(Text(name) == L"Sample", "SetDlgItemText / GetWindowText on an edit");
	Check(Got(IdName, EN_UPDATE) && Got(IdName, EN_CHANGE), "WM_SETTEXT on a single-line edit sends EN_UPDATE and EN_CHANGE");
	Check(IndexOf(IdName, EN_UPDATE) < IndexOf(IdName, EN_CHANGE), "EN_UPDATE comes before EN_CHANGE");
	::SendMessage(name, EM_SETSEL, 1, 3);
	{
		DWORD start = 0, end = 0;
		::SendMessage(name, EM_GETSEL, reinterpret_cast<WPARAM>(&start), reinterpret_cast<LPARAM>(&end));
		Check(start == 1 && end == 3, "EM_SETSEL / EM_GETSEL");
		::SendMessage(name, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(L"XY"));
		Check(Text(name) == L"SXYple", "EM_REPLACESEL");
	}
	::SendMessage(name, EM_LIMITTEXT, 8, 0);
	::SendMessage(name, EM_SETSEL, 6, 6);
	g_notes.clear();
	Key(Qt::Key_A, Qt::NoModifier, QStringLiteral("a"));
	Key(Qt::Key_B, Qt::NoModifier, QStringLiteral("b"));
	Check(!Got(IdName, EN_MAXTEXT), "no EN_MAXTEXT below the limit");
	Key(Qt::Key_C, Qt::NoModifier, QStringLiteral("c"));
	Check(Text(name) == L"SXYpleab", "typing into an edit, up to EM_LIMITTEXT");
	Check(Got(IdName, EN_MAXTEXT), "EN_MAXTEXT");
	Check(::GetDlgItemInt(dlg, IdNumber, nullptr, FALSE) == 42, "GetDlgItemInt");
	::SetDlgItemInt(dlg, IdNumber, static_cast<UINT>(-7), TRUE);
	{
		BOOL translated = FALSE;
		Check(static_cast<int>(::GetDlgItemInt(dlg, IdNumber, &translated, TRUE)) == -7 && translated, "SetDlgItemInt signed");
	}

	// Tab order and the dialog manager.
	Key(Qt::Key_Tab);
	Check(::GetFocus() == check, "Tab moves to the next tab stop");
	Key(Qt::Key_Tab);
	Check(::GetFocus() == radio1, "Tab skips controls without WS_TABSTOP");
	Key(Qt::Key_Backtab, Qt::ShiftModifier);
	Check(::GetFocus() == check, "Shift+Tab moves back");
	Key(Qt::Key_Tab);
	g_notes.clear();
	Key(Qt::Key_Right);
	Check(::GetFocus() == radio2, "arrows move within a group");
	Check(::IsDlgButtonChecked(dlg, IdRadio2) == BST_CHECKED && ::IsDlgButtonChecked(dlg, IdRadio1) == BST_UNCHECKED, "arrowing onto an auto radio button checks it");
	Check(Got(IdRadio2, BN_CLICKED), "... and sends BN_CLICKED");
	Key(Qt::Key_Tab);
	Check(::GetFocus() == list, "Tab leaves the group");

	// Buttons.
	g_notes.clear();
	::SendMessage(check, BM_CLICK, 0, 0);
	Check(::IsDlgButtonChecked(dlg, IdCheck) == BST_CHECKED && Got(IdCheck, BN_CLICKED), "BM_CLICK on an auto check box");
	::CheckDlgButton(dlg, IdCheck, BST_UNCHECKED);
	Check(::SendMessage(check, BM_GETCHECK, 0, 0) == BST_UNCHECKED, "CheckDlgButton / BM_GETCHECK");
	::CheckRadioButton(dlg, IdRadio1, IdRadio2, IdRadio1);
	Check(::IsDlgButtonChecked(dlg, IdRadio1) == BST_CHECKED && ::IsDlgButtonChecked(dlg, IdRadio2) == BST_UNCHECKED, "CheckRadioButton");
	Check((::SendMessage(ok, WM_GETDLGCODE, 0, 0) & DLGC_DEFPUSHBUTTON) != 0, "the default push button");
	Check(LOWORD(::SendMessage(dlg, DM_GETDEFID, 0, 0)) == IDOK, "DM_GETDEFID");

	// List boxes.
	g_notes.clear();
	::SendMessage(list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"pear"));
	::SendMessage(list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"apple"));
	::SendMessage(list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Banana"));
	Check(::SendMessage(list, LB_GETCOUNT, 0, 0) == 3, "LB_ADDSTRING / LB_GETCOUNT");
	{
		wchar_t buf[32] = {};
		::SendMessage(list, LB_GETTEXT, 1, reinterpret_cast<LPARAM>(buf));
		Check(std::wcscmp(buf, L"Banana") == 0, "LBS_SORT sorts without case");
	}
	Check(::SendMessage(list, LB_FINDSTRING, static_cast<WPARAM>(-1), reinterpret_cast<LPARAM>(L"PE")) == 2, "LB_FINDSTRING is a prefix match");
	::SendMessage(list, LB_SETITEMDATA, 2, 1234);
	Check(::SendMessage(list, LB_GETITEMDATA, 2, 0) == 1234, "LB_SETITEMDATA / LB_GETITEMDATA");
	::SendMessage(list, LB_SETCURSEL, 0, 0);
	Check(::SendMessage(list, LB_GETCURSEL, 0, 0) == 0 && !Got(IdList, LBN_SELCHANGE), "LB_SETCURSEL, no notification");
	Key(Qt::Key_Down);
	Check(::SendMessage(list, LB_GETCURSEL, 0, 0) == 1 && Got(IdList, LBN_SELCHANGE), "Down in a list box selects the next item and notifies");

	// Drop-down list combo boxes.
	g_notes.clear();
	for(const wchar_t *s : {L"Sine", L"Square", L"Saw", L"Noise"})
		::SendMessage(dropList, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(s));
	Check(::SendMessage(dropList, CB_GETCOUNT, 0, 0) == 4, "CB_ADDSTRING / CB_GETCOUNT");
	Check(::SendMessage(dropList, CB_GETCURSEL, 0, 0) == CB_ERR, "no selection to begin with");
	::SendMessage(dropList, CB_SETCURSEL, 1, 0);
	Check(::SendMessage(dropList, CB_GETCURSEL, 0, 0) == 1 && Text(dropList) == L"Square", "CB_SETCURSEL and the combo box's text");
	{
		RECT rc{};
		::GetWindowRect(dropList, &rc);
		const LRESULT field = ::SendMessage(dropList, CB_GETITEMHEIGHT, static_cast<WPARAM>(-1), 0);
		Check(rc.bottom - rc.top < 40 && rc.bottom - rc.top == field + 6, "a drop-down combo box keeps its closed height");
		COMBOBOXINFO info{sizeof(info)};
		Check(::GetComboBoxInfo(dropList, &info) && info.hwndList != nullptr && info.hwndItem == dropList, "GetComboBoxInfo");
	}
	::SetFocus(dropList);
	Pump();
	Check(Got(IdDropList, CBN_SETFOCUS), "CBN_SETFOCUS");
	g_notes.clear();
	Key(Qt::Key_Down);
	Check(::SendMessage(dropList, CB_GETCURSEL, 0, 0) == 2 && Got(IdDropList, CBN_SELCHANGE), "Down on a closed combo box selects the next item");
	Key(Qt::Key_N, Qt::NoModifier, QStringLiteral("n"));
	Check(::SendMessage(dropList, CB_GETCURSEL, 0, 0) == 3, "typing selects by the first letter");
	g_notes.clear();
	Key(Qt::Key_F4);
	COMBOBOXINFO info{sizeof(info)};
	::GetComboBoxInfo(dropList, &info);
	Check(::SendMessage(dropList, CB_GETDROPPEDSTATE, 0, 0) == TRUE && ::IsWindowVisible(info.hwndList), "F4 drops the list down");
	Check(Got(IdDropList, CBN_DROPDOWN), "CBN_DROPDOWN");
	Check(::GetFocus() == dropList, "the focus stays on the combo box while it is dropped");
	{
		RECT combo{}, dropped{};
		::GetWindowRect(dropList, &combo);
		::GetWindowRect(info.hwndList, &dropped);
		Check(dropped.top == combo.bottom || dropped.bottom == combo.top, "the list opens below (or above) the combo box");
	}
	Key(Qt::Key_Up);
	Check(::SendMessage(dropList, CB_GETCURSEL, 0, 0) == 2 && Got(IdDropList, CBN_SELCHANGE), "Up in the open list changes the selection");
	g_notes.clear();
	Key(Qt::Key_Return);
	Check(::SendMessage(dropList, CB_GETDROPPEDSTATE, 0, 0) == FALSE && !::IsWindowVisible(info.hwndList), "Enter closes the list");
	Check(Got(IdDropList, CBN_SELENDOK) && Got(IdDropList, CBN_CLOSEUP), "CBN_SELENDOK, CBN_CLOSEUP");
	Check(!Got(IDOK, BN_CLICKED) && g_commands.empty(), "... and does not press the default button");
	Check(Text(dropList) == L"Saw", "the chosen item");
	g_notes.clear();
	::SendMessage(dropList, CB_SHOWDROPDOWN, TRUE, 0);
	Key(Qt::Key_Down);
	Key(Qt::Key_Escape);
	Check(::SendMessage(dropList, CB_GETDROPPEDSTATE, 0, 0) == FALSE && Got(IdDropList, CBN_CLOSEUP), "Escape closes the list");
	Check(g_commands.empty(), "... and does not cancel the dialog");
	Check(::SendMessage(dropList, CB_SELECTSTRING, static_cast<WPARAM>(-1), reinterpret_cast<LPARAM>(L"si")) == 0 && Text(dropList) == L"Sine", "CB_SELECTSTRING");
	::SendMessage(dropList, CB_RESETCONTENT, 0, 0);
	Check(::SendMessage(dropList, CB_GETCOUNT, 0, 0) == 0 && ::SendMessage(dropList, CB_GETCURSEL, 0, 0) == CB_ERR, "CB_RESETCONTENT");

	// Drop-down combo boxes (with an edit).
	g_notes.clear();
	HWND comboEdit = ::GetDlgItem(dropDown, 1001);
	Check(comboEdit != nullptr, "a drop-down combo box's edit is item 1001");
	for(const wchar_t *s : {L"120", L"125", L"140"})
		::SendMessage(dropDown, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(s));
	::SendMessage(dropDown, CB_SETCURSEL, 2, 0);
	Check(Text(dropDown) == L"140" && Text(comboEdit) == L"140", "CB_SETCURSEL sets the edit's text");
	Check(!Got(IdDropDown, CBN_EDITCHANGE), "... without CBN_EDITCHANGE");
	::SetWindowText(dropDown, L"133");
	Check(Text(comboEdit) == L"133" && ::SendMessage(dropDown, CB_GETCURSEL, 0, 0) == CB_ERR, "WM_SETTEXT on a drop-down combo box");
	::SetFocus(dropDown);
	Pump();
	Check(::GetFocus() == comboEdit, "focusing the combo box focuses its edit");
	Check(Got(IdDropDown, CBN_SETFOCUS), "CBN_SETFOCUS from the edit");
	g_notes.clear();
	::SendMessage(comboEdit, EM_SETSEL, 3, 3);
	Key(Qt::Key_7, Qt::NoModifier, QStringLiteral("7"));
	Check(Text(dropDown) == L"1337" && Got(IdDropDown, CBN_EDITCHANGE) && Got(IdDropDown, CBN_EDITUPDATE), "typing into the edit: CBN_EDITUPDATE, CBN_EDITCHANGE");
	g_notes.clear();
	Key(Qt::Key_Down);
	Check(::SendMessage(dropDown, CB_GETCURSEL, 0, 0) == 0 && Text(comboEdit) == L"120" && Got(IdDropDown, CBN_SELCHANGE), "Down in the edit selects an item");
	Key(Qt::Key_Down, Qt::AltModifier);
	Check(::SendMessage(dropDown, CB_GETDROPPEDSTATE, 0, 0) == TRUE, "Alt+Down drops the list down");
	Key(Qt::Key_Down, Qt::AltModifier);
	Check(::SendMessage(dropDown, CB_GETDROPPEDSTATE, 0, 0) == FALSE, "... and Alt+Down closes it");
	::SetWindowPos(dropDown, nullptr, 0, 0, 120, 200, SWP_NOMOVE | SWP_NOZORDER);
	{
		RECT rc{};
		::GetWindowRect(dropDown, &rc);
		Check(rc.right - rc.left == 120 && rc.bottom - rc.top < 40, "resizing a combo box keeps its height");
	}

	SaveShot(dlg, 0);

	// Enter and Escape in the dialog.
	::SetFocus(name);
	Pump();
	g_commands.clear();
	Key(Qt::Key_Return);
	Check(g_commands.size() == 1 && g_commands[0] == IDOK, "Enter presses the default button");
	::DestroyWindow(dlg);
	g_modeless = nullptr;
	Pump();

	// ---- a modal dialog ----
	g_commands.clear();
	QTimer::singleShot(0, []()
	{
		Pump();
		Key(Qt::Key_Escape);
	});
	const INT_PTR result = ::DialogBoxIndirectParam(instance, t.Get(), nullptr, TestDlgProc, 0);
	Check(result == IDCANCEL, "DialogBox: Escape ends it with IDCANCEL");
	g_commands.clear();
	QTimer::singleShot(0, []()
	{
		Pump();
		Key(Qt::Key_L, Qt::AltModifier, QStringLiteral("l"));
		Key(Qt::Key_Return);
	});
	g_notes.clear();
	const INT_PTR result2 = ::DialogBoxIndirectParam(instance, t.Get(), nullptr, TestDlgProc, 0);
	Check(result2 == IDOK, "DialogBox: Enter ends it with IDOK");
	Check(Got(IdCheck, BN_CLICKED), "Alt+mnemonic clicks the check box");

	UpstreamTemplates();
	MfcDialogs();

	std::printf("%s: %d failure(s)\n", g_failures ? "FAILED" : "passed", g_failures);
	return g_failures ? 1 : 0;
}
