// The status bar of comctl32 (msctls_statusbar32): a row of parts
// (SB_SETPARTS, right edges, -1 for the rest), each with its text and a
// sunken, raised or no border, owner-drawn parts (WM_DRAWITEM), the simple
// mode (one part over the whole bar), and the size grip (SBARS_SIZEGRIP),
// which sizes the top-level window. Without CCS_NOPARENTALIGN or
// CCS_NORESIZE, a WM_SIZE puts the bar along the bottom of its parent, as
// comctl32 does. Text with tabs is aligned: after one tab centred, after two
// right-aligned.
#include "QtWin.h"

#include <algorithm>
#include <string>
#include <vector>

namespace linopenmpt::qtwin
{

namespace
{

constexpr int HorzBorder = 0;   // SB_GETBORDERS: around the parts
constexpr int VertBorder = 2;
constexpr int Gap = 2;          // between parts

struct Part
{
	int right = -1;
	std::wstring text;
	UINT type = 0;          // SBT_*
	LPARAM data = 0;        // SBT_OWNERDRAW
	std::wstring tip;
};

struct StatusState : ControlState
{
	std::vector<Part> parts{Part{}};
	Part simple;
	bool isSimple = false;
	int minHeight = 0;
	HFONT font = nullptr;
	COLORREF background = CLR_DEFAULT;
};

StatusState *State(HWND hwnd)
{
	return Control<StatusState>(hwnd);
}

LONG_PTR Style(HWND hwnd)
{
	return ::GetWindowLongPtr(hwnd, GWL_STYLE);
}

HFONT Font(const StatusState &s)
{
	return s.font != nullptr ? s.font : static_cast<HFONT>(::GetStockObject(DEFAULT_GUI_FONT));
}

int GripSize()
{
	return ::GetSystemMetrics(SM_CXVSCROLL);
}

bool HasGrip(HWND hwnd)
{
	if(!(Style(hwnd) & SBARS_SIZEGRIP))
		return false;
	// No grip while the window it would size is maximized.
	const HWND root = ::GetAncestor(hwnd, GA_ROOT);
	return root == nullptr || !::IsZoomed(root);
}

// The height comctl32 gives the bar: the font's, the borders, the minimum.
int IdealHeight(const StatusState &s)
{
	HDC dc = ::CreateCompatibleDC(nullptr);
	HGDIOBJ old = ::SelectObject(dc, Font(s));
	TEXTMETRIC tm{};
	::GetTextMetrics(dc, &tm);
	::SelectObject(dc, old);
	::DeleteDC(dc);
	const int edges = 2 * ::GetSystemMetrics(SM_CYEDGE);
	return std::max(static_cast<int>(tm.tmHeight) + 2 * VertBorder + edges + 1, s.minHeight + 2 * VertBorder);
}

RECT PartRect(HWND hwnd, const StatusState &s, int index)
{
	RECT client{};
	::GetClientRect(hwnd, &client);
	RECT rc{client.left + HorzBorder, client.top + VertBorder, 0, client.bottom - VertBorder + (VertBorder > 0 ? 0 : 0)};
	if(s.isSimple)
	{
		rc.right = client.right - HorzBorder;
		return rc;
	}
	for(int i = 0; i <= index && i < static_cast<int>(s.parts.size()); i++)
	{
		if(i > 0)
			rc.left = rc.right + Gap;
		const int right = s.parts[static_cast<std::size_t>(i)].right;
		rc.right = right < 0 ? client.right - HorzBorder : std::max(static_cast<int>(rc.left), right);
	}
	// The last part (and one that reaches it) leaves room for the grip.
	if(HasGrip(hwnd))
		rc.right = std::min(static_cast<int>(rc.right), static_cast<int>(client.right) - GripSize());
	rc.right = std::max(rc.right, rc.left);
	return rc;
}

void DrawPart(HWND hwnd, HDC dc, StatusState &s, const Part &part, RECT rc, UINT index)
{
	if(!(part.type & SBT_NOBORDERS))
		::DrawEdge(dc, &rc, (part.type & SBT_POPOUT) ? BDR_RAISEDINNER : BDR_SUNKENOUTER, BF_RECT | BF_ADJUST);
	if(part.type & SBT_OWNERDRAW)
	{
		DRAWITEMSTRUCT dis{};
		dis.CtlID = static_cast<UINT>(::GetDlgCtrlID(hwnd));
		dis.itemID = index;
		dis.hwndItem = hwnd;
		dis.hDC = dc;
		dis.rcItem = rc;
		dis.itemData = static_cast<ULONG_PTR>(part.data);
		::SendMessage(::GetParent(hwnd), WM_DRAWITEM, dis.CtlID, reinterpret_cast<LPARAM>(&dis));
		return;
	}
	if(part.text.empty())
		return;
	// As comctl32's DrawStatusText: three pixels in on the left, and text
	// that does not fit is clipped, not shortened.
	RECT text = rc;
	text.left += 3;
	// Tabs: left, centre, right.
	std::wstring pieces[3];
	if(part.type & SBT_NOTABPARSING)
	{
		pieces[0] = part.text;
	} else
	{
		int which = 0;
		for(wchar_t c : part.text)
		{
			if(c == L'\t' && which < 2)
				which++;
			else
				pieces[which] += c;
		}
	}
	const UINT formats[3] = {DT_LEFT, DT_CENTER, DT_RIGHT};
	for(int i = 0; i < 3; i++)
	{
		if(!pieces[i].empty())
			::DrawText(dc, pieces[i].c_str(), static_cast<int>(pieces[i].size()), &text, formats[i] | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
	}
	(void)s;
}

void Paint(HWND hwnd, HDC dc)
{
	StatusState *s = State(hwnd);
	if(s == nullptr)
		return;
	RECT client{};
	::GetClientRect(hwnd, &client);
	if(s->background != CLR_DEFAULT)
	{
		HBRUSH brush = ::CreateSolidBrush(s->background);
		::FillRect(dc, &client, brush);
		::DeleteObject(brush);
	} else
	{
		::FillRect(dc, &client, ::GetSysColorBrush(COLOR_BTNFACE));
	}
	::SetBkMode(dc, TRANSPARENT);
	::SetTextColor(dc, ::GetSysColor(COLOR_BTNTEXT));
	HGDIOBJ oldFont = ::SelectObject(dc, Font(*s));
	if(s->isSimple)
	{
		DrawPart(hwnd, dc, *s, s->simple, PartRect(hwnd, *s, 0), SB_SIMPLEID);
	} else
	{
		for(std::size_t i = 0; i < s->parts.size(); i++)
		{
			if((s = State(hwnd)) == nullptr || i >= s->parts.size())
				break;
			const Part part = s->parts[i];
			DrawPart(hwnd, dc, *s, part, PartRect(hwnd, *s, static_cast<int>(i)), static_cast<UINT>(i));
		}
	}
	if(HasGrip(hwnd))
	{
		RECT grip{client.right - GripSize(), client.bottom - GripSize(), client.right, client.bottom};
		::DrawFrameControl(dc, &grip, DFC_SCROLL, DFCS_SCROLLSIZEGRIP);
	}
	::SelectObject(dc, oldFont);
}

bool OverGrip(HWND hwnd, POINT screen)
{
	if(!HasGrip(hwnd))
		return false;
	POINT pt = screen;
	::ScreenToClient(hwnd, &pt);
	RECT client{};
	::GetClientRect(hwnd, &client);
	return pt.x >= client.right - GripSize() && pt.y >= client.bottom - GripSize() && pt.x < client.right && pt.y < client.bottom;
}

// Along the bottom of the parent, unless the styles say otherwise.
void AlignToParent(HWND hwnd, StatusState &s)
{
	const LONG_PTR style = Style(hwnd);
	if(style & CCS_NORESIZE)
		return;
	const HWND parent = ::GetParent(hwnd);
	if(parent == nullptr)
		return;
	RECT parentClient{};
	::GetClientRect(parent, &parentClient);
	const int height = IdealHeight(s);
	RECT now{};
	::GetWindowRect(hwnd, &now);
	::MapWindowPoints(nullptr, parent, reinterpret_cast<POINT *>(&now), 2);
	int x = 0, y = parentClient.bottom - height, cx = parentClient.right;
	if(style & CCS_NOPARENTALIGN)
	{
		x = now.left;
		y = (style & CCS_NOMOVEY) ? now.top : now.top;
		cx = now.right - now.left;
	}
	::SetWindowPos(hwnd, nullptr, x, y, cx, height, SWP_NOZORDER | SWP_NOACTIVATE);
}

void SetText(HWND hwnd, StatusState &s, WPARAM wp, const wchar_t *text)
{
	const UINT index = LOBYTE(LOWORD(wp));
	const UINT type = static_cast<UINT>(wp) & 0xFF00;
	Part *part = nullptr;
	if(index == SB_SIMPLEID)
		part = &s.simple;
	else if(index < s.parts.size())
		part = &s.parts[index];
	if(part == nullptr)
		return;
	part->type = type;
	if(type & SBT_OWNERDRAW)
	{
		part->data = reinterpret_cast<LPARAM>(text);
		part->text.clear();
	} else
	{
		part->text = text != nullptr ? text : L"";
	}
	if(index == SB_SIMPLEID ? s.isSimple : !s.isSimple)
	{
		RECT rc = PartRect(hwnd, s, index == SB_SIMPLEID ? 0 : static_cast<int>(index));
		::InflateRect(&rc, 1, 1);
		::InvalidateRect(hwnd, &rc, TRUE);
	}
}

LRESULT CALLBACK StatusBarProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
	StatusState *s = State(hwnd);
	switch(msg)
	{
	case WM_NCCREATE:
		if(WindowData *d = Data(hwnd))
			d->control = std::make_unique<StatusState>();
		return ::DefWindowProc(hwnd, msg, wp, lp);
	case WM_CREATE:
		if(s != nullptr)
		{
			const auto *cs = reinterpret_cast<const CREATESTRUCT *>(lp);
			if(cs != nullptr && cs->lpszName != nullptr)
				s->parts[0].text = cs->lpszName;
			AlignToParent(hwnd, *s);
		}
		return 0;
	case WM_NCDESTROY:
		if(WindowData *d = Data(hwnd))
			d->control.reset();
		return ::DefWindowProc(hwnd, msg, wp, lp);
	case WM_SETFONT:
		if(s != nullptr)
		{
			s->font = reinterpret_cast<HFONT>(wp);
			if(LOWORD(lp))
				::InvalidateRect(hwnd, nullptr, TRUE);
		}
		return 0;
	case WM_GETFONT:
		// comctl32 answers with the font it draws with, its own by default.
		return s != nullptr ? reinterpret_cast<LRESULT>(Font(*s)) : 0;
	case WM_SETTEXT:
		if(s != nullptr)
			SetText(hwnd, *s, 0, reinterpret_cast<const wchar_t *>(lp));
		return TRUE;
	case WM_GETTEXT:
	case WM_GETTEXTLENGTH:
	case SB_GETTEXTW:
	case SB_GETTEXTLENGTHW:
	{
		if(s == nullptr)
			return 0;
		const UINT index = (msg == WM_GETTEXT || msg == WM_GETTEXTLENGTH) ? 0 : LOBYTE(LOWORD(wp));
		const Part *part = index == SB_SIMPLEID ? &s->simple : (index < s->parts.size() ? &s->parts[index] : nullptr);
		if(part == nullptr)
			return 0;
		if(msg == WM_GETTEXTLENGTH)
			return static_cast<LRESULT>(part->text.size());
		if(msg == WM_GETTEXT)
		{
			if(wp == 0 || lp == 0)
				return 0;
			const std::size_t n = std::min(part->text.size(), static_cast<std::size_t>(wp) - 1);
			std::copy_n(part->text.c_str(), n, reinterpret_cast<wchar_t *>(lp));
			reinterpret_cast<wchar_t *>(lp)[n] = 0;
			return static_cast<LRESULT>(n);
		}
		if(part->type & SBT_OWNERDRAW)
			return msg == SB_GETTEXTW ? static_cast<LRESULT>(part->data) : 0;
		if(msg == SB_GETTEXTW && lp != 0)
		{
			std::copy_n(part->text.c_str(), part->text.size() + 1, reinterpret_cast<wchar_t *>(lp));
		}
		return MAKELONG(static_cast<WORD>(part->text.size()), static_cast<WORD>(part->type));
	}
	case SB_SETTEXTW:
		if(s != nullptr)
		{
			const UINT index = LOBYTE(LOWORD(wp));
			if(index != SB_SIMPLEID && index >= s->parts.size())
				return FALSE;
			SetText(hwnd, *s, wp, reinterpret_cast<const wchar_t *>(lp));
			return TRUE;
		}
		return FALSE;
	case SB_SETPARTS:
		if(s != nullptr && wp > 0 && wp <= 256 && lp != 0)
		{
			const int *rights = reinterpret_cast<const int *>(lp);
			s->parts.resize(wp);
			for(std::size_t i = 0; i < wp; i++)
				s->parts[i].right = rights[i];
			::InvalidateRect(hwnd, nullptr, TRUE);
			return TRUE;
		}
		return FALSE;
	case SB_GETPARTS:
		if(s != nullptr)
		{
			if(lp != 0)
			{
				int *rights = reinterpret_cast<int *>(lp);
				for(std::size_t i = 0; i < std::min(s->parts.size(), static_cast<std::size_t>(wp)); i++)
					rights[i] = s->parts[i].right;
			}
			return static_cast<LRESULT>(s->parts.size());
		}
		return 0;
	case SB_GETBORDERS:
		if(lp != 0)
		{
			int *borders = reinterpret_cast<int *>(lp);
			borders[0] = HorzBorder;
			borders[1] = VertBorder;
			borders[2] = Gap;
			return TRUE;
		}
		return FALSE;
	case SB_SETMINHEIGHT:
		if(s != nullptr)
			s->minHeight = static_cast<int>(wp);
		return 0;
	case SB_GETRECT:
		if(s != nullptr && lp != 0)
		{
			const UINT index = static_cast<UINT>(wp);
			if(!s->isSimple && index >= s->parts.size())
				return FALSE;
			*reinterpret_cast<RECT *>(lp) = PartRect(hwnd, *s, s->isSimple ? 0 : static_cast<int>(index));
			return TRUE;
		}
		return FALSE;
	case SB_SIMPLE:
		if(s != nullptr && s->isSimple != (wp != 0))
		{
			s->isSimple = wp != 0;
			NMHDR nm{hwnd, static_cast<UINT_PTR>(::GetDlgCtrlID(hwnd)), SBN_SIMPLEMODECHANGE};
			::SendMessage(::GetParent(hwnd), WM_NOTIFY, nm.idFrom, reinterpret_cast<LPARAM>(&nm));
			::InvalidateRect(hwnd, nullptr, TRUE);
		}
		return 0;
	case SB_ISSIMPLE:
		return s != nullptr && s->isSimple;
	case SB_SETTIPTEXTW:
		if(s != nullptr && wp < s->parts.size())
			s->parts[wp].tip = lp != 0 ? reinterpret_cast<const wchar_t *>(lp) : L"";
		return 0;
	case SB_GETTIPTEXTW:
		if(s != nullptr && LOWORD(wp) < s->parts.size() && lp != 0 && HIWORD(wp) > 0)
		{
			const std::wstring &tip = s->parts[LOWORD(wp)].tip;
			const std::size_t n = std::min(tip.size(), static_cast<std::size_t>(HIWORD(wp)) - 1);
			std::copy_n(tip.c_str(), n, reinterpret_cast<wchar_t *>(lp));
			reinterpret_cast<wchar_t *>(lp)[n] = 0;
		}
		return 0;
	case SB_SETICON:
	case SB_GETICON:
		return 0;
	case SB_SETBKCOLOR:
		if(s != nullptr)
		{
			const COLORREF old = s->background;
			s->background = static_cast<COLORREF>(lp);
			::InvalidateRect(hwnd, nullptr, TRUE);
			return static_cast<LRESULT>(old);
		}
		return CLR_DEFAULT;
	case WM_SIZE:
		// A parent passes its WM_SIZE on; the bar finds its place itself.
		if(s != nullptr)
		{
			if(!(Style(hwnd) & CCS_NORESIZE))
			{
				static bool aligning = false;
				if(!aligning)
				{
					aligning = true;
					AlignToParent(hwnd, *s);
					aligning = false;
				}
			}
			::InvalidateRect(hwnd, nullptr, TRUE);
		}
		return 0;
	case WM_NCHITTEST:
		if(OverGrip(hwnd, POINT{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)}))
			return HTBOTTOMRIGHT;
		return HTCLIENT;
	case WM_NCLBUTTONDOWN:
		// The grip sizes the window the bar is in.
		if(wp == HTBOTTOMRIGHT)
		{
			if(const HWND root = ::GetAncestor(hwnd, GA_ROOT))
				::SendMessage(root, WM_NCLBUTTONDOWN, HTBOTTOMRIGHT, lp);
			return 0;
		}
		return ::DefWindowProc(hwnd, msg, wp, lp);
	case WM_SETCURSOR:
		if(LOWORD(lp) == HTBOTTOMRIGHT)
		{
			::SetCursor(::LoadCursor(nullptr, IDC_SIZENWSE));
			return TRUE;
		}
		return ::DefWindowProc(hwnd, msg, wp, lp);
	case WM_LBUTTONDOWN:
	case WM_LBUTTONDBLCLK:
	case WM_RBUTTONDOWN:
	case WM_RBUTTONDBLCLK:
	{
		NMMOUSE nm{};
		nm.hdr.hwndFrom = hwnd;
		nm.hdr.idFrom = static_cast<UINT_PTR>(::GetDlgCtrlID(hwnd));
		nm.hdr.code = msg == WM_LBUTTONDOWN ? NM_CLICK : msg == WM_LBUTTONDBLCLK ? NM_DBLCLK : msg == WM_RBUTTONDOWN ? NM_RCLICK : NM_RDBLCLK;
		nm.pt = POINT{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
		nm.dwItemSpec = static_cast<DWORD_PTR>(-1);
		if(s != nullptr)
		{
			for(std::size_t i = 0; i < s->parts.size(); i++)
			{
				const RECT rc = PartRect(hwnd, *s, static_cast<int>(i));
				if(::PtInRect(&rc, nm.pt))
					nm.dwItemSpec = i;
			}
		}
		::SendMessage(::GetParent(hwnd), WM_NOTIFY, nm.hdr.idFrom, reinterpret_cast<LPARAM>(&nm));
		return 0;
	}
	case WM_ERASEBKGND:
		return TRUE;
	case WM_PAINT:
	case WM_PRINTCLIENT:
	{
		if(msg == WM_PRINTCLIENT)
		{
			Paint(hwnd, reinterpret_cast<HDC>(wp));
			return 0;
		}
		PAINTSTRUCT ps;
		HDC dc = ::BeginPaint(hwnd, &ps);
		Paint(hwnd, dc);
		::EndPaint(hwnd, &ps);
		return 0;
	}
	default:
		return ::DefWindowProc(hwnd, msg, wp, lp);
	}
}

} // namespace

void RegisterStatusBarClass()
{
	WNDCLASS wc{};
	wc.style = CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = StatusBarProc;
	wc.hCursor = ::LoadCursor(nullptr, IDC_ARROW);
	wc.lpszClassName = STATUSCLASSNAME;
	::RegisterClass(&wc);
}

} // namespace linopenmpt::qtwin
