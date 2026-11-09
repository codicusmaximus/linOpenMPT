// The dialog manager: dialogs created from DIALOG and DIALOGEX templates
// (upstream's mptrack.rc), dialog units, the dialog window class and
// DefDlgProc, modal dialogs, and keyboard navigation (IsDialogMessage).
//
// It follows Windows' dialog manager: a dialog is an ordinary window of the
// "#32770" class whose window procedure calls the dialog procedure first;
// controls are child windows created from the template in tab order.
#include "QtWin.h"

#include <QtGui/QFontInfo>
#include <QtGui/QGuiApplication>
#include <QtGui/QScreen>
#include <QtWidgets/QApplication>

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace linopenmpt::qtwin
{

namespace
{

// ---- templates -----------------------------------------------------------------------

// A name or an ordinal (sz_Or_Ord in a template).
struct NameOrOrd
{
	std::wstring name;
	WORD ordinal = 0;
	bool isOrdinal = false;
	bool empty() const { return !isOrdinal && name.empty(); }
};

struct ItemTemplate
{
	DWORD style = 0, exStyle = 0, helpId = 0;
	short x = 0, y = 0, cx = 0, cy = 0;
	DWORD id = 0;
	NameOrOrd cls, title;
	const BYTE *data = nullptr; // creation data, starting with its size (WORD)
	WORD dataSize = 0;
};

struct DialogTemplate
{
	bool ex = false;
	DWORD style = 0, exStyle = 0, helpId = 0;
	short x = 0, y = 0, cx = 0, cy = 0;
	NameOrOrd menu, cls;
	std::wstring title;
	WORD pointSize = 0;
	WORD weight = FW_NORMAL;
	BYTE italic = 0, charset = DEFAULT_CHARSET;
	std::wstring face;
	std::vector<ItemTemplate> items;
};

class Reader
{
public:
	explicit Reader(const BYTE *p) : base(p), p(p) { }
	WORD Word() { WORD v; std::memcpy(&v, p, 2); p += 2; return v; }
	DWORD Dword() { DWORD v; std::memcpy(&v, p, 4); p += 4; return v; }
	short Short() { return static_cast<short>(Word()); }
	BYTE Byte() { return *p++; }
	// Template strings are UTF-16, whatever wchar_t is.
	std::wstring String()
	{
		std::u16string s;
		for(WORD c; (c = Word()) != 0;)
			s.push_back(static_cast<char16_t>(c));
		return QString::fromUtf16(s.data(), static_cast<qsizetype>(s.size())).toStdWString();
	}
	NameOrOrd NameOrOrdinal()
	{
		NameOrOrd r;
		WORD first;
		std::memcpy(&first, p, 2);
		if(first == 0)
		{
			p += 2;
		} else if(first == 0xFFFF)
		{
			p += 2;
			r.ordinal = Word();
			r.isOrdinal = true;
		} else
		{
			r.name = String();
		}
		return r;
	}
	void AlignDword()
	{
		const auto offset = static_cast<std::size_t>(p - base);
		p = base + ((offset + 3) & ~std::size_t(3));
	}
	const BYTE *Pos() const { return p; }
	void Skip(std::size_t n) { p += n; }

private:
	const BYTE *base;
	const BYTE *p;
};

DialogTemplate ParseTemplate(LPCDLGTEMPLATE dlgTemplate)
{
	DialogTemplate t;
	Reader r(reinterpret_cast<const BYTE *>(dlgTemplate));
	WORD cdit = 0;
	{
		Reader peek = r;
		const WORD version = peek.Word();
		const WORD signature = peek.Word();
		t.ex = version == 1 && signature == 0xFFFF;
	}
	if(t.ex)
	{
		r.Skip(4);
		t.helpId = r.Dword();
		t.exStyle = r.Dword();
		t.style = r.Dword();
	} else
	{
		t.style = r.Dword();
		t.exStyle = r.Dword();
	}
	cdit = r.Word();
	t.x = r.Short();
	t.y = r.Short();
	t.cx = r.Short();
	t.cy = r.Short();
	t.menu = r.NameOrOrdinal();
	t.cls = r.NameOrOrdinal();
	t.title = r.String();
	if(t.style & DS_SETFONT)
	{
		t.pointSize = r.Word();
		if(t.ex)
		{
			t.weight = r.Word();
			t.italic = r.Byte();
			t.charset = r.Byte();
		}
		t.face = r.String();
	}
	for(WORD i = 0; i < cdit; i++)
	{
		r.AlignDword();
		ItemTemplate item;
		if(t.ex)
		{
			item.helpId = r.Dword();
			item.exStyle = r.Dword();
			item.style = r.Dword();
		} else
		{
			item.style = r.Dword();
			item.exStyle = r.Dword();
		}
		item.x = r.Short();
		item.y = r.Short();
		item.cx = r.Short();
		item.cy = r.Short();
		item.id = t.ex ? r.Dword() : r.Word();
		item.cls = r.NameOrOrdinal();
		item.title = r.NameOrOrdinal();
		item.dataSize = r.Word();
		if(item.dataSize > 0)
		{
			// The control gets a pointer to the size, then the data.
			item.data = r.Pos() - 2;
			r.Skip(item.dataSize);
		}
		t.items.push_back(std::move(item));
	}
	return t;
}

std::wstring ClassName(const NameOrOrd &cls)
{
	if(!cls.isOrdinal)
		return cls.name;
	switch(cls.ordinal)
	{
	case 0x80: return L"Button";
	case 0x81: return L"Edit";
	case 0x82: return L"Static";
	case 0x83: return L"ListBox";
	case 0x84: return L"ScrollBar";
	case 0x85: return L"ComboBox";
	default: return std::wstring();
	}
}

// ---- dialog state ----------------------------------------------------------------

struct DialogInfo
{
	HFONT font = nullptr;
	bool ownsFont = false;
	int baseX = 0, baseY = 0;
	bool modal = false;
	bool ended = false;
	INT_PTR result = 0;
	HWND owner = nullptr;
	HWND savedFocus = nullptr; // restored when the dialog is activated again
	UINT defId = IDOK;
	bool hasDefId = false;
};

std::map<HWND, DialogInfo> &Dialogs()
{
	static std::map<HWND, DialogInfo> dialogs;
	return dialogs;
}

DialogInfo *Info(HWND dlg)
{
	auto it = Dialogs().find(dlg);
	return it != Dialogs().end() ? &it->second : nullptr;
}

// Modal dialogs from DialogBox(), innermost last.
std::vector<HWND> g_modal;

// Dialog fonts: "MS Shell Dlg" at 8 points is the UI font. Its size follows
// the desktop's, as Windows' dialog font follows its UI font (9 point Segoe
// UI, 12 pixels at 96 dpi), so dialogs keep their proportions.
LONG DialogFontPixels(int points)
{
	const int ui = QFontInfo(QApplication::font()).pixelSize();
	const double windowsPixels = points * 96.0 / 72.0;
	return std::max(1L, std::lround(windowsPixels * ui / 12.0));
}

void ComputeBaseUnits(HFONT font, int &baseX, int &baseY)
{
	HDC dc = ::CreateCompatibleDC(nullptr);
	HGDIOBJ old = ::SelectObject(dc, font != nullptr ? font : ::GetStockObject(DEFAULT_GUI_FONT));
	SIZE size{};
	static const wchar_t alphabet[] = L"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
	::GetTextExtentPoint32(dc, alphabet, 52, &size);
	TEXTMETRIC tm{};
	::GetTextMetrics(dc, &tm);
	::SelectObject(dc, old);
	::DeleteDC(dc);
	// As Windows computes them (GdiGetCharDimensions).
	baseX = std::max(1, static_cast<int>((size.cx / 26 + 1) / 2));
	baseY = std::max(1, static_cast<int>(tm.tmHeight));
}

RECT DialogUnitsToPixels(int baseX, int baseY, int x, int y, int cx, int cy)
{
	RECT r;
	r.left = ::MulDiv(x, baseX, 4);
	r.top = ::MulDiv(y, baseY, 8);
	r.right = r.left + ::MulDiv(cx, baseX, 4);
	r.bottom = r.top + ::MulDiv(cy, baseY, 8);
	return r;
}

bool IsDialog(HWND hwnd)
{
	return Info(hwnd) != nullptr;
}

DWORD DlgCode(HWND ctl, MSG *msg = nullptr)
{
	return ctl ? static_cast<DWORD>(::SendMessage(ctl, WM_GETDLGCODE, msg ? msg->wParam : 0, reinterpret_cast<LPARAM>(msg))) : 0;
}

bool IsTabStop(HWND hwnd)
{
	const LONG_PTR style = ::GetWindowLongPtr(hwnd, GWL_STYLE);
	return (style & WS_TABSTOP) && (style & WS_VISIBLE) && !(style & WS_DISABLED);
}

bool IsControlParent(HWND hwnd)
{
	return (::GetWindowLongPtr(hwnd, GWL_EXSTYLE) & WS_EX_CONTROLPARENT) != 0
		&& (::GetWindowLongPtr(hwnd, GWL_STYLE) & (WS_VISIBLE | WS_DISABLED)) == WS_VISIBLE;
}

// The controls of a dialog in tab order, with the controls of nested
// dialogs (WS_EX_CONTROLPARENT) in place of the nested dialog.
void CollectControls(HWND parent, std::vector<HWND> &out)
{
	for(HWND c = ::GetWindow(parent, GW_CHILD); c != nullptr; c = ::GetWindow(c, GW_HWNDNEXT))
	{
		if(IsControlParent(c))
			CollectControls(c, out);
		else
			out.push_back(c);
	}
}

// Nested dialogs (DS_CONTROL) navigate as part of the dialog they are in.
HWND RootDialog(HWND dlg)
{
	while(dlg != nullptr && (::GetWindowLongPtr(dlg, GWL_STYLE) & WS_CHILD) && (::GetWindowLongPtr(dlg, GWL_EXSTYLE) & WS_EX_CONTROLPARENT))
	{
		const HWND parent = ::GetParent(dlg);
		if(parent == nullptr || !(IsDialog(parent) || (::GetWindowLongPtr(parent, GWL_EXSTYLE) & WS_EX_CONTROLPARENT)))
			break;
		dlg = parent;
	}
	return dlg;
}

// The control of dlg that ctl is, or is inside.
HWND DirectControl(HWND dlg, HWND ctl)
{
	while(ctl != nullptr && ::GetParent(ctl) != nullptr && ::GetParent(ctl) != dlg && ::IsChild(dlg, ctl))
	{
		const HWND parent = ::GetParent(ctl);
		if(IsControlParent(parent))
			break;
		ctl = parent;
	}
	return ctl;
}

// Focus to a control as the dialog manager does it: edits select all.
void DlgSetFocus(HWND ctl)
{
	if(DlgCode(ctl) & DLGC_HASSETSEL)
		::SendMessage(ctl, EM_SETSEL, 0, -1);
	::SetFocus(ctl);
}

// The window whose text holds a mnemonic for ch, and which control it moves to.
HWND FindMnemonic(HWND dlg, HWND from, WCHAR ch)
{
	std::vector<HWND> controls;
	CollectControls(dlg, controls);
	if(controls.empty())
		return nullptr;
	const QChar wanted = QChar(static_cast<char16_t>(ch)).toUpper();
	auto start = std::find(controls.begin(), controls.end(), from);
	const std::size_t first = start == controls.end() ? 0 : static_cast<std::size_t>(start - controls.begin()) + 1;
	for(std::size_t n = 0; n < controls.size(); n++)
	{
		const HWND c = controls[(first + n) % controls.size()];
		const LONG_PTR style = ::GetWindowLongPtr(c, GWL_STYLE);
		if(!(style & WS_VISIBLE) || (style & WS_DISABLED))
			continue;
		const DWORD code = DlgCode(c);
		if(!(code & (DLGC_BUTTON | DLGC_STATIC | DLGC_DEFPUSHBUTTON | DLGC_UNDEFPUSHBUTTON | DLGC_RADIOBUTTON)))
			continue;
		if((code & DLGC_STATIC) && (style & SS_NOPREFIX))
			continue;
		wchar_t text[256] = {};
		::GetWindowText(c, text, 256);
		for(const wchar_t *p = text; *p; p++)
		{
			if(*p == L'&')
			{
				if(p[1] == L'&')
				{
					p++;
					continue;
				}
				if(p[1] != 0 && QChar(static_cast<char16_t>(p[1])).toUpper() == wanted)
					return c;
				break;
			}
		}
	}
	return nullptr;
}

void SendCommand(HWND dlg, UINT id, HWND ctl, UINT code = BN_CLICKED)
{
	::SendMessage(dlg, WM_COMMAND, MAKEWPARAM(id, code), reinterpret_cast<LPARAM>(ctl));
}

// The default push button gets the BS_DEFPUSHBUTTON look, others lose it.
void SetDefaultButton(HWND dlg, HWND newDefault)
{
	std::vector<HWND> controls;
	CollectControls(dlg, controls);
	for(HWND c : controls)
	{
		const DWORD code = DlgCode(c);
		if((code & DLGC_DEFPUSHBUTTON) && c != newDefault)
			::SendMessage(c, BM_SETSTYLE, BS_PUSHBUTTON, TRUE);
		else if((code & DLGC_UNDEFPUSHBUTTON) && c == newDefault)
			::SendMessage(c, BM_SETSTYLE, BS_DEFPUSHBUTTON, TRUE);
	}
}

HWND DefaultButton(HWND dlg)
{
	const LRESULT def = ::SendMessage(dlg, DM_GETDEFID, 0, 0);
	if(HIWORD(def) != DC_HASDEFID)
		return nullptr;
	std::vector<HWND> controls;
	CollectControls(dlg, controls);
	for(HWND c : controls)
	{
		if(::GetDlgCtrlID(c) == LOWORD(def))
			return c;
	}
	return nullptr;
}

// Focus moved to ctl: a push button becomes the default while it has focus.
void FocusChanged(HWND dlg, HWND ctl)
{
	const DWORD code = DlgCode(ctl);
	if(code & (DLGC_DEFPUSHBUTTON | DLGC_UNDEFPUSHBUTTON))
		SetDefaultButton(dlg, ctl);
	else
		SetDefaultButton(dlg, DefaultButton(dlg));
}

LRESULT CALLBACK DialogClassProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
	return ::DefDlgProc(hwnd, msg, wp, lp);
}

void RegisterDialogClass()
{
	static bool registered = false;
	if(registered)
		return;
	registered = true;
	WNDCLASS wc{};
	wc.style = CS_DBLCLKS | CS_SAVEBITS;
	wc.lpfnWndProc = DialogClassProc;
	wc.cbWndExtra = DLGWINDOWEXTRA;
	wc.hCursor = ::LoadCursor(nullptr, IDC_ARROW);
	wc.lpszClassName = L"#32770";
	::RegisterClass(&wc);
}

BOOL ModalIdle(LONG count)
{
	if(g_modal.empty())
		return FALSE;
	const HWND dlg = g_modal.back();
	const DialogInfo *info = Info(dlg);
	if(info != nullptr && info->owner != nullptr && count == 0)
		::SendMessage(info->owner, WM_ENTERIDLE, MSGF_DIALOGBOX, reinterpret_cast<LPARAM>(dlg));
	return FALSE;
}

HWND CreateDialogInternal(HINSTANCE instance, LPCDLGTEMPLATE dlgTemplate, HWND parent, DLGPROC proc, LPARAM param, bool modal)
{
	if(dlgTemplate == nullptr)
		return nullptr;
	RegisterDialogClass();
	const DialogTemplate t = ParseTemplate(dlgTemplate);

	// The font, and the dialog units it gives.
	HFONT font = nullptr;
	bool ownsFont = false;
	if(t.style & DS_SETFONT)
	{
		if(t.pointSize == 0x7FFF)
		{
			font = static_cast<HFONT>(::GetStockObject(DEFAULT_GUI_FONT));
		} else
		{
			LOGFONT lf{};
			lf.lfHeight = -DialogFontPixels(t.pointSize);
			lf.lfWeight = t.weight;
			lf.lfItalic = t.italic;
			lf.lfCharSet = t.charset;
			std::wcsncpy(lf.lfFaceName, t.face.c_str(), LF_FACESIZE - 1);
			font = ::CreateFontIndirect(&lf);
			ownsFont = true;
		}
	}
	int baseX = 0, baseY = 0;
	ComputeBaseUnits(font, baseX, baseY);

	DWORD style = t.style & ~WS_VISIBLE;
	DWORD exStyle = t.exStyle;
	if(style & DS_MODALFRAME)
		exStyle |= WS_EX_DLGMODALFRAME;
	if(style & DS_CONTROL)
	{
		style &= ~(WS_CAPTION | WS_SYSMENU);
		exStyle |= WS_EX_CONTROLPARENT;
	}
	const bool child = (style & WS_CHILD) != 0;
	HWND owner = parent;
	if(!child && owner != nullptr)
		owner = ::GetAncestor(owner, GA_ROOT);

	RECT rect = DialogUnitsToPixels(baseX, baseY, t.x, t.y, t.cx, t.cy);
	const RECT client = rect;
	HMENU menu = nullptr;
	if(!t.menu.empty() && !child)
		menu = ::LoadMenu(instance, t.menu.isOrdinal ? MAKEINTRESOURCE(t.menu.ordinal) : t.menu.name.c_str());
	::AdjustWindowRectEx(&rect, style, menu != nullptr, exStyle);
	const int width = rect.right - rect.left, height = rect.bottom - rect.top;
	int x = rect.left, y = rect.top;
	if(!child)
	{
		// Relative to the owner's client area unless DS_ABSALIGN; centred
		// with DS_CENTER. Kept on the screen.
		QRect area = QGuiApplication::primaryScreen() ? QGuiApplication::primaryScreen()->availableGeometry() : QRect(0, 0, 1024, 768);
		Win32Widget *ownerWidget = FromHandle(owner);
		if(ownerWidget != nullptr && ownerWidget->screen() != nullptr)
			area = ownerWidget->screen()->availableGeometry();
		if(style & (DS_CENTER | DS_CENTERMOUSE))
		{
			QRect around = area;
			if(style & DS_CENTERMOUSE)
				around = QRect(QCursor::pos(), QSize(0, 0));
			else if(ownerWidget != nullptr && ownerWidget->isVisible() && !ownerWidget->isMinimized())
				around = ownerWidget->frameGeometry();
			x = around.center().x() - width / 2;
			y = around.center().y() - height / 2;
		} else if(!(style & DS_ABSALIGN) && owner != nullptr)
		{
			POINT origin{0, 0};
			::ClientToScreen(owner, &origin);
			x += origin.x;
			y += origin.y;
		}
		x = std::clamp(x, area.left(), std::max(area.left(), area.right() + 1 - width));
		y = std::clamp(y, area.top(), std::max(area.top(), area.bottom() + 1 - height));
	}

	const std::wstring className = t.cls.empty() ? std::wstring(L"#32770") : ClassName(t.cls);
	const HWND dlg = ::CreateWindowEx(exStyle, className.c_str(), t.title.c_str(), style, x, y, width, height, child ? parent : owner, child ? nullptr : menu, instance, reinterpret_cast<LPVOID>(param));
	if(dlg == nullptr)
	{
		if(ownsFont)
			::DeleteObject(font);
		if(menu != nullptr)
			::DestroyMenu(menu);
		return nullptr;
	}
	DialogInfo &info = Dialogs()[dlg];
	info.font = font;
	info.ownsFont = ownsFont;
	info.baseX = baseX;
	info.baseY = baseY;
	info.modal = modal;
	info.owner = child ? nullptr : owner;
	::SetWindowLongPtr(dlg, DWLP_DLGPROC, reinterpret_cast<LONG_PTR>(proc));
	if(font != nullptr)
		::SendMessage(dlg, WM_SETFONT, reinterpret_cast<WPARAM>(font), FALSE);

	// The controls, in tab order.
	for(const ItemTemplate &item : t.items)
	{
		const RECT r = DialogUnitsToPixels(baseX, baseY, item.x, item.y, item.cx, item.cy);
		const std::wstring cls = ClassName(item.cls);
		// An ordinal title names a resource (icons and bitmaps of statics),
		// passed as {0xFFFF, ordinal}, as Windows does.
		wchar_t ordinalTitle[3] = {0xFFFF, item.title.ordinal, 0};
		const wchar_t *title = item.title.isOrdinal ? ordinalTitle : item.title.name.c_str();
		const HWND ctl = ::CreateWindowEx(item.exStyle | WS_EX_NOPARENTNOTIFY, cls.c_str(), title, item.style | WS_CHILD, r.left, r.top, r.right - r.left, r.bottom - r.top,
			dlg, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(item.id)), instance, const_cast<BYTE *>(item.data));
		if(ctl == nullptr)
		{
			std::fprintf(stderr, "linOpenMPT: dialog control of class \"%ls\" (id %u) could not be created\n", cls.c_str(), static_cast<unsigned>(item.id));
			if(!(t.style & DS_NOFAILCREATE))
			{
				::DestroyWindow(dlg);
				return nullptr;
			}
			continue;
		}
		if(font != nullptr)
			::SendMessage(ctl, WM_SETFONT, reinterpret_cast<WPARAM>(font), FALSE);
		if((item.style & BS_TYPEMASK) == BS_DEFPUSHBUTTON && (DlgCode(ctl) & DLGC_DEFPUSHBUTTON) && !info.hasDefId)
		{
			info.defId = static_cast<UINT>(item.id);
			info.hasDefId = true;
		}
	}
	if(!Info(dlg))
		return nullptr;
	(void)client;

	const HWND first = ::GetNextDlgTabItem(dlg, nullptr, FALSE);
	if(::SendMessage(dlg, WM_INITDIALOG, reinterpret_cast<WPARAM>(first), param) && ::IsWindow(dlg))
	{
		const HWND focus = ::GetNextDlgTabItem(dlg, nullptr, FALSE);
		if(focus != nullptr)
		{
			DlgSetFocus(focus);
			FocusChanged(dlg, focus);
		}
	}
	if(!::IsWindow(dlg))
		return nullptr;
	if((t.style & WS_VISIBLE) && !modal)
		::ShowWindow(dlg, SW_SHOWNORMAL);
	return dlg;
}

} // namespace

