// The SysLink control of comctl32: text with <a href="..." id="...">links</a>
// in it. The text wraps at the control's width (lines break at '\n' too),
// links are drawn underlined in the link colour, and clicking one, or
// pressing Space on it, sends NM_CLICK with an NMLINK to the parent; Enter
// sends NM_RETURN. Tab moves between the links before leaving the control.
// LM_HITTEST, LM_GETITEM, LM_SETITEM and LM_GETIDEALSIZE work as there.
#include "QtWin.h"

#include <algorithm>
#include <cwctype>
#include <string>
#include <vector>

namespace linopenmpt::qtwin
{

namespace
{

struct Link
{
	std::wstring id;
	std::wstring url;
	UINT state = LIS_ENABLED;
};

struct Segment
{
	std::wstring text;
	int link = -1;
};

struct Fragment
{
	std::wstring text;
	int link = -1;
	RECT rect{};
};

struct LinkState : ControlState
{
	std::vector<Segment> segments;
	std::vector<Link> links;
	std::vector<Fragment> fragments; // laid out
	int layoutWidth = -1;
	int height = 0;                  // of the laid-out text
	HFONT font = nullptr;
	HFONT underline = nullptr;
	HFONT underlineOf = nullptr;     // the font the underlined one was made from
	int focus = -1;                  // the focused link
	int pressed = -1;                // the link the mouse went down on
	int hot = -1;
	bool focused = false;
	bool trackingLeave = false;
};

LinkState *State(HWND hwnd)
{
	return Control<LinkState>(hwnd);
}

LONG_PTR Style(HWND hwnd)
{
	return ::GetWindowLongPtr(hwnd, GWL_STYLE);
}

bool StartsWithNoCase(const std::wstring &text, std::size_t pos, const wchar_t *what)
{
	for(std::size_t i = 0; what[i] != 0; i++)
	{
		if(pos + i >= text.size() || std::towlower(text[pos + i]) != std::towlower(what[i]))
			return false;
	}
	return true;
}

// Reads <a ...> at pos: on success, end is past the '>' and the attributes are filled in.
bool ParseOpenTag(const std::wstring &text, std::size_t pos, std::size_t &end, Link &link)
{
	if(!StartsWithNoCase(text, pos, L"<a"))
		return false;
	std::size_t i = pos + 2;
	if(i < text.size() && text[i] != L'>' && !std::iswspace(text[i]))
		return false;
	while(i < text.size())
	{
		while(i < text.size() && std::iswspace(text[i]))
			i++;
		if(i >= text.size())
			return false;
		if(text[i] == L'>')
		{
			end = i + 1;
			return true;
		}
		std::size_t nameStart = i;
		while(i < text.size() && text[i] != L'=' && text[i] != L'>' && !std::iswspace(text[i]))
			i++;
		std::wstring name = text.substr(nameStart, i - nameStart);
		std::transform(name.begin(), name.end(), name.begin(), [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
		while(i < text.size() && std::iswspace(text[i]))
			i++;
		if(i >= text.size() || text[i] != L'=')
			return false;
		i++;
		while(i < text.size() && std::iswspace(text[i]))
			i++;
		if(i >= text.size())
			return false;
		std::wstring value;
		if(text[i] == L'"' || text[i] == L'\'')
		{
			const wchar_t quote = text[i++];
			const std::size_t close = text.find(quote, i);
			if(close == std::wstring::npos)
				return false;
			value = text.substr(i, close - i);
			i = close + 1;
		} else
		{
			const std::size_t start = i;
			while(i < text.size() && text[i] != L'>' && !std::iswspace(text[i]))
				i++;
			value = text.substr(start, i - start);
		}
		if(name == L"href")
			link.url = value;
		else if(name == L"id")
			link.id = value;
	}
	return false;
}

void Parse(LinkState &s, const std::wstring &text)
{
	s.segments.clear();
	s.links.clear();
	s.fragments.clear();
	s.layoutWidth = -1;
	s.focus = s.pressed = s.hot = -1;
	std::wstring current;
	int link = -1;
	auto flush = [&]()
	{
		if(!current.empty())
			s.segments.push_back(Segment{current, link});
		current.clear();
	};
	std::size_t i = 0;
	while(i < text.size())
	{
		if(text[i] == L'<')
		{
			std::size_t end = 0;
			Link parsed;
			if(link < 0 && ParseOpenTag(text, i, end, parsed))
			{
				// A link needs its closing tag; without it the tag is text.
				const std::size_t close = [&]()
				{
					for(std::size_t j = end; j + 4 <= text.size(); j++)
					{
						if(StartsWithNoCase(text, j, L"</a>"))
							return j;
					}
					return std::wstring::npos;
				}();
				if(close != std::wstring::npos)
				{
					flush();
					s.links.push_back(parsed);
					link = static_cast<int>(s.links.size()) - 1;
					i = end;
					continue;
				}
			} else if(link >= 0 && StartsWithNoCase(text, i, L"</a>"))
			{
				flush();
				link = -1;
				i += 4;
				continue;
			}
		}
		current += text[i++];
	}
	flush();
}

HFONT Font(const LinkState &s)
{
	return s.font != nullptr ? s.font : static_cast<HFONT>(::GetStockObject(DEFAULT_GUI_FONT));
}

HFONT UnderlineFont(LinkState &s)
{
	const HFONT base = Font(s);
	if(s.underline == nullptr || s.underlineOf != base)
	{
		if(s.underline != nullptr)
			::DeleteObject(s.underline);
		LOGFONT lf{};
		::GetObject(base, sizeof(lf), &lf);
		lf.lfUnderline = TRUE;
		s.underline = ::CreateFontIndirect(&lf);
		s.underlineOf = base;
	}
	return s.underline;
}

bool IsActiveLink(const LinkState &s, int link)
{
	return link >= 0 && static_cast<std::size_t>(link) < s.links.size() && (s.links[link].state & LIS_ENABLED);
}

// Lays the text out at the given width: words wrap, '\n' breaks lines.
int Layout(HWND hwnd, LinkState &s, int width, std::vector<Fragment> *out)
{
	HDC dc = ::CreateCompatibleDC(nullptr);
	HGDIOBJ old = ::SelectObject(dc, Font(s));
	TEXTMETRIC tm{};
	::GetTextMetrics(dc, &tm);
	const int lineHeight = std::max(1, static_cast<int>(tm.tmHeight));
	std::vector<Fragment> fragments;
	std::vector<std::size_t> lineStarts{0};
	int x = 0, y = 0;
	auto newLine = [&]()
	{
		x = 0;
		y += lineHeight;
		lineStarts.push_back(fragments.size());
	};
	for(const Segment &segment : s.segments)
	{
		::SelectObject(dc, IsActiveLink(s, segment.link) ? UnderlineFont(s) : Font(s));
		std::size_t i = 0;
		const std::wstring &text = segment.text;
		while(i < text.size())
		{
			if(text[i] == L'\r')
			{
				i++;
				continue;
			}
			if(text[i] == L'\n')
			{
				newLine();
				i++;
				continue;
			}
			// A word and the spaces after it.
			std::size_t end = i;
			while(end < text.size() && text[end] != L' ' && text[end] != L'\n' && text[end] != L'\r')
				end++;
			std::size_t wordEnd = end;
			while(end < text.size() && text[end] == L' ')
				end++;
			const std::wstring word = text.substr(i, end - i);
			SIZE wordSize{}, fullSize{};
			::GetTextExtentPoint32(dc, word.c_str(), static_cast<int>(wordEnd - i), &wordSize);
			::GetTextExtentPoint32(dc, word.c_str(), static_cast<int>(word.size()), &fullSize);
			if(x > 0 && width > 0 && x + wordSize.cx > width)
			{
				newLine();
				if(wordEnd == i)
				{
					i = end; // only spaces: they vanish at the line break
					continue;
				}
			}
			Fragment f;
			f.text = word;
			f.link = segment.link;
			f.rect = RECT{x, y, x + static_cast<int>(fullSize.cx), y + lineHeight};
			// Links are hit and focused without their trailing spaces.
			if(f.link >= 0 && end == text.size())
				f.rect.right = x + static_cast<int>(wordSize.cx);
			fragments.push_back(f);
			x += fullSize.cx;
			i = end;
		}
	}
	::SelectObject(dc, old);
	::DeleteDC(dc);
	const int height = fragments.empty() && y == 0 ? 0 : y + lineHeight;
	if(Style(hwnd) & LWS_RIGHT)
	{
		lineStarts.push_back(fragments.size());
		for(std::size_t l = 0; l + 1 < lineStarts.size(); l++)
		{
			if(lineStarts[l] == lineStarts[l + 1])
				continue;
			const int right = fragments[lineStarts[l + 1] - 1].rect.right;
			const int shift = std::max(0, width - right);
			for(std::size_t f = lineStarts[l]; f < lineStarts[l + 1]; f++)
				::OffsetRect(&fragments[f].rect, shift, 0);
		}
	}
	if(out != nullptr)
		*out = std::move(fragments);
	return height;
}

void EnsureLayout(HWND hwnd, LinkState &s)
{
	RECT rc{};
	::GetClientRect(hwnd, &rc);
	if(s.layoutWidth == rc.right)
		return;
	s.layoutWidth = rc.right;
	s.height = Layout(hwnd, s, rc.right, &s.fragments);
}

int LinkAt(HWND hwnd, LinkState &s, POINT pt)
{
	EnsureLayout(hwnd, s);
	for(const Fragment &f : s.fragments)
	{
		if(f.link >= 0 && ::PtInRect(&f.rect, pt))
			return f.link;
	}
	return -1;
}

void FillItem(const LinkState &s, int index, LITEM &item)
{
	const Link &link = s.links[index];
	item.iLink = index;
	const std::size_t idLength = std::min(link.id.size(), static_cast<std::size_t>(MAX_LINKID_TEXT) - 1);
	std::copy_n(link.id.c_str(), idLength, item.szID);
	item.szID[idLength] = 0;
	const std::size_t urlLength = std::min(link.url.size(), static_cast<std::size_t>(L_MAX_URL_LENGTH) - 1);
	std::copy_n(link.url.c_str(), urlLength, item.szUrl);
	item.szUrl[urlLength] = 0;
	item.state = link.state;
}

void SendLinkNotify(HWND hwnd, int index, UINT code)
{
	LinkState *s = State(hwnd);
	if(s == nullptr || index < 0 || static_cast<std::size_t>(index) >= s->links.size())
		return;
	NMLINK nm{};
	FillItem(*s, index, nm.item);
	nm.item.mask = LIF_ITEMINDEX | LIF_ITEMID | LIF_URL | LIF_STATE;
	nm.hdr.hwndFrom = hwnd;
	nm.hdr.idFrom = static_cast<UINT_PTR>(::GetDlgCtrlID(hwnd));
	nm.hdr.code = code;
	::SendMessage(::GetParent(hwnd), WM_NOTIFY, nm.hdr.idFrom, reinterpret_cast<LPARAM>(&nm));
}

void SetFocusLink(HWND hwnd, LinkState &s, int index)
{
	for(std::size_t i = 0; i < s.links.size(); i++)
	{
		if(static_cast<int>(i) == index)
			s.links[i].state |= LIS_FOCUSED;
		else
			s.links[i].state &= ~LIS_FOCUSED;
	}
	s.focus = index;
	::InvalidateRect(hwnd, nullptr, TRUE);
}

// The next (or previous) enabled link from the focused one, or -1.
int NextLink(const LinkState &s, bool back)
{
	const int count = static_cast<int>(s.links.size());
	int i = s.focus;
	for(;;)
	{
		i += back ? -1 : 1;
		if(s.focus < 0 && back && i < 0)
			i = count - 1;
		if(i < 0 || i >= count)
			return -1;
		if(s.links[i].state & LIS_ENABLED)
			return i;
	}
}

void Paint(HWND hwnd, HDC dc)
{
	LinkState *s = State(hwnd);
	if(s == nullptr)
		return;
	RECT rc{};
	::GetClientRect(hwnd, &rc);
	HBRUSH brush = ControlBrush(hwnd, dc, WM_CTLCOLORSTATIC);
	if(!(Style(hwnd) & LWS_TRANSPARENT))
		::FillRect(dc, &rc, brush);
	if((s = State(hwnd)) == nullptr)
		return;
	EnsureLayout(hwnd, *s);
	const bool enabled = ::IsWindowEnabled(hwnd) != FALSE;
	const COLORREF textColour = enabled ? ::GetTextColor(dc) : ::GetSysColor(COLOR_GRAYTEXT);
	const COLORREF linkColour = enabled ? ::GetSysColor(COLOR_HOTLIGHT) : ::GetSysColor(COLOR_GRAYTEXT);
	::SetBkMode(dc, TRANSPARENT);
	HGDIOBJ oldFont = ::SelectObject(dc, Font(*s));
	for(std::size_t i = 0; i < s->fragments.size(); i++)
	{
		const Fragment &f = s->fragments[i];
		const bool link = IsActiveLink(*s, f.link);
		::SelectObject(dc, link ? UnderlineFont(*s) : Font(*s));
		::SetTextColor(dc, link ? linkColour : textColour);
		std::wstring text = f.text;
		// Spaces inside a link are underlined; those at its end or a line's are not.
		const bool continues = i + 1 < s->fragments.size() && s->fragments[i + 1].link == f.link && s->fragments[i + 1].rect.top == f.rect.top;
		while(!continues && !text.empty() && text.back() == L' ')
			text.pop_back();
		::TextOut(dc, f.rect.left, f.rect.top, text.c_str(), static_cast<int>(text.size()));
	}
	::SelectObject(dc, oldFont);
	// The focus rectangle, around each line's part of the focused link.
	if(s->focused && s->focus >= 0 && !(::SendMessage(hwnd, WM_QUERYUISTATE, 0, 0) & UISF_HIDEFOCUS))
	{
		RECT line{};
		bool open = false;
		for(const Fragment &f : s->fragments)
		{
			if(f.link != s->focus)
				continue;
			if(open && f.rect.top == line.top)
			{
				line.right = std::max(line.right, f.rect.right);
				continue;
			}
			if(open)
				::DrawFocusRect(dc, &line);
			line = f.rect;
			open = true;
		}
		if(open)
			::DrawFocusRect(dc, &line);
	}
}

LRESULT CALLBACK SysLinkProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
	LinkState *s = State(hwnd);
	switch(msg)
	{
	case WM_NCCREATE:
		if(WindowData *d = Data(hwnd))
			d->control = std::make_unique<LinkState>();
		return ::DefWindowProc(hwnd, msg, wp, lp);
	case WM_CREATE:
		if(s != nullptr)
		{
			const auto *cs = reinterpret_cast<const CREATESTRUCT *>(lp);
			Parse(*s, cs->lpszName != nullptr ? std::wstring(cs->lpszName) : std::wstring());
		}
		return 0;
	case WM_NCDESTROY:
		if(s != nullptr && s->underline != nullptr)
			::DeleteObject(s->underline);
		if(WindowData *d = Data(hwnd))
			d->control.reset();
		return ::DefWindowProc(hwnd, msg, wp, lp);
	case WM_SETTEXT:
		::DefWindowProc(hwnd, msg, wp, lp);
		if((s = State(hwnd)) != nullptr)
		{
			Parse(*s, lp != 0 ? std::wstring(reinterpret_cast<LPCWSTR>(lp)) : std::wstring());
			if(s->focused)
				SetFocusLink(hwnd, *s, NextLink(*s, false));
			::InvalidateRect(hwnd, nullptr, TRUE);
		}
		return TRUE;
	case WM_SETFONT:
		if(s != nullptr)
		{
			s->font = reinterpret_cast<HFONT>(wp);
			s->layoutWidth = -1;
			if(LOWORD(lp))
				::InvalidateRect(hwnd, nullptr, TRUE);
		}
		return 0;
	case WM_GETFONT:
		return s != nullptr ? reinterpret_cast<LRESULT>(s->font) : 0;
	case WM_SIZE:
		if(s != nullptr)
		{
			s->layoutWidth = -1;
			::InvalidateRect(hwnd, nullptr, TRUE);
		}
		return 0;
	case WM_ENABLE:
		::InvalidateRect(hwnd, nullptr, TRUE);
		return 0;
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
	case WM_SETCURSOR:
		if(s != nullptr && LOWORD(lp) == HTCLIENT)
		{
			POINT pt{};
			::GetCursorPos(&pt);
			::ScreenToClient(hwnd, &pt);
			if(IsActiveLink(*s, LinkAt(hwnd, *s, pt)))
			{
				::SetCursor(::LoadCursor(nullptr, IDC_HAND));
				return TRUE;
			}
		}
		return ::DefWindowProc(hwnd, msg, wp, lp);
	case WM_MOUSEMOVE:
		if(s != nullptr)
		{
			const POINT pt{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
			const int link = LinkAt(hwnd, *s, pt);
			if(link != s->hot)
			{
				if(s->hot >= 0 && static_cast<std::size_t>(s->hot) < s->links.size())
					s->links[s->hot].state &= ~LIS_HOTTRACK;
				s->hot = link;
				if(link >= 0)
					s->links[link].state |= LIS_HOTTRACK;
			}
			if(!s->trackingLeave)
			{
				TRACKMOUSEEVENT tme{sizeof(tme), TME_LEAVE, hwnd, 0};
				::TrackMouseEvent(&tme);
				s->trackingLeave = true;
			}
		}
		return 0;
	case WM_MOUSELEAVE:
		if(s != nullptr)
		{
			s->trackingLeave = false;
			if(s->hot >= 0 && static_cast<std::size_t>(s->hot) < s->links.size())
				s->links[s->hot].state &= ~LIS_HOTTRACK;
			s->hot = -1;
		}
		return 0;
	case WM_LBUTTONDOWN:
	case WM_LBUTTONDBLCLK:
		if(s != nullptr)
		{
			const POINT pt{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
			const int link = LinkAt(hwnd, *s, pt);
			if(IsActiveLink(*s, link))
			{
				s->pressed = link;
				SetFocusLink(hwnd, *s, link);
				::SetFocus(hwnd);
				::SetCapture(hwnd);
			}
		}
		return 0;
	case WM_LBUTTONUP:
		if(s != nullptr && s->pressed >= 0)
		{
			const int pressed = s->pressed;
			s->pressed = -1;
			if(::GetCapture() == hwnd)
				::ReleaseCapture();
			const POINT pt{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
			if((s = State(hwnd)) != nullptr && LinkAt(hwnd, *s, pt) == pressed)
				SendLinkNotify(hwnd, pressed, NM_CLICK);
		}
		return 0;
	case WM_CAPTURECHANGED:
		if(s != nullptr)
			s->pressed = -1;
		return 0;
	case WM_SETFOCUS:
		if(s != nullptr)
		{
			s->focused = true;
			if(s->focus < 0)
			{
				// Shift+Tab into the control focuses its last link.
				const bool back = (::GetKeyState(VK_SHIFT) & 0x8000) && (::GetKeyState(VK_TAB) & 0x8000);
				SetFocusLink(hwnd, *s, NextLink(*s, back));
			}
			::InvalidateRect(hwnd, nullptr, TRUE);
		}
		return 0;
	case WM_KILLFOCUS:
		if(s != nullptr)
		{
			s->focused = false;
			::InvalidateRect(hwnd, nullptr, TRUE);
		}
		return 0;
	case WM_GETDLGCODE:
		{
			LRESULT code = DLGC_WANTCHARS;
			const auto *m = reinterpret_cast<const MSG *>(lp);
			if(s != nullptr && m != nullptr && m->message == WM_KEYDOWN)
			{
				if(m->wParam == VK_TAB && NextLink(*s, (::GetKeyState(VK_SHIFT) & 0x8000) != 0) >= 0)
					code |= DLGC_WANTTAB;
				if(m->wParam == VK_RETURN && !(Style(hwnd) & LWS_IGNORERETURN) && s->focus >= 0)
					code |= DLGC_WANTMESSAGE;
			}
			return code;
		}
	case WM_KEYDOWN:
		if(s != nullptr)
		{
			switch(wp)
			{
			case VK_TAB:
				{
					const int next = NextLink(*s, (::GetKeyState(VK_SHIFT) & 0x8000) != 0);
					if(next >= 0)
						SetFocusLink(hwnd, *s, next);
				}
				return 0;
			case VK_RETURN:
				if(!(Style(hwnd) & LWS_IGNORERETURN) && IsActiveLink(*s, s->focus))
					SendLinkNotify(hwnd, s->focus, NM_RETURN);
				return 0;
			case VK_SPACE:
				if(IsActiveLink(*s, s->focus))
					SendLinkNotify(hwnd, s->focus, NM_CLICK);
				return 0;
			default:
				break;
			}
		}
		return ::DefWindowProc(hwnd, msg, wp, lp);
	case LM_HITTEST:
		if(s != nullptr && lp != 0)
		{
			auto *info = reinterpret_cast<LHITTESTINFO *>(lp);
			const int link = LinkAt(hwnd, *s, info->pt);
			if(link < 0)
				return FALSE;
			FillItem(*s, link, info->item);
			info->item.mask = LIF_ITEMINDEX | LIF_ITEMID | LIF_URL | LIF_STATE;
			return TRUE;
		}
		return FALSE;
	case LM_GETITEM:
	case LM_SETITEM:
		if(s != nullptr && lp != 0)
		{
			auto *item = reinterpret_cast<LITEM *>(lp);
			int index = -1;
			if(item->mask & LIF_ITEMINDEX)
				index = item->iLink;
			else if(item->mask & LIF_ITEMID)
			{
				for(std::size_t i = 0; i < s->links.size(); i++)
				{
					if(s->links[i].id == item->szID)
						index = static_cast<int>(i);
				}
			}
			if(index < 0 || static_cast<std::size_t>(index) >= s->links.size())
				return FALSE;
			Link &link = s->links[index];
			if(msg == LM_GETITEM)
			{
				LITEM full{};
				FillItem(*s, index, full);
				if(item->mask & LIF_STATE)
					item->state = full.state & item->stateMask;
				if(item->mask & LIF_URL)
					std::copy_n(full.szUrl, L_MAX_URL_LENGTH, item->szUrl);
				if((item->mask & LIF_ITEMID) && (item->mask & LIF_ITEMINDEX))
					std::copy_n(full.szID, MAX_LINKID_TEXT, item->szID);
				return TRUE;
			}
			if(item->mask & LIF_STATE)
			{
				link.state = (link.state & ~item->stateMask) | (item->state & item->stateMask);
				if((item->stateMask & LIS_FOCUSED) && (item->state & LIS_FOCUSED))
					SetFocusLink(hwnd, *s, index);
			}
			if(item->mask & LIF_URL)
				link.url = item->szUrl;
			if((item->mask & LIF_ITEMID) && (item->mask & LIF_ITEMINDEX))
				link.id = item->szID;
			s->layoutWidth = -1; // enabled links are underlined, which may change widths
			::InvalidateRect(hwnd, nullptr, TRUE);
			return TRUE;
		}
		return FALSE;
	case LM_GETIDEALSIZE:
		if(s != nullptr)
		{
			RECT rc{};
			::GetClientRect(hwnd, &rc);
			const int width = wp > 0 ? static_cast<int>(wp) : rc.right;
			std::vector<Fragment> fragments;
			const int height = Layout(hwnd, *s, width, &fragments);
			if(lp != 0)
			{
				int right = 0;
				for(const Fragment &f : fragments)
					right = std::max(right, static_cast<int>(f.rect.right));
				auto *size = reinterpret_cast<SIZE *>(lp);
				size->cx = right;
				size->cy = height;
			}
			return height;
		}
		return 0;
	default:
		return ::DefWindowProc(hwnd, msg, wp, lp);
	}
}

} // namespace

void RegisterSysLinkClass()
{
	WNDCLASS wc{};
	wc.style = CS_DBLCLKS;
	wc.lpfnWndProc = SysLinkProc;
	wc.hCursor = ::LoadCursor(nullptr, IDC_ARROW);
	wc.lpszClassName = WC_LINK;
	::RegisterClass(&wc);
}

} // namespace linopenmpt::qtwin
