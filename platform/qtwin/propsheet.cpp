// Property sheets, as comctl32 makes them: PropertySheet(),
// CreatePropertySheetPage() and DestroyPropertySheetPage().
//
// A sheet is a dialog with a tab control and the OK, Cancel, Apply and Help
// buttons; each page is a child dialog (DS_CONTROL) made from its own
// template when it is first shown, placed in the tab control's display area.
// Pages hear about the sheet through WM_NOTIFY (PSN_SETACTIVE,
// PSN_KILLACTIVE, PSN_APPLY, PSN_RESET, PSN_QUERYCANCEL, PSN_HELP) and talk
// to it with the PSM_* messages. Modal sheets return from PropertySheet()
// when closed; modeless ones (PSH_MODELESS) report the end by
// PSM_GETCURRENTPAGEHWND returning NULL, as there. Wizards are not done:
// OpenMPT has none.
#include "QtWin.h"

#include <QtGui/QRegion>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>

struct _PSP
{
	PROPSHEETPAGEW psp{};
	std::wstring title;    // copies of what the pointers point to
	std::wstring templateName;
};

namespace linopenmpt::qtwin
{

namespace
{

constexpr int MarginDlu = 4;
constexpr int ButtonWidthDlu = 50;
constexpr int ButtonHeightDlu = 14;
constexpr int ButtonGapDlu = 4;

// ---- dialog templates --------------------------------------------------------------

std::wstring FromUtf16(const WORD *p, const WORD **end = nullptr)
{
	std::u16string s;
	while(*p != 0)
		s.push_back(static_cast<char16_t>(*p++));
	if(end != nullptr)
		*end = p + 1;
	return QString::fromUtf16(s.data(), static_cast<qsizetype>(s.size())).toStdWString();
}

const WORD *SkipNameOrOrdinal(const WORD *p)
{
	if(*p == 0)
		return p + 1;
	if(*p == 0xFFFF)
		return p + 2;
	while(*p != 0)
		p++;
	return p + 1;
}

const BYTE *AlignDword(const BYTE *base, const BYTE *p)
{
	const auto offset = static_cast<std::size_t>(p - base);
	return base + ((offset + 3) & ~std::size_t(3));
}

struct TemplateInfo
{
	bool ex = false;
	DWORD style = 0;
	short cx = 0, cy = 0;
	std::wstring title;
	bool hasFont = false;
	WORD pointSize = 8, weight = FW_NORMAL;
	BYTE italic = 0, charset = DEFAULT_CHARSET;
	std::wstring face;
	std::size_t size = 0; // in bytes, the whole template
};

TemplateInfo ReadTemplate(LPCDLGTEMPLATE dlgTemplate)
{
	TemplateInfo t;
	const BYTE *base = reinterpret_cast<const BYTE *>(dlgTemplate);
	auto word = [](const BYTE *at) { WORD v; std::memcpy(&v, at, 2); return v; };
	auto dword = [](const BYTE *at) { DWORD v; std::memcpy(&v, at, 4); return v; };
	t.ex = word(base) == 1 && word(base + 2) == 0xFFFF;
	const BYTE *p = base;
	WORD count = 0;
	if(t.ex)
	{
		t.style = dword(base + 12);
		count = word(base + 16);
		t.cx = static_cast<short>(word(base + 22));
		t.cy = static_cast<short>(word(base + 24));
		p = base + 26;
	} else
	{
		t.style = dword(base);
		count = word(base + 8);
		t.cx = static_cast<short>(word(base + 14));
		t.cy = static_cast<short>(word(base + 16));
		p = base + 18;
	}
	const WORD *w = reinterpret_cast<const WORD *>(p);
	w = SkipNameOrOrdinal(w); // menu
	w = SkipNameOrOrdinal(w); // class
	t.title = FromUtf16(w, &w);
	if(t.style & DS_SETFONT)
	{
		t.hasFont = true;
		t.pointSize = *w++;
		if(t.ex)
		{
			t.weight = *w++;
			const BYTE *b = reinterpret_cast<const BYTE *>(w);
			t.italic = b[0];
			t.charset = b[1];
			w++;
		}
		t.face = FromUtf16(w, &w);
	}
	p = reinterpret_cast<const BYTE *>(w);
	for(WORD i = 0; i < count; i++)
	{
		p = AlignDword(base, p);
		p += t.ex ? 24 : 18;
		w = reinterpret_cast<const WORD *>(p);
		w = SkipNameOrOrdinal(w); // class
		w = SkipNameOrOrdinal(w); // title
		const WORD extra = *w++;
		p = reinterpret_cast<const BYTE *>(w) + extra;
	}
	t.size = static_cast<std::size_t>(p - base);
	return t;
}

// Builds a DLGTEMPLATE (not the EX kind) in memory, DWORD aligned.
class TemplateWriter
{
public:
	void Word(WORD w)
	{
		const std::size_t at = bytes.size();
		bytes.resize(at + 2);
		std::memcpy(bytes.data() + at, &w, 2);
	}
	void Dword(DWORD d)
	{
		Word(LOWORD(d));
		Word(HIWORD(d));
	}
	void String(const std::wstring &s)
	{
		const QString q = QString::fromStdWString(s);
		for(QChar c : q)
			Word(c.unicode());
		Word(0);
	}
	void Align()
	{
		while(bytes.size() % 4 != 0)
			bytes.push_back(0);
	}
	void Header(DWORD style, WORD count, short cx, short cy, const std::wstring &title, WORD pointSize, const std::wstring &face)
	{
		Dword(style);
		Dword(0);
		Word(count);
		Word(0);
		Word(0);
		Word(static_cast<WORD>(cx));
		Word(static_cast<WORD>(cy));
		Word(0); // menu
		Word(0); // class
		String(title);
		if(style & DS_SETFONT)
		{
			Word(pointSize);
			String(face);
		}
	}
	void Item(DWORD style, short x, short y, short cx, short cy, WORD id, const std::wstring &cls, WORD atom, const std::wstring &text)
	{
		Align();
		Dword(style);
		Dword(0);
		Word(static_cast<WORD>(x));
		Word(static_cast<WORD>(y));
		Word(static_cast<WORD>(cx));
		Word(static_cast<WORD>(cy));
		Word(id);
		if(atom != 0)
		{
			Word(0xFFFF);
			Word(atom);
		} else
		{
			String(cls);
		}
		String(text);
		Word(0); // no creation data
	}
	LPDLGTEMPLATE Get()
	{
		storage.assign((bytes.size() + 3) / 4, 0);
		std::memcpy(storage.data(), bytes.data(), bytes.size());
		return reinterpret_cast<LPDLGTEMPLATE>(storage.data());
	}

private:
	std::vector<BYTE> bytes;
	std::vector<DWORD> storage;
};

// ---- pages ---------------------------------------------------------------------------

std::wstring ResourceString(HINSTANCE instance, LPCWSTR text)
{
	if(text == nullptr)
		return {};
	if(IS_INTRESOURCE(text))
	{
		WCHAR buffer[256] = {};
		::LoadString(instance, static_cast<UINT>(reinterpret_cast<UINT_PTR>(text)), buffer, 256);
		return buffer;
	}
	return text;
}

LPCDLGTEMPLATE PageTemplate(const PROPSHEETPAGEW &psp)
{
	if(psp.dwFlags & PSP_DLGINDIRECT)
		return psp.pResource;
	HRSRC res = ::FindResource(psp.hInstance, psp.pszTemplate, RT_DIALOG);
	if(res == nullptr)
		return nullptr;
	return static_cast<LPCDLGTEMPLATE>(::LockResource(::LoadResource(psp.hInstance, res)));
}

struct Page
{
	HPROPSHEETPAGE hpage = nullptr;
	HWND hwnd = nullptr;
	std::wstring title;
	short cx = 0, cy = 0; // dialog units
	std::vector<DWORD> templateCopy;
};

struct Sheet
{
	PROPSHEETHEADERW header{};
	std::wstring caption;
	std::vector<Page> pages;
	HWND hwnd = nullptr;
	HWND tab = nullptr;
	int active = -1;
	bool activeValid = true;
	bool ended = false;
	bool modeless = false;
	INT_PTR result = 0;
	std::set<HWND> changed;
	std::wstring fontFace = L"MS Shell Dlg";
	WORD fontSize = 8;
};

std::map<HWND, Sheet *> &Sheets()
{
	static std::map<HWND, Sheet *> sheets;
	return sheets;
}

// Sheets alive, so that one destroyed while being made is not freed twice.
std::set<Sheet *> &Live()
{
	static std::set<Sheet *> live;
	return live;
}

Sheet *SheetOf(HWND hwnd)
{
	auto it = Sheets().find(hwnd);
	return it != Sheets().end() ? it->second : nullptr;
}

void ReadPageInfo(Page &page)
{
	const PROPSHEETPAGEW &psp = page.hpage->psp;
	LPCDLGTEMPLATE t = PageTemplate(psp);
	TemplateInfo info;
	if(t != nullptr)
		info = ReadTemplate(t);
	page.cx = info.cx;
	page.cy = info.cy;
	page.title = (psp.dwFlags & PSP_USETITLE) ? ResourceString(psp.hInstance, psp.pszTitle) : info.title;
}

LRESULT NotifyPage(const Sheet &s, HWND page, UINT code, LPARAM lParam = 0)
{
	if(page == nullptr)
		return 0;
	PSHNOTIFY nm{};
	nm.hdr.hwndFrom = s.hwnd;
	nm.hdr.idFrom = 0;
	nm.hdr.code = code;
	nm.lParam = lParam;
	return ::SendMessage(page, WM_NOTIFY, 0, reinterpret_cast<LPARAM>(&nm));
}

RECT DisplayRect(const Sheet &s)
{
	RECT rc{};
	::GetWindowRect(s.tab, &rc);
	::MapWindowPoints(nullptr, s.hwnd, reinterpret_cast<POINT *>(&rc), 2);
	::SendMessage(s.tab, TCM_ADJUSTRECT, FALSE, reinterpret_cast<LPARAM>(&rc));
	return rc;
}

bool CreatePage(Sheet &s, int index)
{
	Page &page = s.pages[index];
	if(page.hwnd != nullptr)
		return true;
	PROPSHEETPAGEW &psp = page.hpage->psp;
	LPCDLGTEMPLATE source = PageTemplate(psp);
	if(source == nullptr)
		return false;
	// A copy, made a child dialog: comctl32 changes the style too.
	const TemplateInfo info = ReadTemplate(source);
	page.templateCopy.assign((info.size + 3) / 4 + 1, 0);
	std::memcpy(page.templateCopy.data(), source, info.size);
	BYTE *bytes = reinterpret_cast<BYTE *>(page.templateCopy.data());
	DWORD style = info.style;
	style &= ~(DS_MODALFRAME | WS_CAPTION | WS_SYSMENU | WS_POPUP | WS_DISABLED | WS_VISIBLE | WS_THICKFRAME);
	style |= DS_3DLOOK | DS_CONTROL | WS_CHILD | WS_TABSTOP;
	std::memcpy(bytes + (info.ex ? 12 : 0), &style, 4);
	if((psp.dwFlags & PSP_USECALLBACK) && psp.pfnCallback != nullptr)
	{
		if(!psp.pfnCallback(nullptr, PSPCB_CREATE, &psp))
			return false;
	}
	const HWND hwnd = ::CreateDialogIndirectParam(psp.hInstance, reinterpret_cast<LPCDLGTEMPLATE>(bytes), s.hwnd, psp.pfnDlgProc, reinterpret_cast<LPARAM>(&psp));
	if(hwnd == nullptr)
		return false;
	// The sheet may have gone while the page was made.
	if(SheetOf(s.hwnd) != &s)
		return false;
	s.pages[index].hwnd = hwnd;
	const RECT display = DisplayRect(s);
	// After the tab control in the Z order, so in the tab order, as there.
	::SetWindowPos(hwnd, s.tab, display.left, display.top, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE);
	return true;
}

// The page's first control that takes the focus (GetNextDlgTabItem() would
// start from the sheet: the page is part of its navigation).
void FocusFirst(HWND page)
{
	for(HWND c = ::GetWindow(page, GW_CHILD); c != nullptr; c = ::GetWindow(c, GW_HWNDNEXT))
	{
		const LONG_PTR style = ::GetWindowLongPtr(c, GWL_STYLE);
		if((style & (WS_TABSTOP | WS_VISIBLE | WS_DISABLED)) == (WS_TABSTOP | WS_VISIBLE))
		{
			::SetFocus(c);
			return;
		}
	}
	::SetFocus(page);
}

// Activates a page. killSent: PSN_KILLACTIVE has been sent (and allowed) already.
bool SetCurSel(Sheet &s, int index, bool killSent = false)
{
	if(index < 0 || static_cast<std::size_t>(index) >= s.pages.size())
		return false;
	const HWND hwnd = s.hwnd;
	const HWND old = s.active >= 0 ? s.pages[s.active].hwnd : nullptr;
	if(index == s.active && old != nullptr)
		return true;
	if(old != nullptr && !killSent)
	{
		if(NotifyPage(s, old, PSN_KILLACTIVE))
			return false;
		if(SheetOf(hwnd) != &s)
			return false;
	}
	// A page may refuse with -1: the next one in the same direction is tried.
	const int step = index >= s.active ? 1 : -1;
	for(int i = index; i >= 0 && static_cast<std::size_t>(i) < s.pages.size(); i += step)
	{
		if(!CreatePage(s, i))
			continue;
		const HWND page = s.pages[i].hwnd;
		const LRESULT result = NotifyPage(s, page, PSN_SETACTIVE);
		if(SheetOf(hwnd) != &s)
			return false;
		if(result == -1)
			continue;
		const HWND focus = ::GetFocus();
		const bool focusInOld = old != nullptr && (focus == old || ::IsChild(old, focus));
		::ShowWindow(page, SW_SHOWNA);
		if(old != nullptr && old != page)
			::ShowWindow(old, SW_HIDE);
		s.active = i;
		::SendMessage(s.tab, TCM_SETCURSEL, static_cast<WPARAM>(i), 0);
		if(focusInOld)
			FocusFirst(page);
		return true;
	}
	return false;
}

void Layout(Sheet &s)
{
	const HWND hwnd = s.hwnd;
	short cx = 0, cy = 0;
	for(const Page &page : s.pages)
	{
		cx = std::max(cx, page.cx);
		cy = std::max(cy, page.cy);
	}
	RECT page{0, 0, cx, cy};
	::MapDialogRect(hwnd, &page);
	RECT margin{0, 0, MarginDlu, MarginDlu};
	::MapDialogRect(hwnd, &margin);
	RECT button{0, 0, ButtonWidthDlu, ButtonHeightDlu};
	::MapDialogRect(hwnd, &button);
	RECT gap{0, 0, ButtonGapDlu, 0};
	::MapDialogRect(hwnd, &gap);

	RECT tab = page;
	::SendMessage(s.tab, TCM_ADJUSTRECT, TRUE, reinterpret_cast<LPARAM>(&tab));
	::OffsetRect(&tab, margin.right - tab.left, margin.bottom - tab.top);
	::SetWindowPos(s.tab, nullptr, tab.left, tab.top, tab.right - tab.left, tab.bottom - tab.top, SWP_NOZORDER | SWP_NOACTIVATE);
	const RECT display = DisplayRect(s);
	for(const Page &p : s.pages)
	{
		if(p.hwnd != nullptr)
			::SetWindowPos(p.hwnd, nullptr, display.left, display.top, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
	}
	// The tab control leaves its display area to the page under it, as
	// WS_CLIPSIBLINGS does on Windows.
	if(Win32Widget *tabWidget = FromHandle(s.tab))
	{
		QRegion region(0, 0, tab.right - tab.left, tab.bottom - tab.top);
		region -= QRegion(display.left - tab.left, display.top - tab.top, display.right - display.left, display.bottom - display.top);
		tabWidget->setMask(region);
	}

	std::vector<HWND> buttons;
	for(int id : {IDOK, IDCANCEL, IDC_APPLY_BUTTON, IDHELP})
	{
		const HWND b = ::GetDlgItem(hwnd, id);
		if(b != nullptr && (::GetWindowLongPtr(b, GWL_STYLE) & WS_VISIBLE))
			buttons.push_back(b);
	}
	const int bw = button.right, bh = button.bottom;
	const int count = static_cast<int>(buttons.size());
	int x = tab.right - (count * bw + std::max(0, count - 1) * gap.right);
	const int y = tab.bottom + margin.bottom;
	for(HWND b : buttons)
	{
		::SetWindowPos(b, nullptr, x, y, bw, bh, SWP_NOZORDER | SWP_NOACTIVATE);
		x += bw + gap.right;
	}
	RECT client{0, 0, tab.right + margin.right, y + bh + margin.bottom};
	const DWORD style = static_cast<DWORD>(::GetWindowLongPtr(hwnd, GWL_STYLE));
	const DWORD exStyle = static_cast<DWORD>(::GetWindowLongPtr(hwnd, GWL_EXSTYLE));
	::AdjustWindowRectEx(&client, style, FALSE, exStyle);
	RECT window{};
	::GetWindowRect(hwnd, &window);
	const int width = client.right - client.left, height = client.bottom - client.top;
	// Keep the centre where the dialog manager put it.
	const int left = (window.left + window.right) / 2 - width / 2;
	const int top = (window.top + window.bottom) / 2 - height / 2;
	::SetWindowPos(hwnd, nullptr, left, top, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
}

void InsertTab(Sheet &s, int index)
{
	TCITEM item{};
	item.mask = TCIF_TEXT;
	item.pszText = const_cast<LPWSTR>(s.pages[index].title.c_str());
	::SendMessage(s.tab, TCM_INSERTITEM, static_cast<WPARAM>(index), reinterpret_cast<LPARAM>(&item));
}

void EndSheet(Sheet &s)
{
	s.ended = true;
	if(s.modeless)
		s.activeValid = false;
	else
		::EndDialog(s.hwnd, s.result);
}

// OK (closing) or Apply: the active page must agree to be left, then every
// page applies. False when a page found an error.
bool Apply(Sheet &s, bool closing)
{
	const HWND hwnd = s.hwnd;
	if(s.active >= 0 && NotifyPage(s, s.pages[s.active].hwnd, PSN_KILLACTIVE))
		return false;
	for(std::size_t i = 0; i < s.pages.size(); i++)
	{
		if(SheetOf(hwnd) != &s)
			return false;
		const HWND page = s.pages[i].hwnd;
		if(page == nullptr)
			continue;
		const LRESULT result = NotifyPage(s, page, PSN_APPLY, closing ? TRUE : FALSE);
		if(SheetOf(hwnd) != &s)
			return false;
		if(result == PSNRET_INVALID)
		{
			SetCurSel(s, static_cast<int>(i), true);
			return false;
		}
		if(result == PSNRET_INVALID_NOCHANGEPAGE)
		{
			if(s.active >= 0)
				NotifyPage(s, s.pages[s.active].hwnd, PSN_SETACTIVE);
			return false;
		}
	}
	if(SheetOf(hwnd) != &s)
		return false;
	s.result |= 1;
	s.changed.clear();
	if(HWND apply = ::GetDlgItem(hwnd, IDC_APPLY_BUTTON))
		::EnableWindow(apply, FALSE);
	// Applying without closing: the active page carries on.
	if(!closing && s.active >= 0)
		NotifyPage(s, s.pages[s.active].hwnd, PSN_SETACTIVE);
	return true;
}

void Cancel(Sheet &s)
{
	const HWND hwnd = s.hwnd;
	if(s.active >= 0 && NotifyPage(s, s.pages[s.active].hwnd, PSN_QUERYCANCEL))
		return;
	for(std::size_t i = 0; i < s.pages.size(); i++)
	{
		if(SheetOf(hwnd) != &s)
			return;
		if(s.pages[i].hwnd != nullptr)
			NotifyPage(s, s.pages[i].hwnd, PSN_RESET, FALSE);
	}
	if(SheetOf(hwnd) == &s)
		EndSheet(s);
}

void RemovePage(Sheet &s, int index)
{
	if(index < 0 || static_cast<std::size_t>(index) >= s.pages.size())
		return;
	if(index == s.active)
	{
		// Another page first, if there is one.
		const int next = static_cast<std::size_t>(index) + 1 < s.pages.size() ? index + 1 : index - 1;
		if(next >= 0)
			SetCurSel(s, next, true);
	}
	Page page = std::move(s.pages[index]);
	s.pages.erase(s.pages.begin() + index);
	if(s.active > index)
		s.active--;
	else if(s.active == index)
		s.active = -1;
	::SendMessage(s.tab, TCM_DELETEITEM, static_cast<WPARAM>(index), 0);
	if(s.active >= 0)
		::SendMessage(s.tab, TCM_SETCURSEL, static_cast<WPARAM>(s.active), 0);
	if(page.hwnd != nullptr)
	{
		s.changed.erase(page.hwnd);
		::DestroyWindow(page.hwnd);
	}
	::DestroyPropertySheetPage(page.hpage);
}

int IndexOfPage(const Sheet &s, HPROPSHEETPAGE hpage)
{
	for(std::size_t i = 0; i < s.pages.size(); i++)
	{
		if(s.pages[i].hpage == hpage)
			return static_cast<int>(i);
	}
	return -1;
}

int IndexOfHwnd(const Sheet &s, HWND hwnd)
{
	for(std::size_t i = 0; i < s.pages.size(); i++)
	{
		if(s.pages[i].hwnd == hwnd && hwnd != nullptr)
			return static_cast<int>(i);
	}
	return -1;
}

int IndexOfId(const Sheet &s, LPARAM id)
{
	for(std::size_t i = 0; i < s.pages.size(); i++)
	{
		const PROPSHEETPAGEW &psp = s.pages[i].hpage->psp;
		if(!(psp.dwFlags & PSP_DLGINDIRECT) && psp.pszTemplate == reinterpret_cast<LPCWSTR>(id))
			return static_cast<int>(i);
		if(!(psp.dwFlags & PSP_DLGINDIRECT) && !IS_INTRESOURCE(psp.pszTemplate) && !IS_INTRESOURCE(reinterpret_cast<LPCWSTR>(id)) && std::wcscmp(psp.pszTemplate, reinterpret_cast<LPCWSTR>(id)) == 0)
			return static_cast<int>(i);
	}
	return -1;
}

void AddPage(Sheet &s, int index, HPROPSHEETPAGE hpage)
{
	Page page;
	page.hpage = hpage;
	ReadPageInfo(page);
	index = std::clamp(index, 0, static_cast<int>(s.pages.size()));
	s.pages.insert(s.pages.begin() + index, std::move(page));
	if(s.active >= index)
		s.active++;
	InsertTab(s, index);
	if(s.active >= 0)
		::SendMessage(s.tab, TCM_SETCURSEL, static_cast<WPARAM>(s.active), 0);
}

void SetTitle(Sheet &s, DWORD style, LPCWSTR text)
{
	std::wstring title = ResourceString(s.header.hInstance, text);
	if(style & PSH_PROPTITLE)
		title += L" Properties";
	::SetWindowText(s.hwnd, title.c_str());
}

bool PressButton(Sheet &s, int button)
{
	int id = 0;
	switch(button)
	{
	case PSBTN_OK: id = IDOK; break;
	case PSBTN_CANCEL: id = IDCANCEL; break;
	case PSBTN_APPLYNOW: id = IDC_APPLY_BUTTON; break;
	case PSBTN_HELP: id = IDHELP; break;
	default: return false;
	}
	const HWND b = ::GetDlgItem(s.hwnd, id);
	if(b != nullptr && !::IsWindowEnabled(b))
		return false;
	::SendMessage(s.hwnd, WM_COMMAND, MAKEWPARAM(id, BN_CLICKED), reinterpret_cast<LPARAM>(b));
	return true;
}

BOOL IsSheetDialogMessage(Sheet &s, MSG *msg)
{
	if(msg == nullptr)
		return FALSE;
	if(msg->message == WM_KEYDOWN && (::GetKeyState(VK_CONTROL) & 0x8000) && (msg->wParam == VK_TAB || msg->wParam == VK_PRIOR || msg->wParam == VK_NEXT))
	{
		// Ctrl+Tab and Ctrl+Page Down: the next page; with Shift, or Ctrl+Page Up, the previous one.
		const int count = static_cast<int>(s.pages.size());
		if(count > 0)
		{
			const bool back = msg->wParam == VK_PRIOR || (msg->wParam == VK_TAB && (::GetKeyState(VK_SHIFT) & 0x8000));
			const int next = ((s.active < 0 ? 0 : s.active) + (back ? count - 1 : 1)) % count;
			const HWND hwnd = s.hwnd;
			const HWND focus = ::GetFocus();
			if(SetCurSel(s, next) && SheetOf(hwnd) == &s && (focus == nullptr || !::IsWindowVisible(focus)))
				FocusFirst(s.pages[s.active].hwnd);
		}
		return TRUE;
	}
	return ::IsDialogMessage(s.hwnd, msg);
}

INT_PTR Result(HWND hwnd, LRESULT value)
{
	::SetWindowLongPtr(hwnd, DWLP_MSGRESULT, value);
	return TRUE;
}

INT_PTR CALLBACK SheetProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
	if(msg == WM_INITDIALOG)
	{
		auto *s = reinterpret_cast<Sheet *>(lp);
		Sheets()[hwnd] = s;
		s->hwnd = hwnd;
		s->tab = ::GetDlgItem(hwnd, IDC_TABCONTROL);
		for(std::size_t i = 0; i < s->pages.size(); i++)
			InsertTab(*s, static_cast<int>(i));
		if(HWND apply = ::GetDlgItem(hwnd, IDC_APPLY_BUTTON))
			::EnableWindow(apply, FALSE);
		Layout(*s);
		int start = 0;
		if(s->header.dwFlags & PSH_USEPSTARTPAGE)
		{
			const std::wstring name = ResourceString(s->header.hInstance, s->header.pStartPage);
			for(std::size_t i = 0; i < s->pages.size(); i++)
			{
				if(s->pages[i].title == name)
					start = static_cast<int>(i);
			}
		} else if(s->header.nStartPage < s->pages.size())
		{
			start = static_cast<int>(s->header.nStartPage);
		}
		if(!SetCurSel(*s, start) && SheetOf(hwnd) == s)
			SetCurSel(*s, 0);
		if(SheetOf(hwnd) != s)
			return FALSE;
		if((s->header.dwFlags & PSH_USECALLBACK) && s->header.pfnCallback != nullptr)
			s->header.pfnCallback(hwnd, PSCB_INITIALIZED, 0);
		if((s = SheetOf(hwnd)) != nullptr && s->active >= 0)
			FocusFirst(s->pages[s->active].hwnd);
		return FALSE; // the focus is set
	}
	Sheet *s = SheetOf(hwnd);
	if(s == nullptr)
		return FALSE;
	switch(msg)
	{
	case WM_COMMAND:
		switch(LOWORD(wp))
		{
		case IDOK:
			if(Apply(*s, true) && SheetOf(hwnd) == s)
				EndSheet(*s);
			return TRUE;
		case IDCANCEL:
			Cancel(*s);
			return TRUE;
		case IDC_APPLY_BUTTON:
			Apply(*s, false);
			return TRUE;
		case IDHELP:
			if(s->active >= 0)
				NotifyPage(*s, s->pages[s->active].hwnd, PSN_HELP);
			return TRUE;
		default:
			return FALSE;
		}
	case WM_NOTIFY:
		{
			const auto *hdr = reinterpret_cast<const NMHDR *>(lp);
			if(hdr->hwndFrom != s->tab)
				return FALSE;
			if(hdr->code == TCN_SELCHANGING)
			{
				const bool refuse = s->active >= 0 && NotifyPage(*s, s->pages[s->active].hwnd, PSN_KILLACTIVE) != 0;
				return Result(hwnd, refuse ? TRUE : FALSE);
			}
			if(hdr->code == TCN_SELCHANGE)
			{
				const int index = static_cast<int>(::SendMessage(s->tab, TCM_GETCURSEL, 0, 0));
				if(!SetCurSel(*s, index, true) && (s = SheetOf(hwnd)) != nullptr && s->active >= 0)
					::SendMessage(s->tab, TCM_SETCURSEL, static_cast<WPARAM>(s->active), 0);
				return TRUE;
			}
			return FALSE;
		}
	case WM_SYSCOMMAND:
		if((wp & 0xFFF0) == SC_CLOSE)
		{
			Cancel(*s);
			return TRUE;
		}
		return FALSE;
	case WM_DESTROY:
		{
			Sheets().erase(hwnd);
			for(Page &page : s->pages)
				::DestroyPropertySheetPage(page.hpage);
			s->pages.clear();
			if(s->modeless)
			{
				Live().erase(s);
				delete s;
			}
			return FALSE;
		}
	case PSM_SETCURSEL:
		{
			int index = static_cast<int>(wp);
			if(lp != 0)
				index = IndexOfPage(*s, reinterpret_cast<HPROPSHEETPAGE>(lp));
			return Result(hwnd, SetCurSel(*s, index) ? TRUE : FALSE);
		}
	case PSM_SETCURSELID:
		return Result(hwnd, SetCurSel(*s, IndexOfId(*s, lp)) ? TRUE : FALSE);
	case PSM_REMOVEPAGE:
		{
			int index = static_cast<int>(wp);
			if(lp != 0)
				index = IndexOfPage(*s, reinterpret_cast<HPROPSHEETPAGE>(lp));
			RemovePage(*s, index);
			return Result(hwnd, 0);
		}
	case PSM_ADDPAGE:
	case PSM_INSERTPAGE:
		{
			if(lp == 0)
				return Result(hwnd, FALSE);
			int index = static_cast<int>(s->pages.size());
			if(msg == PSM_INSERTPAGE)
			{
				// wp is an index, or the page to insert after.
				if(wp < 0x10000)
					index = static_cast<int>(wp);
				else
					index = IndexOfPage(*s, reinterpret_cast<HPROPSHEETPAGE>(wp)) + 1;
			}
			AddPage(*s, index, reinterpret_cast<HPROPSHEETPAGE>(lp));
			return Result(hwnd, TRUE);
		}
	case PSM_CHANGED:
		s->changed.insert(reinterpret_cast<HWND>(wp));
		if(HWND apply = ::GetDlgItem(hwnd, IDC_APPLY_BUTTON))
			::EnableWindow(apply, TRUE);
		return Result(hwnd, 0);
	case PSM_UNCHANGED:
		s->changed.erase(reinterpret_cast<HWND>(wp));
		if(s->changed.empty())
		{
			if(HWND apply = ::GetDlgItem(hwnd, IDC_APPLY_BUTTON))
				::EnableWindow(apply, FALSE);
		}
		return Result(hwnd, 0);
	case PSM_RESTARTWINDOWS:
		s->result |= ID_PSRESTARTWINDOWS;
		return Result(hwnd, 0);
	case PSM_REBOOTSYSTEM:
		s->result |= ID_PSREBOOTSYSTEM;
		return Result(hwnd, 0);
	case PSM_CANCELTOCLOSE:
		if(HWND cancel = ::GetDlgItem(hwnd, IDCANCEL))
			::SetWindowText(cancel, L"Close");
		if(HWND apply = ::GetDlgItem(hwnd, IDC_APPLY_BUTTON))
			::EnableWindow(apply, FALSE);
		return Result(hwnd, 0);
	case PSM_QUERYSIBLINGS:
		for(std::size_t i = 0; i < s->pages.size(); i++)
		{
			if(s->pages[i].hwnd == nullptr)
				continue;
			const HWND page = s->pages[i].hwnd;
			const LRESULT r = ::SendMessage(page, PSM_QUERYSIBLINGS, wp, lp);
			if((s = SheetOf(hwnd)) == nullptr)
				return TRUE;
			if(r != 0)
				return Result(hwnd, r);
		}
		return Result(hwnd, 0);
	case PSM_APPLY:
		return Result(hwnd, Apply(*s, false) ? TRUE : FALSE);
	case PSM_SETTITLEW:
		SetTitle(*s, static_cast<DWORD>(wp), reinterpret_cast<LPCWSTR>(lp));
		return Result(hwnd, 0);
	case PSM_PRESSBUTTON:
		return Result(hwnd, PressButton(*s, static_cast<int>(wp)) ? TRUE : FALSE);
	case PSM_GETTABCONTROL:
		return Result(hwnd, reinterpret_cast<LRESULT>(s->tab));
	case PSM_ISDIALOGMESSAGE:
		return Result(hwnd, IsSheetDialogMessage(*s, reinterpret_cast<MSG *>(lp)));
	case PSM_GETCURRENTPAGEHWND:
		return Result(hwnd, s->activeValid && s->active >= 0 ? reinterpret_cast<LRESULT>(s->pages[s->active].hwnd) : 0);
	case PSM_HWNDTOINDEX:
		return Result(hwnd, IndexOfHwnd(*s, reinterpret_cast<HWND>(wp)));
	case PSM_INDEXTOHWND:
		return Result(hwnd, wp < s->pages.size() ? reinterpret_cast<LRESULT>(s->pages[wp].hwnd) : 0);
	case PSM_PAGETOINDEX:
		return Result(hwnd, IndexOfPage(*s, reinterpret_cast<HPROPSHEETPAGE>(lp)));
	case PSM_INDEXTOPAGE:
		return Result(hwnd, wp < s->pages.size() ? reinterpret_cast<LRESULT>(s->pages[wp].hpage) : 0);
	case PSM_IDTOINDEX:
		return Result(hwnd, IndexOfId(*s, lp));
	case PSM_INDEXTOID:
		if(wp < s->pages.size())
		{
			const PROPSHEETPAGEW &psp = s->pages[wp].hpage->psp;
			return Result(hwnd, (psp.dwFlags & PSP_DLGINDIRECT) ? reinterpret_cast<LRESULT>(psp.pResource) : reinterpret_cast<LRESULT>(psp.pszTemplate));
		}
		return Result(hwnd, 0);
	case PSM_GETRESULT:
		return Result(hwnd, s->ended ? s->result : -1);
	case PSM_RECALCPAGESIZES:
		for(Page &page : s->pages)
			ReadPageInfo(page);
		Layout(*s);
		return Result(hwnd, TRUE);
	case PSM_SETWIZBUTTONS:
	case PSM_SETFINISHTEXTW:
	case PSM_SETHEADERTITLEW:
	case PSM_SETHEADERSUBTITLEW:
		return Result(hwnd, 0);
	default:
		return FALSE;
	}
}

} // namespace

bool PropertySheetDialogMessage(HWND dlg, MSG &msg)
{
	Sheet *s = SheetOf(dlg);
	if(s == nullptr)
		return false;
	return IsSheetDialogMessage(*s, &msg) != FALSE;
}

} // namespace linopenmpt::qtwin

using namespace linopenmpt::qtwin;

HPROPSHEETPAGE CreatePropertySheetPage(LPCPROPSHEETPAGEW page)
{
	if(page == nullptr || page->dwSize < PROPSHEETPAGE_V1_SIZE)
		return nullptr;
	auto *p = new _PSP;
	std::memcpy(&p->psp, page, std::min<std::size_t>(page->dwSize, sizeof(PROPSHEETPAGEW)));
	p->psp.dwSize = sizeof(PROPSHEETPAGEW);
	if(page->dwSize < sizeof(PROPSHEETPAGEW))
	{
		// Older structures end earlier.
		BYTE *rest = reinterpret_cast<BYTE *>(&p->psp) + page->dwSize;
		std::memset(rest, 0, sizeof(PROPSHEETPAGEW) - page->dwSize);
	}
	if(!(p->psp.dwFlags & PSP_DLGINDIRECT) && p->psp.pszTemplate != nullptr && !IS_INTRESOURCE(p->psp.pszTemplate))
	{
		p->templateName = p->psp.pszTemplate;
		p->psp.pszTemplate = p->templateName.c_str();
	}
	if((p->psp.dwFlags & PSP_USETITLE) && p->psp.pszTitle != nullptr && !IS_INTRESOURCE(p->psp.pszTitle))
	{
		p->title = p->psp.pszTitle;
		p->psp.pszTitle = p->title.c_str();
	}
	if((p->psp.dwFlags & PSP_USEREFPARENT) && p->psp.pcRefParent != nullptr)
		(*p->psp.pcRefParent)++;
	if((p->psp.dwFlags & PSP_USECALLBACK) && p->psp.pfnCallback != nullptr)
		p->psp.pfnCallback(nullptr, PSPCB_ADDREF, &p->psp);
	return p;
}

BOOL DestroyPropertySheetPage(HPROPSHEETPAGE page)
{
	if(page == nullptr)
		return FALSE;
	if((page->psp.dwFlags & PSP_USECALLBACK) && page->psp.pfnCallback != nullptr)
		page->psp.pfnCallback(nullptr, PSPCB_RELEASE, &page->psp);
	if((page->psp.dwFlags & PSP_USEREFPARENT) && page->psp.pcRefParent != nullptr)
		(*page->psp.pcRefParent)--;
	delete page;
	return TRUE;
}

INT_PTR PropertySheet(LPCPROPSHEETHEADERW header)
{
	if(header == nullptr || header->dwSize < PROPSHEETHEADER_V1_SIZE)
		return -1;
	if(header->dwFlags & (PSH_WIZARD | PSH_WIZARD97 | PSH_AEROWIZARD | PSH_WIZARD_LITE))
	{
		std::fprintf(stderr, "linOpenMPT: wizard property sheets are not supported\n");
		return -1;
	}
	const bool modeless = (header->dwFlags & PSH_MODELESS) != 0;
	auto *s = new Sheet;
	Live().insert(s);
	std::memcpy(&s->header, header, std::min<std::size_t>(header->dwSize, sizeof(PROPSHEETHEADERW)));
	s->modeless = modeless;
	// The pages: structures one after another (each with its own size), or handles.
	const BYTE *next = reinterpret_cast<const BYTE *>(header->ppsp);
	for(UINT i = 0; i < header->nPages; i++)
	{
		HPROPSHEETPAGE hpage = nullptr;
		if(header->dwFlags & PSH_PROPSHEETPAGE)
		{
			const auto *psp = reinterpret_cast<const PROPSHEETPAGEW *>(next);
			hpage = ::CreatePropertySheetPage(psp);
			next += psp->dwSize;
		} else
		{
			hpage = header->phpage[i];
		}
		if(hpage == nullptr)
			continue;
		Page page;
		page.hpage = hpage;
		ReadPageInfo(page);
		s->pages.push_back(std::move(page));
	}
	// The sheet's font: the first page's, so their dialog units agree.
	short cx = 0, cy = 0;
	for(const Page &page : s->pages)
	{
		cx = std::max(cx, page.cx);
		cy = std::max(cy, page.cy);
	}
	if(!s->pages.empty())
	{
		if(LPCDLGTEMPLATE t = PageTemplate(s->pages.front().hpage->psp))
		{
			const TemplateInfo info = ReadTemplate(t);
			if(info.hasFont && info.pointSize != 0x7FFF)
			{
				s->fontFace = info.face;
				s->fontSize = info.pointSize;
			}
		}
	}

	std::wstring caption = ResourceString(header->hInstance, header->pszCaption);
	if(header->dwFlags & PSH_PROPTITLE)
		caption += L" Properties";
	DWORD style = DS_MODALFRAME | DS_SETFONT | DS_3DLOOK | DS_CENTER | DS_CONTEXTHELP | WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN;
	if(header->dwFlags & PSH_NOCONTEXTHELP)
		style &= ~DS_CONTEXTHELP;
	if(modeless)
		style |= WS_VISIBLE;
	// Sizes in dialog units, near enough: WM_INITDIALOG lays it out exactly.
	const short tabCx = static_cast<short>(cx + 4), tabCy = static_cast<short>(cy + 16);
	const short buttonsY = static_cast<short>(MarginDlu + tabCy + MarginDlu);
	const short width = static_cast<short>(MarginDlu * 2 + tabCx), height = static_cast<short>(buttonsY + ButtonHeightDlu + MarginDlu);
	TemplateWriter w;
	w.Header(style, 5, width, height, caption, s->fontSize, s->fontFace);
	w.Item(WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_CLIPSIBLINGS | WS_GROUP, MarginDlu, MarginDlu, tabCx, tabCy, IDC_TABCONTROL, WC_TABCONTROL, 0, L"");
	auto buttonX = [&](int i) { return static_cast<short>(width - MarginDlu - (4 - i) * (ButtonWidthDlu + ButtonGapDlu) + ButtonGapDlu); };
	w.Item(WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_GROUP | BS_DEFPUSHBUTTON, buttonX(0), buttonsY, ButtonWidthDlu, ButtonHeightDlu, IDOK, {}, 0x0080, L"OK");
	w.Item(WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, buttonX(1), buttonsY, ButtonWidthDlu, ButtonHeightDlu, IDCANCEL, {}, 0x0080, L"Cancel");
	w.Item(WS_CHILD | ((header->dwFlags & PSH_NOAPPLYNOW) ? 0 : WS_VISIBLE) | WS_TABSTOP | BS_PUSHBUTTON, buttonX(2), buttonsY, ButtonWidthDlu, ButtonHeightDlu, IDC_APPLY_BUTTON, {}, 0x0080, L"&Apply");
	w.Item(WS_CHILD | ((header->dwFlags & PSH_HASHELP) ? WS_VISIBLE : 0) | WS_TABSTOP | BS_PUSHBUTTON, buttonX(3), buttonsY, ButtonWidthDlu, ButtonHeightDlu, IDHELP, {}, 0x0080, L"Help");
	LPDLGTEMPLATE dlgTemplate = w.Get();
	if((header->dwFlags & PSH_USECALLBACK) && header->pfnCallback != nullptr)
		header->pfnCallback(nullptr, PSCB_PRECREATE, reinterpret_cast<LPARAM>(dlgTemplate));

	if(modeless)
	{
		const HWND hwnd = ::CreateDialogIndirectParam(header->hInstance, dlgTemplate, header->hwndParent, SheetProc, reinterpret_cast<LPARAM>(s));
		if(hwnd == nullptr)
		{
			if(Live().count(s) != 0)
			{
				for(Page &page : s->pages)
					::DestroyPropertySheetPage(page.hpage);
				Live().erase(s);
				delete s;
			}
			return -1;
		}
		return reinterpret_cast<INT_PTR>(hwnd);
	}
	const INT_PTR result = ::DialogBoxIndirectParam(header->hInstance, dlgTemplate, header->hwndParent, SheetProc, reinterpret_cast<LPARAM>(s));
	const INT_PTR sheetResult = s->result;
	// WM_DESTROY released the pages; if the dialog never got that far, do it here.
	for(Page &page : s->pages)
		::DestroyPropertySheetPage(page.hpage);
	Live().erase(s);
	delete s;
	return result < 0 ? -1 : sheetResult;
}