void DialogFocusChanged(HWND focus)
{
	// The default push button follows the focus, however it moved.
	HWND dlg = ::GetParent(focus);
	while(dlg != nullptr && !IsDialog(dlg))
		dlg = ::GetParent(dlg);
	if(dlg != nullptr)
		FocusChanged(RootDialog(dlg), focus);
}

bool HandleDialogMessage(const MSG &msg)
{
	// Messages in DialogBox()'s loop go through IsDialogMessage(), as there.
	if(g_modal.empty())
		return false;
	const HWND dlg = g_modal.back();
	if(msg.hwnd != dlg && !::IsChild(dlg, msg.hwnd))
		return false;
	MSG m = msg;
	return ::IsDialogMessage(dlg, &m) != FALSE;
}

} // namespace linopenmpt::qtwin

using namespace linopenmpt::qtwin;

// ---- creating dialogs ---------------------------------------------------------

HWND CreateDialogIndirectParam(HINSTANCE instance, LPCDLGTEMPLATE dlgTemplate, HWND parent, DLGPROC proc, LPARAM param)
{
	return CreateDialogInternal(instance, dlgTemplate, parent, proc, param, false);
}

HWND CreateDialogParam(HINSTANCE instance, LPCTSTR templateName, HWND parent, DLGPROC proc, LPARAM param)
{
	HRSRC res = ::FindResource(instance, templateName, RT_DIALOG);
	if(res == nullptr)
		return nullptr;
	return CreateDialogIndirectParam(instance, static_cast<LPCDLGTEMPLATE>(::LockResource(::LoadResource(instance, res))), parent, proc, param);
}

