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

#include <cstdio>
#include <cstring>
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
	static const std::set<std::wstring> standard{L"Button", L"Edit", L"Static", L"ListBox", L"ScrollBar", L"ComboBox"};
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

} // namespace

int main(int argc, char **argv)
{
	QApplication app(argc, argv);
	LinOpenMPTRegisterMptrackResources();
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

	std::printf("%s: %d failure(s)\n", g_failures ? "FAILED" : "passed", g_failures);
	return g_failures ? 1 : 0;
}
