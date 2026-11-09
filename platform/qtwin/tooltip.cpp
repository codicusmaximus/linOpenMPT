// The tooltip control of comctl32 (tooltips_class32): a popup that shows a
// tool's text after the mouse rests on it. Tools are windows (TTF_IDISHWND)
// or rectangles in a window; the mouse reaches the tooltip through
// TTM_RELAYEVENT or, with TTF_SUBCLASS, by subclassing the tool's window.
// Text comes from the tool, a string resource or TTN_GETDISPINFO. Tracking
// tooltips (TTF_TRACK) are placed by the application. Balloons, titles,
// TTS_CLOSE and links (TTF_PARSELINKS, TTN_LINKCLICK) are drawn too.
#include "QtWin.h"

#include <QtGui/QFontMetrics>
#include <QtGui/QPainter>
#include <QtGui/QPainterPath>
#include <QtWidgets/QApplication>
#include <QtWidgets/QStyle>
#include <QtWidgets/QStyleOption>

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace linopenmpt::qtwin
{

namespace
{

constexpr UINT_PTR ShowTimer = 1;
constexpr UINT_PTR PopTimer = 2;
constexpr UINT_PTR LeaveTimer = 3;
constexpr int BalloonTail = 0;   // no stem; comctl32 6 draws one, Qt's tooltips don't
constexpr int CloseSize = 14;

struct Tool
{
	UINT flags = 0;
	HWND hwnd = nullptr;
	UINT_PTR id = 0;
	RECT rect{};
	HINSTANCE hinst = nullptr;
	std::wstring text;
	bool callback = false;
	UINT resource = 0;
	LPARAM param = 0;
	bool subclassed = false;
};

// A run of the text: plain, or a link.
struct Run
{
	std::wstring text;
	bool link = false;
};

struct TipState : ControlState
{
	std::vector<Tool> tools;
	bool active = true;
	int current = -1;         // the tool under the mouse
	bool shown = false;
	int tracking = -1;        // the tool of TTM_TRACKACTIVATE
	POINT trackPos{};
	UINT initial = 0, autopop = 0, reshow = 0; // 0: automatic
	int maxWidth = -1;
	RECT margin{};
	COLORREF bk = CLR_DEFAULT;
	COLORREF textColor = CLR_DEFAULT;
	std::wstring title;
	UINT icon = TTI_NONE;
	std::wstring shownText;
	std::vector<std::vector<Run>> lines; // shownText, laid out
	std::vector<QRect> linkRects;
	bool reshowFast = false;
	POINT lastPos{};          // screen position of the last relayed move
	bool closeHot = false;
};

// Windows subclassed by TTF_SUBCLASS tools: the tooltips relaying for each.
struct SubclassLink
{
	WNDPROC old = nullptr;
	std::vector<HWND> tips;
};
std::map<HWND, SubclassLink> &Subclassed()
{
	static std::map<HWND, SubclassLink> links;
	return links;
}

LONG_PTR StyleOf(HWND hwnd)
{
	return ::GetWindowLongPtr(hwnd, GWL_STYLE);
}

UINT Delay(const TipState &s, int which)
{
	const UINT base = ::GetDoubleClickTime();
	switch(which)
	{
	case TTDT_INITIAL: return s.initial ? s.initial : base;
	case TTDT_AUTOPOP: return s.autopop ? s.autopop : base * 10;
	case TTDT_RESHOW: return s.reshow ? s.reshow : base / 5;
	default: return base;
	}
}

HWND ToolWindow(const Tool &t)
{
	return (t.flags & TTF_IDISHWND) ? reinterpret_cast<HWND>(t.id) : t.hwnd;
}

bool SameTool(const Tool &t, HWND hwnd, UINT_PTR id)
{
	return t.hwnd == hwnd && t.id == id;
}

int FindTool(const TipState &s, const TOOLINFO *ti)
{
	if(ti == nullptr)
		return -1;
	for(std::size_t i = 0; i < s.tools.size(); i++)
	{
		if(SameTool(s.tools[i], ti->hwnd, ti->uId))
			return static_cast<int>(i);
	}
	return -1;
}

// The tool at a point of a window (client coordinates).
int ToolAt(const TipState &s, HWND hwnd, POINT pt)
{
	for(std::size_t i = 0; i < s.tools.size(); i++)
	{
		const Tool &t = s.tools[i];
		if(t.flags & TTF_IDISHWND)
		{
			if(reinterpret_cast<HWND>(t.id) == hwnd)
				return static_cast<int>(i);
		} else if(t.hwnd == hwnd && ::PtInRect(&t.rect, pt))
		{
			return static_cast<int>(i);
		}
	}
	return -1;
}

void CopyIn(Tool &t, const TOOLINFO *ti, bool withText)
{
	t.flags = ti->uFlags;
	t.hwnd = ti->hwnd;
	t.id = ti->uId;
	t.rect = ti->rect;
	t.hinst = ti->hinst;
	if(ti->cbSize > offsetof(TOOLINFO, lParam))
		t.param = ti->lParam;
	if(withText)
	{
		t.callback = ti->lpszText == LPSTR_TEXTCALLBACK;
		t.resource = 0;
		t.text.clear();
		if(!t.callback && ti->lpszText != nullptr)
		{
			if(IS_INTRESOURCE(ti->lpszText))
				t.resource = static_cast<UINT>(reinterpret_cast<UINT_PTR>(ti->lpszText));
			else
				t.text = ti->lpszText;
		}
	}
}

void CopyOut(const Tool &t, TOOLINFO *ti)
{
	ti->uFlags = t.flags;
	ti->hwnd = t.hwnd;
	ti->uId = t.id;
	ti->rect = t.rect;
	ti->hinst = t.hinst;
	if(ti->cbSize > offsetof(TOOLINFO, lParam))
		ti->lParam = t.param;
	if(ti->lpszText != nullptr && ti->lpszText != LPSTR_TEXTCALLBACK && !IS_INTRESOURCE(ti->lpszText))
	{
		// comctl32 copies into the caller's buffer, which it trusts to be big
		// enough (80 characters is the documented size).
		const std::size_t n = std::min<std::size_t>(t.text.size(), 79);
		std::copy_n(t.text.begin(), n, ti->lpszText);
		ti->lpszText[n] = 0;
	} else if(t.callback)
	{
		ti->lpszText = LPSTR_TEXTCALLBACK;
	}
}

LRESULT NotifyTool(HWND hwnd, const Tool &t, NMHDR &hdr, UINT code)
{
	hdr.hwndFrom = hwnd;
	hdr.idFrom = t.id;
	hdr.code = code;
	return t.hwnd != nullptr ? ::SendMessage(t.hwnd, WM_NOTIFY, t.id, reinterpret_cast<LPARAM>(&hdr)) : 0;
}

std::wstring ToolText(HWND hwnd, int index)
{
	TipState *s = Control<TipState>(hwnd);
	if(s == nullptr || index < 0 || index >= static_cast<int>(s->tools.size()))
		return std::wstring();
	Tool t = s->tools[static_cast<std::size_t>(index)];
	if(t.resource != 0)
	{
		wchar_t buffer[1024] = {};
		::LoadString(t.hinst, t.resource, buffer, 1024);
		return buffer;
	}
	if(!t.callback)
		return t.text;
	NMTTDISPINFO di{};
	di.uFlags = t.flags;
	di.lParam = t.param;
	di.hinst = nullptr;
	di.lpszText = di.szText;
	NotifyTool(hwnd, t, di.hdr, TTN_GETDISPINFO);
	std::wstring text;
	if(di.lpszText == nullptr || di.lpszText == LPSTR_TEXTCALLBACK)
	{
		text.clear();
	} else if(IS_INTRESOURCE(di.lpszText))
	{
		wchar_t buffer[1024] = {};
		::LoadString(di.hinst, static_cast<UINT>(reinterpret_cast<UINT_PTR>(di.lpszText)), buffer, 1024);
		text = buffer;
	} else
	{
		text = di.lpszText;
	}
	if((di.uFlags & TTF_DI_SETITEM) && (s = Control<TipState>(hwnd)) != nullptr && index < static_cast<int>(s->tools.size()))
	{
		Tool &live = s->tools[static_cast<std::size_t>(index)];
		live.callback = false;
		live.text = text;
	}
	return text;
}

// Splits the text into lines of runs; with links, <a>...</a> marks them.
void Parse(TipState &s, const std::wstring &text, bool links)
{
	s.lines.assign(1, {});
	std::wstring current;
	bool inLink = false;
	auto flush = [&]() {
		if(!current.empty())
			s.lines.back().push_back(Run{current, inLink});
		current.clear();
	};
	for(std::size_t i = 0; i < text.size(); i++)
	{
		const wchar_t c = text[i];
		if(links && c == L'<')
		{
			const std::size_t close = text.find(L'>', i);
			if(close != std::wstring::npos)
			{
				const QString tag = ToQString(text.substr(i + 1, close - i - 1).c_str()).trimmed().toLower();
				if(tag == QStringLiteral("a") || tag.startsWith(QStringLiteral("a ")))
				{
					flush();
					inLink = true;
					i = close;
					continue;
				}
				if(tag == QStringLiteral("/a"))
				{
					flush();
					inLink = false;
					i = close;
					continue;
				}
			}
		}
		if(c == L'\r')
			continue;
		if(c == L'\n')
		{
			flush();
			s.lines.emplace_back();
			continue;
		}
		current.push_back(c == L'\t' ? L' ' : c);
	}
	flush();
}

QFont FontOf(HWND hwnd)
{
	Win32Widget *w = FromHandle(hwnd);
	return w != nullptr ? w->font() : QApplication::font("QToolTip");
}

int Padding(HWND hwnd)
{
	return (StyleOf(hwnd) & TTS_BALLOON) ? 8 : 4;
}

// Lays the shown text out, wrapping at the maximum width, and returns the
// window's size.
QSize Layout(HWND hwnd, TipState &s)
{
	const QFont font = FontOf(hwnd);
	const QFontMetrics fm(font);
	const bool noPrefix = (StyleOf(hwnd) & TTS_NOPREFIX) != 0;
	// Single ampersands are mnemonics unless TTS_NOPREFIX.
	auto strip = [noPrefix](std::wstring t) {
		if(noPrefix)
			return t;
		std::wstring out;
		for(std::size_t i = 0; i < t.size(); i++)
		{
			if(t[i] == L'&' && i + 1 < t.size())
				i++;
			out.push_back(t[i]);
		}
		return out;
	};
	const int limit = s.maxWidth >= 0 ? s.maxWidth : -1;
	// Wrap by words when a maximum width is set.
	std::vector<std::vector<Run>> wrapped;
	for(const auto &line : s.lines)
	{
		std::vector<Run> out;
		int x = 0;
		for(const Run &run : line)
		{
			const std::wstring text = strip(run.text);
			if(limit < 0)
			{
				out.push_back(Run{text, run.link});
				continue;
			}
			std::wstring piece;
			std::size_t i = 0;
			while(i < text.size())
			{
				std::size_t end = text.find(L' ', i);
				end = end == std::wstring::npos ? text.size() : end + 1;
				const std::wstring word = text.substr(i, end - i);
				const int ww = fm.horizontalAdvance(ToQString((piece + word).c_str()));
				if(x + ww > limit && (x > 0 || !piece.empty()))
				{
					if(!piece.empty())
						out.push_back(Run{piece, run.link});
					wrapped.push_back(out);
					out.clear();
					piece.clear();
					x = 0;
					std::wstring trimmed = word;
					while(!trimmed.empty() && trimmed.front() == L' ')
						trimmed.erase(trimmed.begin());
					piece = trimmed;
				} else
				{
					piece += word;
				}
				i = end;
			}
			if(!piece.empty())
			{
				x += fm.horizontalAdvance(ToQString(piece.c_str()));
				out.push_back(Run{piece, run.link});
			}
		}
		wrapped.push_back(out);
	}
	s.lines = wrapped;
	int width = 0;
	for(const auto &line : s.lines)
	{
		int w = 0;
		for(const Run &run : line)
			w += fm.horizontalAdvance(ToQString(run.text.c_str()));
		width = std::max(width, w);
	}
	int height = static_cast<int>(s.lines.size()) * fm.height();
	if(!s.title.empty())
	{
		QFont bold = font;
		bold.setBold(true);
		const QFontMetrics bm(bold);
		int titleWidth = bm.horizontalAdvance(ToQString(s.title.c_str())) + (s.icon != TTI_NONE ? 20 : 0);
		if(StyleOf(hwnd) & TTS_CLOSE)
			titleWidth += CloseSize + 8;
		width = std::max(width, titleWidth);
		height += std::max(bm.height(), 16) + 6;
	} else if(StyleOf(hwnd) & TTS_CLOSE)
	{
		width += CloseSize + 8;
	}
	const int pad = Padding(hwnd);
	return QSize(width + 2 * pad + s.margin.left + s.margin.right, height + 2 * pad + s.margin.top + s.margin.bottom + BalloonTail);
}

QRect CloseRect(HWND hwnd)
{
	RECT rc{};
	::GetClientRect(hwnd, &rc);
	const int pad = Padding(hwnd);
	return QRect(rc.right - pad - CloseSize, pad, CloseSize, CloseSize);
}

void Hide(HWND hwnd, bool notify, UINT_PTR fromId = 0, bool useId = false)
{
	TipState *s = Control<TipState>(hwnd);
	if(s == nullptr)
		return;
	::KillTimer(hwnd, PopTimer);
	::KillTimer(hwnd, LeaveTimer);
	if(!s->shown)
		return;
	s->shown = false;
	::ShowWindow(hwnd, SW_HIDE);
	const int index = s->tracking >= 0 ? s->tracking : s->current;
	if(notify && index >= 0 && index < static_cast<int>(s->tools.size()))
	{
		Tool t = s->tools[static_cast<std::size_t>(index)];
		if(useId)
			t.id = fromId;
		NMHDR hdr{};
		NotifyTool(hwnd, t, hdr, TTN_POP);
	}
}

// Shows the tooltip for a tool; at the cursor, or for a tracking tool where
// TTM_TRACKPOSITION put it.
void Show(HWND hwnd, int index)
{
	TipState *s = Control<TipState>(hwnd);
	if(s == nullptr || index < 0 || index >= static_cast<int>(s->tools.size()))
		return;
	const std::wstring text = ToolText(hwnd, index);
	if((s = Control<TipState>(hwnd)) == nullptr || index >= static_cast<int>(s->tools.size()))
		return;
	if(text.empty() && s->title.empty())
	{
		Hide(hwnd, true);
		return;
	}
	const Tool tool = s->tools[static_cast<std::size_t>(index)];
	s->shownText = text;
	Parse(*s, text, (tool.flags & TTF_PARSELINKS) != 0);
	const QSize size = Layout(hwnd, *s);
	// Placement: tracking tools at their position; TTF_CENTERTIP below the
	// tool's middle; otherwise below the cursor.
	POINT at{};
	if(tool.flags & TTF_TRACK)
	{
		at = s->trackPos;
		if(!(tool.flags & TTF_ABSOLUTE))
		{
			at.x += 0;
			at.y += 0;
		}
	} else if(tool.flags & TTF_CENTERTIP)
	{
		RECT r{};
		if(tool.flags & TTF_IDISHWND)
		{
			::GetWindowRect(reinterpret_cast<HWND>(tool.id), &r);
		} else
		{
			r = tool.rect;
			::MapWindowPoints(tool.hwnd, nullptr, reinterpret_cast<POINT *>(&r), 2);
		}
		at = POINT{(r.left + r.right) / 2 - size.width() / 2, r.bottom + 2};
	} else
	{
		::GetCursorPos(&at);
		at.y += ::GetSystemMetrics(SM_CYCURSOR) * 2 / 3;
	}
	// Keep it on the screen of the point.
	RECT work{};
	::SystemParametersInfo(SPI_GETWORKAREA, 0, &work, 0);
	if(work.right > work.left)
	{
		at.x = std::clamp(static_cast<int>(at.x), static_cast<int>(work.left), std::max(static_cast<int>(work.left), static_cast<int>(work.right) - size.width()));
		at.y = std::clamp(static_cast<int>(at.y), static_cast<int>(work.top), std::max(static_cast<int>(work.top), static_cast<int>(work.bottom) - size.height()));
	}
	::SetWindowPos(hwnd, HWND_TOPMOST, at.x, at.y, size.width(), size.height(), SWP_NOACTIVATE);
	// TTN_SHOW: nonzero means the owner moved the window itself.
	NMHDR hdr{};
	NotifyTool(hwnd, tool, hdr, TTN_SHOW);
	if((s = Control<TipState>(hwnd)) == nullptr)
		return;
	s->shown = true;
	::ShowWindow(hwnd, SW_SHOWNA);
	::InvalidateRect(hwnd, nullptr, TRUE);
	if(!(tool.flags & TTF_TRACK))
	{
		if(!(StyleOf(hwnd) & TTS_BALLOON) || !(StyleOf(hwnd) & TTS_CLOSE))
			::SetTimer(hwnd, PopTimer, Delay(*s, TTDT_AUTOPOP), nullptr);
		::SetTimer(hwnd, LeaveTimer, 100, nullptr);
	}
}

// Whether the cursor is still on the current tool.
bool CursorOnTool(HWND hwnd, const TipState &s)
{
	if(s.current < 0 || s.current >= static_cast<int>(s.tools.size()))
		return false;
	const Tool &t = s.tools[static_cast<std::size_t>(s.current)];
	POINT pt{};
	::GetCursorPos(&pt);
	RECT r{};
	if(t.flags & TTF_IDISHWND)
	{
		::GetWindowRect(reinterpret_cast<HWND>(t.id), &r);
	} else
	{
		r = t.rect;
		::MapWindowPoints(t.hwnd, nullptr, reinterpret_cast<POINT *>(&r), 2);
	}
	(void)hwnd;
	return ::PtInRect(&r, pt) != FALSE;
}

void Relay(HWND hwnd, const MSG *msg)
{
	TipState *s = Control<TipState>(hwnd);
	if(s == nullptr || msg == nullptr)
		return;
	switch(msg->message)
	{
	case WM_MOUSEMOVE:
		{
			POINT pt{GET_X_LPARAM(msg->lParam), GET_Y_LPARAM(msg->lParam)};
			POINT screen = pt;
			::ClientToScreen(msg->hwnd, &screen);
			s->lastPos = screen;
			const int tool = s->active ? ToolAt(*s, msg->hwnd, pt) : -1;
			if(tool == s->current)
			{
				// Moving on the same tool restarts the wait while hidden.
				if(!s->shown && tool >= 0)
					::SetTimer(hwnd, ShowTimer, Delay(*s, TTDT_INITIAL), nullptr);
				return;
			}
			const bool wasShown = s->shown;
			if(wasShown)
				Hide(hwnd, true);
			if((s = Control<TipState>(hwnd)) == nullptr)
				return;
			s->current = tool;
			::KillTimer(hwnd, ShowTimer);
			if(tool >= 0)
				::SetTimer(hwnd, ShowTimer, wasShown || (StyleOf(hwnd) & TTS_ALWAYSTIP && s->reshowFast) ? Delay(*s, TTDT_RESHOW) : Delay(*s, TTDT_INITIAL), nullptr);
			s->reshowFast = wasShown;
		}
		break;
	case WM_LBUTTONDOWN:
	case WM_LBUTTONUP:
	case WM_MBUTTONDOWN:
	case WM_MBUTTONUP:
	case WM_RBUTTONDOWN:
	case WM_RBUTTONUP:
	case WM_NCLBUTTONDOWN:
	case WM_NCMBUTTONDOWN:
	case WM_NCRBUTTONDOWN:
		// A click puts the tip away until the mouse moves to another tool.
		::KillTimer(hwnd, ShowTimer);
		Hide(hwnd, true);
		break;
	default:
		break;
	}
}

LRESULT CALLBACK ToolSubclass(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
	auto &links = Subclassed();
	const auto it = links.find(hwnd);
	if(it == links.end())
		return ::DefWindowProc(hwnd, msg, wp, lp);
	const WNDPROC old = it->second.old;
	if((msg >= WM_MOUSEFIRST && msg <= WM_MOUSELAST) || msg == WM_NCMOUSEMOVE)
	{
		MSG m{};
		m.hwnd = hwnd;
		m.message = msg;
		m.wParam = wp;
		m.lParam = lp;
		const std::vector<HWND> tips = it->second.tips;
		for(HWND tip : tips)
			Relay(tip, &m);
	}
	if(msg == WM_NCDESTROY)
	{
		const LRESULT r = ::CallWindowProc(old, hwnd, msg, wp, lp);
		Subclassed().erase(hwnd);
		return r;
	}
	return ::CallWindowProc(old, hwnd, msg, wp, lp);
}

void Subclass(HWND tip, HWND target)
{
	if(target == nullptr || !::IsWindow(target))
		return;
	auto &links = Subclassed();
	auto it = links.find(target);
	if(it == links.end())
	{
		SubclassLink link;
		link.old = reinterpret_cast<WNDPROC>(::GetWindowLongPtr(target, GWLP_WNDPROC));
		link.tips.push_back(tip);
		links.emplace(target, link);
		::SetWindowLongPtr(target, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&ToolSubclass));
	} else if(std::find(it->second.tips.begin(), it->second.tips.end(), tip) == it->second.tips.end())
	{
		it->second.tips.push_back(tip);
	}
}

void Unsubclass(HWND tip, HWND target)
{
	auto &links = Subclassed();
	auto it = links.find(target);
	if(it == links.end())
		return;
	auto &tips = it->second.tips;
	tips.erase(std::remove(tips.begin(), tips.end(), tip), tips.end());
	// The subclass stays while other tooltips use it; once none do, the old
	// procedure goes back if nobody subclassed after us.
	if(tips.empty())
	{
		if(::GetWindowLongPtr(target, GWLP_WNDPROC) == reinterpret_cast<LONG_PTR>(&ToolSubclass))
		{
			::SetWindowLongPtr(target, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(it->second.old));
			links.erase(it);
		}
	}
}

void RemoveTool(HWND hwnd, TipState &s, int index)
{
	const Tool t = s.tools[static_cast<std::size_t>(index)];
	if(index == s.current || index == s.tracking)
	{
		Hide(hwnd, false);
		if(Control<TipState>(hwnd) == nullptr)
			return;
	}
	if(t.subclassed)
	{
		bool otherUses = false;
		for(std::size_t i = 0; i < s.tools.size(); i++)
		{
			if(static_cast<int>(i) != index && s.tools[i].subclassed && ToolWindow(s.tools[i]) == ToolWindow(t))
				otherUses = true;
		}
		if(!otherUses)
			Unsubclass(hwnd, ToolWindow(t));
	}
	s.tools.erase(s.tools.begin() + index);
	auto fix = [index](int &i) {
		if(i == index)
			i = -1;
		else if(i > index)
			i--;
	};
	fix(s.current);
	fix(s.tracking);
}

void Paint(HWND hwnd, HDC dc)
{
	TipState *s = Control<TipState>(hwnd);
	Win32Widget *w = FromHandle(hwnd);
	QPainter *p = DCPainter(dc);
	if(s == nullptr || w == nullptr || p == nullptr)
		return;
	RECT rc{};
	::GetClientRect(hwnd, &rc);
	const QRect r(0, 0, rc.right, rc.bottom);
	const bool balloon = (StyleOf(hwnd) & TTS_BALLOON) != 0;
	const QColor bk = ToQColor(s->bk == CLR_DEFAULT ? ::GetSysColor(COLOR_INFOBK) : s->bk);
	const QColor fg = ToQColor(s->textColor == CLR_DEFAULT ? ::GetSysColor(COLOR_INFOTEXT) : s->textColor);
	p->save();
	if(balloon)
	{
		p->setRenderHint(QPainter::Antialiasing);
		QPainterPath path;
		path.addRoundedRect(QRectF(r).adjusted(0.5, 0.5, -0.5, -0.5), 6, 6);
		p->fillPath(path, bk);
		p->setPen(ToQColor(::GetSysColor(COLOR_WINDOWFRAME)));
		p->drawPath(path);
	} else
	{
		QStyleOptionFrame opt;
		opt.initFrom(w);
		opt.rect = r;
		opt.palette.setColor(QPalette::ToolTipBase, bk);
		opt.palette.setColor(QPalette::ToolTipText, fg);
		opt.palette.setColor(QPalette::Window, bk);
		p->fillRect(r, bk);
		QApplication::style()->drawPrimitive(QStyle::PE_PanelTipLabel, &opt, p, w);
	}
	p->restore();

	p->save();
	const QFont font = FontOf(hwnd);
	const QFontMetrics fm(font);
	const int pad = Padding(hwnd);
	int y = pad + s->margin.top;
	const int left = pad + s->margin.left;
	if(!s->title.empty())
	{
		QFont bold = font;
		bold.setBold(true);
		const QFontMetrics bm(bold);
		const int lineHeight = std::max(bm.height(), 16);
		int x = left;
		if(s->icon != TTI_NONE)
		{
			QStyle::StandardPixmap sp = s->icon == TTI_ERROR ? QStyle::SP_MessageBoxCritical : (s->icon == TTI_WARNING ? QStyle::SP_MessageBoxWarning : QStyle::SP_MessageBoxInformation);
			const QIcon icon = QApplication::style()->standardIcon(sp, nullptr, w);
			icon.paint(p, QRect(x, y + (lineHeight - 16) / 2, 16, 16));
			x += 20;
		}
		p->setFont(bold);
		p->setPen(fg);
		p->drawText(QRect(x, y, r.width() - x, lineHeight), Qt::AlignLeft | Qt::AlignVCenter, ToQString(s->title.c_str()));
		y += lineHeight + 6;
	}
	if(StyleOf(hwnd) & TTS_CLOSE)
	{
		const QRect cr = CloseRect(hwnd);
		QPen pen(s->closeHot ? ToQColor(::GetSysColor(COLOR_HOTLIGHT)) : fg);
		pen.setWidthF(1.5);
		p->setPen(pen);
		p->setRenderHint(QPainter::Antialiasing);
		p->drawLine(cr.topLeft() + QPoint(3, 3), cr.bottomRight() - QPoint(3, 3));
		p->drawLine(QPoint(cr.left() + 3, cr.bottom() - 3), QPoint(cr.right() - 3, cr.top() + 3));
	}
	p->setFont(font);
	s->linkRects.clear();
	for(const auto &line : s->lines)
	{
		int x = left;
		for(const Run &run : line)
		{
			const QString text = ToQString(run.text.c_str());
			const int width = fm.horizontalAdvance(text);
			if(run.link)
			{
				QFont linkFont = font;
				linkFont.setUnderline(true);
				p->setFont(linkFont);
				p->setPen(ToQColor(::GetSysColor(COLOR_HOTLIGHT)));
				s->linkRects.push_back(QRect(x, y, width, fm.height()));
			} else
			{
				p->setFont(font);
				p->setPen(fg);
			}
			p->drawText(QPoint(x, y + fm.ascent()), text);
			x += width;
		}
		y += fm.height();
	}
	p->restore();
}

LRESULT CALLBACK TipProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
	TipState *s = Control<TipState>(hwnd);
	switch(msg)
	{
	case WM_NCCREATE:
		if(WindowData *d = Data(hwnd))
			d->control = std::make_unique<TipState>();
		// comctl32 makes every tooltip a borderless popup.
		::SetWindowLongPtr(hwnd, GWL_STYLE, (::GetWindowLongPtr(hwnd, GWL_STYLE) & ~(WS_BORDER | WS_CAPTION | WS_CHILD)) | WS_POPUP);
		return ::DefWindowProc(hwnd, msg, wp, lp);
	case WM_DESTROY:
		if(s != nullptr)
		{
			while(!s->tools.empty())
			{
				RemoveTool(hwnd, *s, static_cast<int>(s->tools.size()) - 1);
				if((s = Control<TipState>(hwnd)) == nullptr)
					break;
			}
		}
		return ::DefWindowProc(hwnd, msg, wp, lp);
	case WM_MOUSEACTIVATE:
		return MA_NOACTIVATE;
	case WM_ERASEBKGND:
		return 1;
	case WM_PAINT:
	case WM_PRINTCLIENT:
		if(msg == WM_PRINTCLIENT)
		{
			Paint(hwnd, reinterpret_cast<HDC>(wp));
		} else
		{
			PAINTSTRUCT ps;
			HDC dc = ::BeginPaint(hwnd, &ps);
			if(dc != nullptr)
				Paint(hwnd, dc);
			::EndPaint(hwnd, &ps);
		}
		return 0;
	case WM_TIMER:
		if(s == nullptr)
			return 0;
		switch(wp)
		{
		case ShowTimer:
			::KillTimer(hwnd, ShowTimer);
			if(s->active && s->current >= 0 && s->tracking < 0)
				Show(hwnd, s->current);
			return 0;
		case PopTimer:
			::KillTimer(hwnd, PopTimer);
			Hide(hwnd, true);
			return 0;
		case LeaveTimer:
			if(s->shown && s->tracking < 0 && !CursorOnTool(hwnd, *s))
			{
				Hide(hwnd, true);
				if((s = Control<TipState>(hwnd)) != nullptr)
					s->current = -1;
			}
			return 0;
		default:
			return ::DefWindowProc(hwnd, msg, wp, lp);
		}
	case WM_MOUSEMOVE:
		if(s != nullptr)
		{
			// Plain tips get out of the way; balloons with a close button or
			// links stay to be clicked.
			const bool interactive = (StyleOf(hwnd) & TTS_CLOSE) || !s->linkRects.empty() || s->tracking >= 0;
			if(!interactive)
			{
				Hide(hwnd, true);
				return 0;
			}
			const QPoint pt(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
			const bool hot = (StyleOf(hwnd) & TTS_CLOSE) && CloseRect(hwnd).contains(pt);
			if(hot != s->closeHot)
			{
				s->closeHot = hot;
				::InvalidateRect(hwnd, nullptr, TRUE);
			}
			bool overLink = false;
			for(const QRect &lr : s->linkRects)
				overLink = overLink || lr.contains(pt);
			::SetCursor(::LoadCursor(nullptr, overLink ? IDC_HAND : IDC_ARROW));
		}
		return 0;
	case WM_LBUTTONUP:
		if(s != nullptr)
		{
			const QPoint pt(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
			const int index = s->tracking >= 0 ? s->tracking : s->current;
			if((StyleOf(hwnd) & TTS_CLOSE) && CloseRect(hwnd).contains(pt))
			{
				// The close button: TTN_POP with an id of -1.
				Hide(hwnd, true, static_cast<UINT_PTR>(-1), true);
				if((s = Control<TipState>(hwnd)) != nullptr)
					s->tracking = -1;
				return 0;
			}
			for(const QRect &lr : s->linkRects)
			{
				if(lr.contains(pt) && index >= 0 && index < static_cast<int>(s->tools.size()))
				{
					NMHDR hdr{};
					NotifyTool(hwnd, s->tools[static_cast<std::size_t>(index)], hdr, TTN_LINKCLICK);
					return 0;
				}
			}
		}
		return 0;
	case WM_SETFONT:
		::DefWindowProc(hwnd, msg, wp, lp);
		if(s != nullptr && s->shown)
			Show(hwnd, s->tracking >= 0 ? s->tracking : s->current);
		return 0;

	case TTM_ACTIVATE:
		if(s != nullptr)
		{
			s->active = wp != FALSE;
			if(!s->active)
			{
				::KillTimer(hwnd, ShowTimer);
				Hide(hwnd, true);
				if((s = Control<TipState>(hwnd)) != nullptr)
					s->current = -1;
			}
		}
		return 0;
	case TTM_ADDTOOL:
		if(s != nullptr && lp != 0)
		{
			const auto *ti = reinterpret_cast<const TOOLINFO *>(lp);
			Tool t;
			CopyIn(t, ti, true);
			if(t.flags & TTF_SUBCLASS)
			{
				Subclass(hwnd, ToolWindow(t));
				t.subclassed = true;
			}
			if((s = Control<TipState>(hwnd)) == nullptr)
				return FALSE;
			s->tools.push_back(t);
			return TRUE;
		}
		return FALSE;
	case TTM_DELTOOL:
		if(s != nullptr)
		{
			const int index = FindTool(*s, reinterpret_cast<const TOOLINFO *>(lp));
			if(index >= 0)
				RemoveTool(hwnd, *s, index);
		}
		return 0;
	case TTM_NEWTOOLRECT:
		if(s != nullptr)
		{
			const auto *ti = reinterpret_cast<const TOOLINFO *>(lp);
			const int index = FindTool(*s, ti);
			if(index >= 0)
				s->tools[static_cast<std::size_t>(index)].rect = ti->rect;
		}
		return 0;
	case TTM_GETTOOLINFO:
		if(s != nullptr && lp != 0)
		{
			auto *ti = reinterpret_cast<TOOLINFO *>(lp);
			const int index = FindTool(*s, ti);
			if(index < 0)
				return FALSE;
			CopyOut(s->tools[static_cast<std::size_t>(index)], ti);
			return TRUE;
		}
		return FALSE;
	case TTM_SETTOOLINFO:
		if(s != nullptr && lp != 0)
		{
			const auto *ti = reinterpret_cast<const TOOLINFO *>(lp);
			const int index = FindTool(*s, ti);
			if(index >= 0)
			{
				Tool &t = s->tools[static_cast<std::size_t>(index)];
				const bool subclassed = t.subclassed;
				CopyIn(t, ti, true);
				t.subclassed = subclassed;
			}
		}
		return 0;
	case TTM_UPDATETIPTEXT:
		if(s != nullptr && lp != 0)
		{
			const auto *ti = reinterpret_cast<const TOOLINFO *>(lp);
			const int index = FindTool(*s, ti);
			if(index >= 0)
			{
				Tool &t = s->tools[static_cast<std::size_t>(index)];
				t.hinst = ti->hinst;
				t.callback = ti->lpszText == LPSTR_TEXTCALLBACK;
				t.resource = 0;
				t.text.clear();
				if(!t.callback && ti->lpszText != nullptr)
				{
					if(IS_INTRESOURCE(ti->lpszText))
						t.resource = static_cast<UINT>(reinterpret_cast<UINT_PTR>(ti->lpszText));
					else
						t.text = ti->lpszText;
				}
				if(s->shown && (index == s->current || index == s->tracking))
					Show(hwnd, index);
			}
		}
		return 0;
	case TTM_GETTEXT:
		if(s != nullptr && lp != 0)
		{
			auto *ti = reinterpret_cast<TOOLINFO *>(lp);
			const int index = FindTool(*s, ti);
			if(index >= 0 && ti->lpszText != nullptr)
			{
				const std::wstring text = ToolText(hwnd, index);
				const std::size_t max = wp != 0 ? static_cast<std::size_t>(wp) : 80;
				const std::size_t n = std::min(text.size(), max - 1);
				std::copy_n(text.begin(), n, ti->lpszText);
				ti->lpszText[n] = 0;
			}
		}
		return 0;
	case TTM_GETTOOLCOUNT:
		return s ? static_cast<LRESULT>(s->tools.size()) : 0;
	case TTM_ENUMTOOLS:
		if(s != nullptr && lp != 0 && wp < s->tools.size())
		{
			CopyOut(s->tools[wp], reinterpret_cast<TOOLINFO *>(lp));
			return TRUE;
		}
		return FALSE;
	case TTM_GETCURRENTTOOL:
		if(s != nullptr)
		{
			const int index = s->tracking >= 0 ? s->tracking : (s->shown ? s->current : -1);
			if(index < 0)
				return FALSE;
			if(lp != 0)
				CopyOut(s->tools[static_cast<std::size_t>(index)], reinterpret_cast<TOOLINFO *>(lp));
			return TRUE;
		}
		return FALSE;
	case TTM_HITTEST:
		if(s != nullptr && lp != 0)
		{
			auto *hit = reinterpret_cast<TTHITTESTINFO *>(lp);
			const int index = ToolAt(*s, hit->hwnd, hit->pt);
			if(index < 0)
				return FALSE;
			if(hit->ti.cbSize >= TTTOOLINFO_V1_SIZE)
				CopyOut(s->tools[static_cast<std::size_t>(index)], &hit->ti);
			return TRUE;
		}
		return FALSE;
	case TTM_WINDOWFROMPOINT:
		return lp != 0 ? reinterpret_cast<LRESULT>(::WindowFromPoint(*reinterpret_cast<const POINT *>(lp))) : 0;
	case TTM_RELAYEVENT:
		Relay(hwnd, reinterpret_cast<const MSG *>(lp));
		return 0;
	case TTM_TRACKACTIVATE:
		if(s != nullptr)
		{
			if(wp)
			{
				const int index = FindTool(*s, reinterpret_cast<const TOOLINFO *>(lp));
				if(index < 0)
					return 0;
				s->tracking = index;
				Show(hwnd, index);
			} else
			{
				Hide(hwnd, true);
				if((s = Control<TipState>(hwnd)) != nullptr)
					s->tracking = -1;
			}
		}
		return 0;
	case TTM_TRACKPOSITION:
		if(s != nullptr)
		{
			s->trackPos = POINT{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
			if(s->shown && s->tracking >= 0)
			{
				RECT r{};
				::GetWindowRect(hwnd, &r);
				::SetWindowPos(hwnd, HWND_TOPMOST, s->trackPos.x, s->trackPos.y, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE);
			}
		}
		return 0;
	case TTM_SETDELAYTIME:
		if(s != nullptr)
		{
			const int value = static_cast<short>(LOWORD(lp));
			const UINT v = value < 0 ? 0 : static_cast<UINT>(value);
			switch(wp)
			{
			case TTDT_AUTOMATIC:
				// comctl32: the initial delay; the others follow from it.
				s->initial = v;
				s->autopop = v * 10;
				s->reshow = v / 5;
				break;
			case TTDT_INITIAL: s->initial = v; break;
			case TTDT_AUTOPOP: s->autopop = v; break;
			case TTDT_RESHOW: s->reshow = v; break;
			default: break;
			}
		}
		return 0;
	case TTM_GETDELAYTIME:
		return s ? Delay(*s, static_cast<int>(wp)) : 0;
	case TTM_SETTIPBKCOLOR:
	case TTM_SETTIPTEXTCOLOR:
		if(s != nullptr)
		{
			(msg == TTM_SETTIPBKCOLOR ? s->bk : s->textColor) = static_cast<COLORREF>(wp);
			::InvalidateRect(hwnd, nullptr, TRUE);
		}
		return 0;
	case TTM_GETTIPBKCOLOR:
		return s ? static_cast<LRESULT>(s->bk == CLR_DEFAULT ? ::GetSysColor(COLOR_INFOBK) : s->bk) : 0;
	case TTM_GETTIPTEXTCOLOR:
		return s ? static_cast<LRESULT>(s->textColor == CLR_DEFAULT ? ::GetSysColor(COLOR_INFOTEXT) : s->textColor) : 0;
	case TTM_SETMAXTIPWIDTH:
		if(s != nullptr)
		{
			const int old = s->maxWidth;
			s->maxWidth = static_cast<int>(lp);
			return old;
		}
		return -1;
	case TTM_GETMAXTIPWIDTH:
		return s ? s->maxWidth : -1;
	case TTM_SETMARGIN:
		if(s != nullptr && lp != 0)
			s->margin = *reinterpret_cast<const RECT *>(lp);
		return 0;
	case TTM_GETMARGIN:
		if(s != nullptr && lp != 0)
			*reinterpret_cast<RECT *>(lp) = s->margin;
		return 0;
	case TTM_POP:
		if(s != nullptr)
		{
			::KillTimer(hwnd, ShowTimer);
			Hide(hwnd, true);
		}
		return 0;
	case TTM_POPUP:
		if(s != nullptr)
		{
			int index = s->current;
			if(index < 0 && !s->tools.empty())
				index = 0;
			if(index >= 0)
			{
				s->current = index;
				Show(hwnd, index);
			}
		}
		return 0;
	case TTM_UPDATE:
		if(s != nullptr && s->shown)
			Show(hwnd, s->tracking >= 0 ? s->tracking : s->current);
		return 0;
	case TTM_GETBUBBLESIZE:
		if(s != nullptr && lp != 0)
		{
			const int index = FindTool(*s, reinterpret_cast<const TOOLINFO *>(lp));
			if(index < 0)
				return 0;
			const std::wstring text = ToolText(hwnd, index);
			if((s = Control<TipState>(hwnd)) == nullptr)
				return 0;
			Parse(*s, text, (s->tools[static_cast<std::size_t>(index)].flags & TTF_PARSELINKS) != 0);
			const QSize size = Layout(hwnd, *s);
			return MAKELONG(size.width(), size.height());
		}
		return 0;
	case TTM_ADJUSTRECT:
		if(lp != 0)
		{
			auto *r = reinterpret_cast<RECT *>(lp);
			const int pad = Padding(hwnd);
			if(wp)
			{
				r->left -= pad;
				r->top -= pad;
				r->right += pad;
				r->bottom += pad;
			} else
			{
				r->left += pad;
				r->top += pad;
				r->right -= pad;
				r->bottom -= pad;
			}
			return TRUE;
		}
		return FALSE;
	case TTM_SETTITLE:
		if(s != nullptr)
		{
			s->icon = static_cast<UINT>(wp);
			s->title = lp != 0 ? reinterpret_cast<LPCWSTR>(lp) : L"";
			return TRUE;
		}
		return FALSE;
	default:
		return ::DefWindowProc(hwnd, msg, wp, lp);
	}
}

} // namespace

void RegisterToolTipClass()
{
	WNDCLASS wc{};
	wc.style = CS_DBLCLKS | CS_SAVEBITS;
	wc.lpfnWndProc = TipProc;
	wc.hCursor = ::LoadCursor(nullptr, IDC_ARROW);
	wc.lpszClassName = TOOLTIPS_CLASS;
	::RegisterClass(&wc);
}

} // namespace linopenmpt::qtwin