INT_PTR DialogBoxIndirectParam(HINSTANCE instance, LPCDLGTEMPLATE dlgTemplate, HWND parent, DLGPROC proc, LPARAM param)
{
	const HWND owner = parent != nullptr ? ::GetAncestor(parent, GA_ROOT) : nullptr;
	const HWND dlg = CreateDialogInternal(instance, dlgTemplate, owner, proc, param, true);
	if(dlg == nullptr)
		return -1;
	DialogInfo *info = Info(dlg);
	const bool disableOwner = owner != nullptr && ::IsWindowEnabled(owner);
	if(disableOwner)
		::EnableWindow(owner, FALSE);
	if(info != nullptr && !info->ended)
	{
		::ShowWindow(dlg, SW_SHOWNORMAL);
		g_modal.push_back(dlg);
		LinOpenMPTRunModalLoopFor(dlg, ModalIdle);
		g_modal.erase(std::find(g_modal.begin(), g_modal.end(), dlg));
	}
	info = Info(dlg);
	const INT_PTR result = info != nullptr ? info->result : -1;
	if(disableOwner && ::IsWindow(owner))
		::EnableWindow(owner, TRUE);
	if(::IsWindow(dlg))
		::DestroyWindow(dlg);
	if(owner != nullptr && ::IsWindow(owner) && ::IsWindowVisible(owner))
		::SetActiveWindow(owner);
	return result;
}

INT_PTR DialogBoxParam(HINSTANCE instance, LPCTSTR templateName, HWND parent, DLGPROC proc, LPARAM param)
{
	HRSRC res = ::FindResource(instance, templateName, RT_DIALOG);
	if(res == nullptr)
		return -1;
	return DialogBoxIndirectParam(instance, static_cast<LPCDLGTEMPLATE>(::LockResource(::LoadResource(instance, res))), parent, proc, param);
}

BOOL EndDialog(HWND dlg, INT_PTR result)
{
	DialogInfo *info = Info(dlg);
	if(info == nullptr)
		return FALSE;
	info->result = result;
	info->ended = true;
	if(info->modal)
	{
		// The owner comes back first, so that it is what gets activated.
		if(info->owner != nullptr)
			::EnableWindow(info->owner, TRUE);
		::ShowWindow(dlg, SW_HIDE);
		LinOpenMPTEndModalLoopFor(dlg, 0);
	}
	return TRUE;
}

// ---- the dialog procedure's default ---------------------------------------------

LRESULT DefDlgProc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp)
{
	auto proc = reinterpret_cast<DLGPROC>(::GetWindowLongPtr(dlg, DWLP_DLGPROC));
	if(proc != nullptr)
	{
		::SetWindowLongPtr(dlg, DWLP_MSGRESULT, 0);
		const INT_PTR handled = proc(dlg, msg, wp, lp);
		if(!::IsWindow(dlg))
			return 0;
		if(handled)
		{
			// These return the procedure's result itself; everything else
			// returns DWLP_MSGRESULT.
			switch(msg)
			{
			case WM_COMPAREITEM:
			case WM_VKEYTOITEM:
			case WM_CHARTOITEM:
			case WM_QUERYDRAGICON:
			case WM_INITDIALOG:
			case WM_CTLCOLORMSGBOX:
			case WM_CTLCOLOREDIT:
			case WM_CTLCOLORLISTBOX:
			case WM_CTLCOLORBTN:
			case WM_CTLCOLORDLG:
			case WM_CTLCOLORSCROLLBAR:
			case WM_CTLCOLORSTATIC:
				return static_cast<LRESULT>(handled);
			default:
				return ::GetWindowLongPtr(dlg, DWLP_MSGRESULT);
			}
		}
	}
	DialogInfo *info = Info(dlg);
	switch(msg)
	{
	case WM_INITDIALOG:
		return TRUE;
	case WM_ERASEBKGND:
		{
			RECT rc;
			::GetClientRect(dlg, &rc);
			auto brush = reinterpret_cast<HBRUSH>(::SendMessage(dlg, WM_CTLCOLORDLG, wp, reinterpret_cast<LPARAM>(dlg)));
			if(brush == nullptr)
				brush = ::GetSysColorBrush(COLOR_3DFACE);
			::FillRect(reinterpret_cast<HDC>(wp), &rc, brush);
			return 1;
		}
	case WM_CTLCOLORDLG:
	case WM_CTLCOLORSTATIC:
	case WM_CTLCOLORBTN:
		::SetBkColor(reinterpret_cast<HDC>(wp), ::GetSysColor(COLOR_3DFACE));
		::SetTextColor(reinterpret_cast<HDC>(wp), ::GetSysColor(COLOR_WINDOWTEXT));
		return reinterpret_cast<LRESULT>(::GetSysColorBrush(COLOR_3DFACE));
	case WM_SETFONT:
		if(info != nullptr && wp != 0)
			info->font = reinterpret_cast<HFONT>(wp);
		return ::DefWindowProc(dlg, msg, wp, lp);
	case WM_GETFONT:
		return reinterpret_cast<LRESULT>(info != nullptr ? info->font : nullptr);
	case WM_CLOSE:
		{
			// Like pressing Cancel, if there is one to press.
			const HWND cancel = ::GetDlgItem(dlg, IDCANCEL);
			if(cancel == nullptr || ::IsWindowEnabled(cancel))
				SendCommand(dlg, IDCANCEL, cancel);
		}
		return 0;
	case DM_GETDEFID:
		if(info != nullptr && info->hasDefId)
			return MAKELONG(info->defId, DC_HASDEFID);
		return 0;
	case DM_SETDEFID:
		if(info != nullptr)
		{
			info->defId = static_cast<UINT>(wp);
			info->hasDefId = true;
			HWND button = nullptr;
			std::vector<HWND> controls;
			CollectControls(dlg, controls);
			for(HWND c : controls)
			{
				if(::GetDlgCtrlID(c) == static_cast<int>(wp))
					button = c;
			}
			SetDefaultButton(dlg, button);
		}
		return TRUE;
	case DM_REPOSITION:
		return 0;
	case WM_NEXTDLGCTL:
		if(const HWND root = RootDialog(dlg); root != dlg)
			return ::SendMessage(root, msg, wp, lp);
		{
			HWND target;
			if(LOWORD(lp))
				target = reinterpret_cast<HWND>(wp);
			else
				target = ::GetNextDlgTabItem(dlg, ::GetFocus(), wp != 0);
			if(target != nullptr)
			{
				DlgSetFocus(target);
				FocusChanged(dlg, target);
			}
		}
		return 0;
	case WM_ACTIVATE:
		if(info != nullptr)
		{
			if(LOWORD(wp) == WA_INACTIVE)
			{
				const HWND focus = ::GetFocus();
				if(focus != nullptr && ::IsChild(dlg, focus))
					info->savedFocus = focus;
			} else if(info->savedFocus != nullptr && ::IsWindow(info->savedFocus))
			{
				::SetFocus(info->savedFocus);
			}
		}
		return 0;
	case WM_SETFOCUS:
		{
			// The dialog itself does not keep the focus.
			HWND target = info != nullptr && info->savedFocus != nullptr && ::IsWindow(info->savedFocus) ? info->savedFocus : ::GetNextDlgTabItem(dlg, nullptr, FALSE);
			if(target != nullptr)
				DlgSetFocus(target);
		}
		return 0;
	case WM_SHOWWINDOW:
		if(!wp && info != nullptr)
		{
			const HWND focus = ::GetFocus();
			if(focus != nullptr && ::IsChild(dlg, focus))
				info->savedFocus = focus;
		}
		return ::DefWindowProc(dlg, msg, wp, lp);
	case WM_GETDLGCODE:
		return 0;
	case WM_NCDESTROY:
		if(info != nullptr)
		{
			if(info->ownsFont)
				::DeleteObject(info->font);
			Dialogs().erase(dlg);
		}
		return ::DefWindowProc(dlg, msg, wp, lp);
	default:
		return ::DefWindowProc(dlg, msg, wp, lp);
	}
}

// ---- dialog units ---------------------------------------------------------------------

BOOL MapDialogRect(HWND dlg, LPRECT rect)
{
	DialogInfo *info = Info(dlg);
	if(info == nullptr || rect == nullptr)
		return FALSE;
	*rect = RECT{::MulDiv(rect->left, info->baseX, 4), ::MulDiv(rect->top, info->baseY, 8), ::MulDiv(rect->right, info->baseX, 4), ::MulDiv(rect->bottom, info->baseY, 8)};
	return TRUE;
}

LONG GetDialogBaseUnits()
{
	int x = 0, y = 0;
	ComputeBaseUnits(nullptr, x, y);
	return MAKELONG(x, y);
}

// ---- keyboard navigation -----------------------------------------------------------

HWND GetNextDlgTabItem(HWND dlg, HWND ctl, BOOL previous)
{
	std::vector<HWND> controls;
	CollectControls(dlg, controls);
	if(controls.empty())
		return nullptr;
	const std::size_t n = controls.size();
	std::size_t start;
	auto it = std::find(controls.begin(), controls.end(), ctl);
	if(ctl == nullptr || it == controls.end())
	{
		// From the start: the first tab stop (or the last, going back).
		start = previous ? 0 : n - 1;
	} else
	{
		start = static_cast<std::size_t>(it - controls.begin());
	}
	for(std::size_t i = 1; i <= n; i++)
	{
		const std::size_t index = previous ? (start + n - i) % n : (start + i) % n;
		if(IsTabStop(controls[index]))
			return controls[index];
	}
	return ctl;
}

HWND GetNextDlgGroupItem(HWND dlg, HWND ctl, BOOL previous)
{
	std::vector<HWND> controls;
	CollectControls(dlg, controls);
	auto it = std::find(controls.begin(), controls.end(), ctl);
	if(it == controls.end())
		return ctl;
	// The group: from the control with WS_GROUP before ctl up to the next one.
	std::size_t index = static_cast<std::size_t>(it - controls.begin());
	std::size_t first = index;
	while(first > 0 && !(::GetWindowLongPtr(controls[first], GWL_STYLE) & WS_GROUP))
		first--;
	std::size_t last = index;
	while(last + 1 < controls.size() && !(::GetWindowLongPtr(controls[last + 1], GWL_STYLE) & WS_GROUP))
		last++;
	const std::size_t size = last - first + 1;
	for(std::size_t i = 1; i <= size; i++)
	{
		const std::size_t pos = previous ? first + (index - first + size - i) % size : first + (index - first + i) % size;
		const LONG_PTR style = ::GetWindowLongPtr(controls[pos], GWL_STYLE);
		if((style & WS_VISIBLE) && !(style & WS_DISABLED))
			return controls[pos];
	}
	return ctl;
}

BOOL IsDialogMessage(HWND dlg, LPMSG msg)
{
	if(dlg == nullptr || msg == nullptr || (msg->hwnd != dlg && !::IsChild(dlg, msg->hwnd)))
		return FALSE;
	dlg = RootDialog(dlg);
	const HWND ctl = msg->hwnd;
	switch(msg->message)
	{
	case WM_KEYDOWN:
		{
			const DWORD code = DlgCode(ctl, msg);
			if(code & (DLGC_WANTMESSAGE | DLGC_WANTALLKEYS))
				break;
			switch(msg->wParam)
			{
			case VK_TAB:
				if(code & DLGC_WANTTAB)
					break;
				::SendMessage(dlg, WM_NEXTDLGCTL, (::GetKeyState(VK_SHIFT) & 0x8000) ? 1 : 0, 0);
				return TRUE;
			case VK_LEFT:
			case VK_RIGHT:
			case VK_UP:
			case VK_DOWN:
				if(code & DLGC_WANTARROWS)
					break;
				{
					const HWND next = ::GetNextDlgGroupItem(dlg, DirectControl(dlg, ctl), msg->wParam == VK_LEFT || msg->wParam == VK_UP);
					if(next != nullptr && next != ctl)
					{
						const DWORD nextCode = DlgCode(next);
						::SetFocus(next);
						FocusChanged(dlg, next);
						// Arrowing onto an automatic radio button checks it.
						if((nextCode & DLGC_RADIOBUTTON) && (::GetWindowLongPtr(next, GWL_STYLE) & BS_TYPEMASK) == BS_AUTORADIOBUTTON)
							::SendMessage(next, BM_CLICK, 0, 0);
					}
				}
				return TRUE;
			case VK_RETURN:
				{
					// The focused push button, or the default one.
					HWND button = nullptr;
					if(code & DLGC_DEFPUSHBUTTON)
						button = ctl;
					else
						button = DefaultButton(dlg);
					const UINT id = button != nullptr ? static_cast<UINT>(::GetDlgCtrlID(button)) : (HIWORD(::SendMessage(dlg, DM_GETDEFID, 0, 0)) == DC_HASDEFID ? LOWORD(::SendMessage(dlg, DM_GETDEFID, 0, 0)) : IDOK);
					if(button == nullptr || ::IsWindowEnabled(button))
						SendCommand(dlg, id, button != nullptr ? button : ::GetDlgItem(dlg, static_cast<int>(id)));
					else
						::MessageBeep(0);
				}
				return TRUE;
			case VK_ESCAPE:
				{
					const HWND cancel = ::GetDlgItem(dlg, IDCANCEL);
					if(cancel == nullptr || ::IsWindowEnabled(cancel))
						SendCommand(dlg, IDCANCEL, cancel);
				}
				return TRUE;
			default:
				break;
			}
			break;
		}
	case WM_CHAR:
		{
			const DWORD code = DlgCode(ctl, msg);
			if(code & (DLGC_WANTCHARS | DLGC_WANTMESSAGE | DLGC_WANTALLKEYS))
				break;
			if(msg->wParam == '\t' && !(code & DLGC_WANTTAB))
				return TRUE;
			if(msg->wParam == '\r' || msg->wParam == 0x1B)
				return TRUE;
			[[fallthrough]];
		}
	case WM_SYSCHAR:
		{
			if(msg->message == WM_SYSCHAR && !(msg->lParam & 0x20000000))
				break;
			const HWND target = FindMnemonic(dlg, ::GetFocus(), static_cast<WCHAR>(msg->wParam));
			if(target == nullptr)
			{
				if(msg->message == WM_SYSCHAR)
					break; // the menu bar may know it
				break;
			}
			const DWORD code = DlgCode(target);
			if(code & DLGC_STATIC)
			{
				// A label moves to the control after it.
				const HWND next = ::GetNextDlgTabItem(dlg, target, FALSE);
				if(next != nullptr)
				{
					DlgSetFocus(next);
					FocusChanged(dlg, next);
				}
			} else if(code & (DLGC_DEFPUSHBUTTON | DLGC_UNDEFPUSHBUTTON | DLGC_BUTTON | DLGC_RADIOBUTTON))
			{
				if(!(::GetWindowLongPtr(target, GWL_STYLE) & WS_DISABLED))
				{
					::SetFocus(target);
					FocusChanged(dlg, target);
					::SendMessage(target, BM_CLICK, 0, 0);
				}
			}
			return TRUE;
		}
	default:
		break;
	}
	// Everything else is dispatched as usual.
	::TranslateMessage(msg);
	::DispatchMessage(msg);
	return TRUE;
}

// ---- controls by id -------------------------------------------------------------------

BOOL CheckRadioButton(HWND dlg, int first, int last, int check)
{
	for(int id = first; id <= last; id++)
		::SendDlgItemMessage(dlg, id, BM_SETCHECK, id == check ? BST_CHECKED : BST_UNCHECKED, 0);
	return TRUE;
}

BOOL SetDlgItemInt(HWND dlg, int id, UINT value, BOOL isSigned)
{
	wchar_t text[32];
	if(isSigned)
		std::swprintf(text, 32, L"%d", static_cast<int>(value));
	else
		std::swprintf(text, 32, L"%u", value);
	return ::SetDlgItemText(dlg, id, text);
}

UINT GetDlgItemInt(HWND dlg, int id, BOOL *translated, BOOL isSigned)
{
	wchar_t text[64] = {};
	::GetDlgItemText(dlg, id, text, 64);
	const wchar_t *p = text;
	while(*p == L' ')
		p++;
	bool negative = false;
	if(isSigned && *p == L'-')
	{
		negative = true;
		p++;
	}
	if(*p < L'0' || *p > L'9')
	{
		if(translated)
			*translated = FALSE;
		return 0;
	}
	unsigned long long value = 0;
	for(; *p >= L'0' && *p <= L'9'; p++)
		value = value * 10 + static_cast<unsigned>(*p - L'0');
	while(*p == L' ')
		p++;
	const bool ok = *p == 0 && (isSigned ? value <= (negative ? 0x80000000ULL : 0x7FFFFFFFULL) : value <= 0xFFFFFFFFULL);
	if(translated)
		*translated = ok ? TRUE : FALSE;
	if(!ok)
		return 0;
	return negative ? static_cast<UINT>(-static_cast<long long>(value)) : static_cast<UINT>(value);
}
