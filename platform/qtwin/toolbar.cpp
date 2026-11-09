// The toolbar of comctl32 (ToolbarWindow32): buttons with images and text,
// separators, check and group buttons, drop-down buttons (TBN_DROPDOWN,
// TBSTYLE_EX_DRAWDDARROWS), flat buttons with hot tracking, wrapping,
// the CCS_* placement styles and tooltips. Buttons are drawn as Qt tool
// buttons; clicks send WM_COMMAND to the parent as comctl32 does.
#include "QtWin.h"

#include <QtGui/QIcon>
#include <QtGui/QPixmap>
#include <QtWidgets/QApplication>
#include <QtWidgets/QStyle>
#include <QtWidgets/QStyleOption>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace linopenmpt::qtwin
{

namespace
{

constexpr int ArrowWidth = 14;    // the drop-down arrow part of a button
constexpr int SeparatorWidth = 8; // comctl32's default

struct Button
{
	int image = 0;
	int command = 0;
	BYTE state = 0;
	BYTE style = 0;
	DWORD_PTR data = 0;
	std::wstring text;
	int cx = 0;   // TBIF_SIZE, 0 for automatic
	QRect rect;   // laid out
};

struct ToolbarState : ControlState
{
	std::vector<Button> buttons;
	std::vector<std::wstring> strings; // TB_ADDSTRING
	UINT structSize = sizeof(TBBUTTON);
	QSize bitmap{16, 15};
	QSize button{0, 0};  // TB_SETBUTTONSIZE, 0 for automatic
	QSize padding{7, 6};
	int indent = 0;
	int minWidth = 0, maxWidth = 0; // TB_SETBUTTONWIDTH
	DWORD exStyle = 0;
	HIMAGELIST images = nullptr;
	HIMAGELIST disabled = nullptr;
	HIMAGELIST hotImages = nullptr;
	HIMAGELIST ownImages = nullptr; // made by TB_ADDBITMAP
	int hot = -1;     // index
	int pressed = -1; // index, while the mouse is down
	bool pressedInside = false;
	bool pressedArrow = false;
	HWND notify = nullptr; // TB_SETPARENT
	HWND tooltip = nullptr;
	bool tooltipTried = false;
	int rows = 1;
	bool trackingLeave = false;
	int maxTextRows = 1;
};

LONG_PTR StyleOf(HWND hwnd)
{
	return ::GetWindowLongPtr(hwnd, GWL_STYLE);
}

HWND NotifyWindow(HWND hwnd, const ToolbarState &s)
{
	return s.notify != nullptr ? s.notify : ::GetParent(hwnd);
}

LRESULT Notify(HWND hwnd, const ToolbarState &s, NMHDR &hdr, UINT code)
{
	hdr.hwndFrom = hwnd;
	hdr.idFrom = static_cast<UINT_PTR>(::GetDlgCtrlID(hwnd));
	hdr.code = code;
	const HWND target = NotifyWindow(hwnd, s);
	return target != nullptr ? ::SendMessage(target, WM_NOTIFY, hdr.idFrom, reinterpret_cast<LPARAM>(&hdr)) : 0;
}

bool IsSeparator(const Button &b)
{
	return (b.style & BTNS_SEP) != 0;
}

bool HasArrow(const ToolbarState &s, const Button &b)
{
	return ((b.style & BTNS_DROPDOWN) && (s.exStyle & TBSTYLE_EX_DRAWDDARROWS)) || (b.style & BTNS_WHOLEDROPDOWN);
}

std::wstring ButtonText(const ToolbarState &s, const Button &b)
{
	(void)s;
	return b.text;
}

QSize ImageSize(const ToolbarState &s)
{
	int cx = 0, cy = 0;
	if(s.images != nullptr && ImageList_GetIconSize(s.images, &cx, &cy))
		return QSize(cx, cy);
	return s.bitmap;
}

QFont FontOf(HWND hwnd)
{
	Win32Widget *w = FromHandle(hwnd);
	return w != nullptr ? w->font() : QApplication::font();
}

bool AnyText(const ToolbarState &s)
{
	return std::any_of(s.buttons.begin(), s.buttons.end(), [](const Button &b) { return !b.text.empty() && !IsSeparator(b); });
}

// The size of an ordinary button: TB_SETBUTTONSIZE, or the image and the
// text with padding.
QSize ButtonSize(HWND hwnd, const ToolbarState &s)
{
	const QSize image = ImageSize(s);
	QSize size(image.width() + s.padding.width() + 1, image.height() + s.padding.height() + 1);
	if(AnyText(s))
	{
		const QFontMetrics fm(FontOf(hwnd));
		int textWidth = 0;
		for(const Button &b : s.buttons)
		{
			if(!IsSeparator(b) && !(b.style & BTNS_AUTOSIZE))
				textWidth = std::max(textWidth, fm.horizontalAdvance(ToQString(b.text.c_str())));
		}
		if(StyleOf(hwnd) & TBSTYLE_LIST)
		{
			size.setWidth(size.width() + textWidth + 4);
			size.setHeight(std::max(size.height(), fm.height() + s.padding.height()));
		} else
		{
			size.setWidth(std::max(size.width(), textWidth + s.padding.width()));
			size.setHeight(size.height() + fm.height() * s.maxTextRows);
		}
	}
	if(s.button.width() > 0)
		size.setWidth(std::max(s.button.width(), size.width() * 0 + s.button.width()));
	if(s.button.height() > 0)
		size.setHeight(s.button.height());
	if(s.minWidth > 0)
		size.setWidth(std::max(size.width(), s.minWidth));
	if(s.maxWidth > 0)
		size.setWidth(std::min(size.width(), s.maxWidth));
	return size;
}

int ButtonWidth(HWND hwnd, const ToolbarState &s, const Button &b, const QSize &standard)
{
	if(IsSeparator(b))
		return b.image > 0 ? b.image : SeparatorWidth;
	int width = b.cx > 0 ? b.cx : standard.width();
	if((b.style & BTNS_AUTOSIZE) && b.cx == 0)
	{
		const QFontMetrics fm(FontOf(hwnd));
		const QSize image = ImageSize(s);
		const int text = b.text.empty() ? 0 : fm.horizontalAdvance(ToQString(b.text.c_str()));
		width = (StyleOf(hwnd) & TBSTYLE_LIST) ? image.width() + text + s.padding.width() + (text > 0 ? 4 : 0) : std::max(image.width(), text) + s.padding.width();
	}
	if(HasArrow(s, b))
		width += ArrowWidth;
	return width;
}

int TopMargin(HWND hwnd)
{
	return (StyleOf(hwnd) & CCS_NODIVIDER) ? 0 : 2;
}

void Layout(HWND hwnd, ToolbarState &s)
{
	RECT rc{};
	::GetClientRect(hwnd, &rc);
	const QSize standard = ButtonSize(hwnd, s);
	const bool wrap = (StyleOf(hwnd) & TBSTYLE_WRAPABLE) != 0;
	int x = s.indent;
	int y = TopMargin(hwnd);
	s.rows = 1;
	for(Button &b : s.buttons)
	{
		if(b.state & TBSTATE_HIDDEN)
		{
			b.rect = QRect();
			continue;
		}
		const int width = ButtonWidth(hwnd, s, b, standard);
		if(wrap && x > s.indent && x + width > rc.right && !IsSeparator(b))
		{
			x = s.indent;
			y += standard.height();
			s.rows++;
		}
		b.rect = QRect(x, y, width, standard.height());
		x += width;
		if(b.state & TBSTATE_WRAP)
		{
			x = s.indent;
			y += standard.height() + (IsSeparator(b) ? SeparatorWidth : 0);
			s.rows++;
		}
	}
	// Keep the tooltip's rectangles in step.
	if(s.tooltip != nullptr)
	{
		for(const Button &b : s.buttons)
		{
			if(IsSeparator(b))
				continue;
			TOOLINFO ti{};
			ti.cbSize = sizeof(ti);
			ti.hwnd = hwnd;
			ti.uId = static_cast<UINT_PTR>(b.command);
			ti.rect = RECT{b.rect.left(), b.rect.top(), b.rect.left() + b.rect.width(), b.rect.top() + b.rect.height()};
			::SendMessage(s.tooltip, TTM_NEWTOOLRECT, 0, reinterpret_cast<LPARAM>(&ti));
		}
	}
}

QSize IdealSize(HWND hwnd, ToolbarState &s)
{
	Layout(hwnd, s);
	int right = 0, bottom = 0;
	for(const Button &b : s.buttons)
	{
		if(b.rect.isNull())
			continue;
		right = std::max(right, b.rect.left() + b.rect.width());
		bottom = std::max(bottom, b.rect.top() + b.rect.height());
	}
	return QSize(right, bottom);
}

// Sizes and places the toolbar in its parent, as the CCS_* styles ask.
void AutoSize(HWND hwnd, ToolbarState &s)
{
	const LONG_PTR style = StyleOf(hwnd);
	Layout(hwnd, s);
	if(style & CCS_NORESIZE)
		return;
	const HWND parent = ::GetParent(hwnd);
	if(parent == nullptr)
		return;
	RECT pr{};
	::GetClientRect(parent, &pr);
	const QSize ideal = IdealSize(hwnd, s);
	const int height = std::max(ideal.height(), ButtonSize(hwnd, s).height() + TopMargin(hwnd)) + ((style & CCS_NODIVIDER) ? 0 : 0);
	RECT wr{};
	::GetWindowRect(hwnd, &wr);
	::MapWindowPoints(nullptr, parent, reinterpret_cast<POINT *>(&wr), 2);
	int x = wr.left, y = wr.top, width = wr.right - wr.left;
	UINT flags = SWP_NOZORDER | SWP_NOACTIVATE;
	if(!(style & CCS_NOPARENTALIGN))
	{
		x = 0;
		width = pr.right;
		if((style & CCS_BOTTOM) == CCS_BOTTOM)
			y = pr.bottom - height;
		else if(!(style & CCS_NOMOVEY))
			y = 0;
	}
	::SetWindowPos(hwnd, nullptr, x, y, width, height, flags);
	Layout(hwnd, s);
}

int IndexOfCommand(const ToolbarState &s, int command)
{
	for(std::size_t i = 0; i < s.buttons.size(); i++)
	{
		if(s.buttons[i].command == command)
			return static_cast<int>(i);
	}
	return -1;
}

int ButtonAt(const ToolbarState &s, POINT pt)
{
	for(std::size_t i = 0; i < s.buttons.size(); i++)
	{
		if(s.buttons[i].rect.contains(QPoint(pt.x, pt.y)))
			return static_cast<int>(i);
	}
	return -1;
}

void EnsureTooltip(HWND hwnd, ToolbarState &s)
{
	if(s.tooltip != nullptr || s.tooltipTried || !(StyleOf(hwnd) & TBSTYLE_TOOLTIPS))
		return;
	s.tooltipTried = true;
	const HWND tip = ::CreateWindowEx(0, TOOLTIPS_CLASS, nullptr, WS_POPUP | TTS_ALWAYSTIP, 0, 0, 0, 0, hwnd, nullptr, nullptr, nullptr);
	ToolbarState *live = Control<ToolbarState>(hwnd);
	if(live == nullptr)
	{
		::DestroyWindow(tip);
		return;
	}
	live->tooltip = tip;
	for(const Button &b : live->buttons)
	{
		if(IsSeparator(b))
			continue;
		TOOLINFO ti{};
		ti.cbSize = sizeof(ti);
		ti.hwnd = hwnd;
		ti.uId = static_cast<UINT_PTR>(b.command);
		ti.lpszText = LPSTR_TEXTCALLBACK;
		ti.rect = RECT{b.rect.left(), b.rect.top(), b.rect.left() + b.rect.width(), b.rect.top() + b.rect.height()};
		::SendMessage(tip, TTM_ADDTOOL, 0, reinterpret_cast<LPARAM>(&ti));
	}
}

void AddTool(HWND hwnd, ToolbarState &s, const Button &b)
{
	if(s.tooltip == nullptr || IsSeparator(b))
		return;
	TOOLINFO ti{};
	ti.cbSize = sizeof(ti);
	ti.hwnd = hwnd;
	ti.uId = static_cast<UINT_PTR>(b.command);
	ti.lpszText = LPSTR_TEXTCALLBACK;
	::SendMessage(s.tooltip, TTM_ADDTOOL, 0, reinterpret_cast<LPARAM>(&ti));
}

void RemoveTool(HWND hwnd, ToolbarState &s, const Button &b)
{
	if(s.tooltip == nullptr || IsSeparator(b))
		return;
	TOOLINFO ti{};
	ti.cbSize = sizeof(ti);
	ti.hwnd = hwnd;
	ti.uId = static_cast<UINT_PTR>(b.command);
	::SendMessage(s.tooltip, TTM_DELTOOL, 0, reinterpret_cast<LPARAM>(&ti));
}

void Relay(HWND hwnd, ToolbarState &s, UINT msg, WPARAM wp, LPARAM lp)
{
	EnsureTooltip(hwnd, s);
	ToolbarState *live = Control<ToolbarState>(hwnd);
	if(live == nullptr || live->tooltip == nullptr)
		return;
	MSG m{};
	m.hwnd = hwnd;
	m.message = msg;
	m.wParam = wp;
	m.lParam = lp;
	::SendMessage(live->tooltip, TTM_RELAYEVENT, 0, reinterpret_cast<LPARAM>(&m));
}

void SetHot(HWND hwnd, ToolbarState &s, int index, DWORD flags)
{
	if(index == s.hot)
		return;
	NMTBHOTITEM nm{};
	nm.idOld = s.hot >= 0 && s.hot < static_cast<int>(s.buttons.size()) ? s.buttons[static_cast<std::size_t>(s.hot)].command : 0;
	nm.idNew = index >= 0 ? s.buttons[static_cast<std::size_t>(index)].command : 0;
	nm.dwFlags = flags | (s.hot < 0 ? HICF_ENTERING : 0) | (index < 0 ? HICF_LEAVING : 0);
	if(Notify(hwnd, s, nm.hdr, TBN_HOTITEMCHANGE) != 0)
		return;
	if(ToolbarState *live = Control<ToolbarState>(hwnd))
	{
		live->hot = index;
		::InvalidateRect(hwnd, nullptr, TRUE);
	}
}

QIcon IconOf(const ToolbarState &s, const Button &b)
{
	QIcon icon;
	if(b.image == I_IMAGENONE || b.image < 0)
		return icon;
	if(s.images != nullptr)
	{
		const QImage normal = ImageListImage(s.images, b.image);
		if(!normal.isNull())
			icon.addPixmap(QPixmap::fromImage(normal), QIcon::Normal);
	}
	if(s.disabled != nullptr)
	{
		const QImage dis = ImageListImage(s.disabled, b.image);
		if(!dis.isNull())
			icon.addPixmap(QPixmap::fromImage(dis), QIcon::Disabled);
	}
	if(s.hotImages != nullptr)
	{
		const QImage hot = ImageListImage(s.hotImages, b.image);
		if(!hot.isNull())
			icon.addPixmap(QPixmap::fromImage(hot), QIcon::Active);
	}
	return icon;
}

void Paint(HWND hwnd, HDC dc)
{
	ToolbarState *s = Control<ToolbarState>(hwnd);
	Win32Widget *w = FromHandle(hwnd);
	QPainter *p = DCPainter(dc);
	if(s == nullptr || w == nullptr || p == nullptr)
		return;
	RECT rc{};
	::GetClientRect(hwnd, &rc);
	const LONG_PTR style = StyleOf(hwnd);
	if(!(style & TBSTYLE_TRANSPARENT))
	{
		HBRUSH brush = reinterpret_cast<HBRUSH>(::SendMessage(::GetParent(hwnd), WM_CTLCOLORDLG, reinterpret_cast<WPARAM>(dc), reinterpret_cast<LPARAM>(hwnd)));
		::FillRect(dc, &rc, brush != nullptr ? brush : ::GetSysColorBrush(COLOR_BTNFACE));
	}
	QStyle *qs = QApplication::style();
	if(!(style & CCS_NODIVIDER))
	{
		// The divider: an etched line along the top.
		p->save();
		p->setPen(ToQColor(::GetSysColor(COLOR_BTNSHADOW)));
		p->drawLine(0, 0, rc.right, 0);
		p->setPen(ToQColor(::GetSysColor(COLOR_BTNHIGHLIGHT)));
		p->drawLine(0, 1, rc.right, 1);
		p->restore();
	}
	const bool flat = (style & TBSTYLE_FLAT) != 0;
	const bool list = (style & TBSTYLE_LIST) != 0;
	const QSize image = ImageSize(*s);
	const bool enabledWindow = ::IsWindowEnabled(hwnd) != FALSE;
	for(std::size_t i = 0; i < s->buttons.size(); i++)
	{
		const Button &b = s->buttons[i];
		if(b.rect.isNull())
			continue;
		if(IsSeparator(b))
		{
			if(flat || true)
			{
				QStyleOption opt;
				opt.initFrom(w);
				opt.rect = QRect(b.rect.left() + b.rect.width() / 2 - 1, b.rect.top() + 2, 2, b.rect.height() - 4);
				opt.state |= QStyle::State_Horizontal;
				p->save();
				qs->drawPrimitive(QStyle::PE_IndicatorToolBarSeparator, &opt, p, w);
				p->restore();
			}
			continue;
		}
		const bool enabled = enabledWindow && (b.state & TBSTATE_ENABLED);
		const bool hot = static_cast<int>(i) == s->hot && enabled;
		const bool down = (static_cast<int>(i) == s->pressed && s->pressedInside) || (b.state & TBSTATE_PRESSED);
		const bool checked = (b.state & TBSTATE_CHECKED) != 0;
		QStyleOptionToolButton opt;
		opt.initFrom(w);
		opt.rect = b.rect;
		opt.state &= ~(QStyle::State_HasFocus | QStyle::State_MouseOver | QStyle::State_Enabled);
		if(enabled)
			opt.state |= QStyle::State_Enabled;
		if(flat)
			opt.state |= QStyle::State_AutoRaise;
		if(hot)
			opt.state |= QStyle::State_MouseOver;
		if((down && !s->pressedArrow) || checked)
			opt.state |= QStyle::State_Sunken;
		else
			opt.state |= QStyle::State_Raised;
		if(checked)
			opt.state |= QStyle::State_On;
		opt.subControls = QStyle::SC_ToolButton;
		if(HasArrow(*s, b) && !(b.style & BTNS_WHOLEDROPDOWN))
		{
			opt.features |= QStyleOptionToolButton::MenuButtonPopup;
			opt.subControls |= QStyle::SC_ToolButtonMenu;
			if(down && s->pressedArrow)
				opt.activeSubControls = QStyle::SC_ToolButtonMenu;
		} else if(HasArrow(*s, b))
		{
			opt.features |= QStyleOptionToolButton::HasMenu;
		}
		opt.icon = IconOf(*s, b);
		opt.iconSize = image;
		const std::wstring text = ButtonText(*s, b);
		const bool showText = !text.empty() && (!list || !(s->exStyle & TBSTYLE_EX_MIXEDBUTTONS) || (b.style & BTNS_SHOWTEXT));
		if(showText)
		{
			opt.text = ToQString(text.c_str());
			if(!(style & TBSTYLE_NOPREFIX) && !(b.style & BTNS_NOPREFIX))
				opt.text.replace(QStringLiteral("&&"), QStringLiteral("\x01")).remove(QLatin1Char('&')).replace(QLatin1Char('\x01'), QLatin1Char('&'));
		}
		opt.toolButtonStyle = opt.icon.isNull() ? Qt::ToolButtonTextOnly : (showText ? (list ? Qt::ToolButtonTextBesideIcon : Qt::ToolButtonTextUnderIcon) : Qt::ToolButtonIconOnly);
		opt.font = FontOf(hwnd);
		p->save();
		// Checked flat buttons get comctl32's dithered look as a light fill.
		if(checked && flat && !hot)
		{
			QColor fill = ToQColor(::GetSysColor(COLOR_BTNHIGHLIGHT));
			fill.setAlpha(160);
			p->fillRect(b.rect.adjusted(1, 1, -1, -1), fill);
		}
		qs->drawComplexControl(QStyle::CC_ToolButton, &opt, p, w);
		if(b.state & TBSTATE_MARKED)
		{
			QColor mark = ToQColor(::GetSysColor(COLOR_HIGHLIGHT));
			mark.setAlpha(60);
			p->fillRect(b.rect, mark);
		}
		p->restore();
	}
}

void Click(HWND hwnd, int index)
{
	ToolbarState *s = Control<ToolbarState>(hwnd);
	if(s == nullptr || index < 0 || index >= static_cast<int>(s->buttons.size()))
		return;
	Button &b = s->buttons[static_cast<std::size_t>(index)];
	if(b.style & BTNS_CHECK)
	{
		if(b.style & BTNS_GROUP)
		{
			// A group of radio buttons: the clicked one goes down, the
			// others in the same run up.
			if(!(b.state & TBSTATE_CHECKED))
			{
				int first = index, last = index;
				while(first > 0 && (s->buttons[static_cast<std::size_t>(first - 1)].style & (BTNS_CHECK | BTNS_GROUP)) == (BTNS_CHECK | BTNS_GROUP))
					first--;
				while(last + 1 < static_cast<int>(s->buttons.size()) && (s->buttons[static_cast<std::size_t>(last + 1)].style & (BTNS_CHECK | BTNS_GROUP)) == (BTNS_CHECK | BTNS_GROUP))
					last++;
				for(int k = first; k <= last; k++)
					s->buttons[static_cast<std::size_t>(k)].state &= ~TBSTATE_CHECKED;
				b.state |= TBSTATE_CHECKED;
			}
		} else
		{
			b.state ^= TBSTATE_CHECKED;
		}
	}
	const int command = b.command;
	::InvalidateRect(hwnd, nullptr, TRUE);
	::SendMessage(NotifyWindow(hwnd, *s), WM_COMMAND, MAKEWPARAM(command, BN_CLICKED), reinterpret_cast<LPARAM>(hwnd));
}

LRESULT MouseNotify(HWND hwnd, ToolbarState &s, UINT code, int index, POINT pt)
{
	NMMOUSE nm{};
	nm.dwItemSpec = index >= 0 ? static_cast<DWORD_PTR>(s.buttons[static_cast<std::size_t>(index)].command) : static_cast<DWORD_PTR>(-1);
	nm.dwItemData = index >= 0 ? s.buttons[static_cast<std::size_t>(index)].data : 0;
	nm.pt = pt;
	return Notify(hwnd, s, nm.hdr, code);
}

bool InsertButton(HWND hwnd, ToolbarState &s, int index, const TBBUTTON &in)
{
	Button b;
	b.image = in.iBitmap;
	b.command = in.idCommand;
	b.state = in.fsState;
	b.style = in.fsStyle;
	b.data = in.dwData;
	if(in.iString != 0 && in.iString != -1)
	{
		if(IS_INTRESOURCE(in.iString))
		{
			if(static_cast<std::size_t>(in.iString) < s.strings.size())
				b.text = s.strings[static_cast<std::size_t>(in.iString)];
		} else
		{
			b.text = reinterpret_cast<LPCWSTR>(in.iString);
		}
	}
	if(index < 0 || index > static_cast<int>(s.buttons.size()))
		index = static_cast<int>(s.buttons.size());
	s.buttons.insert(s.buttons.begin() + index, b);
	if(s.hot >= index)
		s.hot++;
	AddTool(hwnd, s, b);
	return true;
}

const TBBUTTON *ButtonAtArray(const ToolbarState &s, LPARAM lp, int i)
{
	// TB_BUTTONSTRUCTSIZE lets old callers pass a smaller structure.
	const auto *base = reinterpret_cast<const BYTE *>(lp);
	return reinterpret_cast<const TBBUTTON *>(base + static_cast<std::size_t>(i) * s.structSize);
}

void CopyButton(const Button &b, TBBUTTON *out)
{
	*out = TBBUTTON{};
	out->iBitmap = b.image;
	out->idCommand = b.command;
	out->fsState = b.state;
	out->fsStyle = b.style;
	out->dwData = b.data;
	out->iString = 0;
}

int ResolveIndex(const ToolbarState &s, const TBBUTTONINFO *info, WPARAM wp)
{
	return (info->dwMask & TBIF_BYINDEX) ? (static_cast<int>(wp) < static_cast<int>(s.buttons.size()) ? static_cast<int>(wp) : -1) : IndexOfCommand(s, static_cast<int>(wp));
}

int AddBitmap(HWND hwnd, ToolbarState &s, int count, const TBADDBITMAP *add)
{
	if(add == nullptr)
		return -1;
	HBITMAP bmp = nullptr;
	bool own = false;
	if(add->hInst == nullptr)
	{
		bmp = reinterpret_cast<HBITMAP>(add->nID);
	} else if(add->hInst == HINST_COMMCTRL)
	{
		return -1; // the standard bitmaps are not drawn
	} else
	{
		bmp = ::LoadBitmap(add->hInst, MAKEINTRESOURCE(add->nID));
		own = true;
	}
	if(bmp == nullptr)
		return -1;
	if(s.images == nullptr)
	{
		s.ownImages = ImageList_Create(s.bitmap.width(), s.bitmap.height(), ILC_COLOR32 | ILC_MASK, std::max(count, 4), 4);
		s.images = s.ownImages;
	}
	const int first = ImageList_GetImageCount(s.images);
	ImageList_AddMasked(s.images, bmp, RGB(192, 192, 192));
	if(own)
		::DeleteObject(bmp);
	::InvalidateRect(hwnd, nullptr, TRUE);
	return first;
}

LRESULT CALLBACK ToolbarProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
	ToolbarState *s = Control<ToolbarState>(hwnd);
	switch(msg)
	{
	case WM_NCCREATE:
		if(WindowData *d = Data(hwnd))
			d->control = std::make_unique<ToolbarState>();
		return ::DefWindowProc(hwnd, msg, wp, lp);
	case WM_CREATE:
		if(s != nullptr)
			EnsureTooltip(hwnd, *s);
		return 0;
	case WM_DESTROY:
		if(s != nullptr)
		{
			if(s->tooltip != nullptr)
				::DestroyWindow(s->tooltip);
			if((s = Control<ToolbarState>(hwnd)) != nullptr)
			{
				s->tooltip = nullptr;
				if(s->ownImages != nullptr)
					ImageList_Destroy(s->ownImages);
				s->ownImages = nullptr;
			}
		}
		return ::DefWindowProc(hwnd, msg, wp, lp);
	case WM_ERASEBKGND:
		return 1;
	case WM_PAINT:
	case WM_PRINTCLIENT:
		if(s != nullptr)
		{
			Layout(hwnd, *s);
			EnsureTooltip(hwnd, *s);
		}
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
	case WM_SIZE:
		if(s != nullptr)
		{
			Layout(hwnd, *s);
			::InvalidateRect(hwnd, nullptr, TRUE);
		}
		return 0;
	case WM_ENABLE:
		::InvalidateRect(hwnd, nullptr, TRUE);
		return 0;
	case WM_SETFONT:
		::DefWindowProc(hwnd, msg, wp, lp);
		if(s != nullptr)
		{
			Layout(hwnd, *s);
			::InvalidateRect(hwnd, nullptr, TRUE);
		}
		return 0;
	case WM_GETDLGCODE:
		return 0;
	case WM_MOUSEMOVE:
		if(s != nullptr)
		{
			Relay(hwnd, *s, msg, wp, lp);
			if((s = Control<ToolbarState>(hwnd)) == nullptr)
				return 0;
			POINT pt{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
			const int index = ButtonAt(*s, pt);
			if(s->pressed >= 0)
			{
				const bool inside = index == s->pressed;
				if(inside != s->pressedInside)
				{
					s->pressedInside = inside;
					::InvalidateRect(hwnd, nullptr, TRUE);
				}
			}
			const bool flat = (StyleOf(hwnd) & TBSTYLE_FLAT) != 0;
			int hot = -1;
			if(index >= 0 && !IsSeparator(s->buttons[static_cast<std::size_t>(index)]) && (s->buttons[static_cast<std::size_t>(index)].state & TBSTATE_ENABLED))
				hot = index;
			if(flat || hot != s->hot)
				SetHot(hwnd, *s, hot, HICF_MOUSE);
			if((s = Control<ToolbarState>(hwnd)) != nullptr && !s->trackingLeave)
			{
				TRACKMOUSEEVENT tme{};
				tme.cbSize = sizeof(tme);
				tme.dwFlags = TME_LEAVE;
				tme.hwndTrack = hwnd;
				::TrackMouseEvent(&tme);
				s->trackingLeave = true;
			}
		}
		return 0;
	case WM_MOUSELEAVE:
		if(s != nullptr)
		{
			s->trackingLeave = false;
			if(s->pressed < 0)
				SetHot(hwnd, *s, -1, HICF_MOUSE);
		}
		return 0;
	case WM_LBUTTONDOWN:
	case WM_LBUTTONDBLCLK:
		if(s != nullptr)
		{
			Relay(hwnd, *s, msg, wp, lp);
			if((s = Control<ToolbarState>(hwnd)) == nullptr)
				return 0;
			POINT pt{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
			const int index = ButtonAt(*s, pt);
			if(index < 0)
				return 0;
			Button &b = s->buttons[static_cast<std::size_t>(index)];
			if(IsSeparator(b) || !(b.state & TBSTATE_ENABLED))
				return 0;
			const bool arrow = HasArrow(*s, b) && ((b.style & BTNS_WHOLEDROPDOWN) || pt.x >= b.rect.right() + 1 - ArrowWidth);
			const bool whole = (b.style & BTNS_DROPDOWN) && !(s->exStyle & TBSTYLE_EX_DRAWDDARROWS);
			if(arrow || whole)
			{
				// TBN_DROPDOWN at once; the button shows pressed meanwhile.
				s->pressed = index;
				s->pressedInside = true;
				s->pressedArrow = arrow && !(b.style & BTNS_WHOLEDROPDOWN);
				::InvalidateRect(hwnd, nullptr, TRUE);
				::UpdateWindow(hwnd);
				NMTOOLBAR nm{};
				nm.iItem = b.command;
				CopyButton(b, &nm.tbButton);
				nm.rcButton = RECT{b.rect.left(), b.rect.top(), b.rect.left() + b.rect.width(), b.rect.top() + b.rect.height()};
				const LRESULT r = Notify(hwnd, *s, nm.hdr, TBN_DROPDOWN);
				if((s = Control<ToolbarState>(hwnd)) == nullptr)
					return 0;
				s->pressed = -1;
				s->pressedArrow = false;
				::InvalidateRect(hwnd, nullptr, TRUE);
				// TBDDRET_NODEFAULT: the drop-down was not handled; a whole
				// drop-down button then acts as a plain one.
				if(r == TBDDRET_NODEFAULT && whole)
					Click(hwnd, index);
				return 0;
			}
			s->pressed = index;
			s->pressedInside = true;
			s->pressedArrow = false;
			::SetCapture(hwnd);
			::InvalidateRect(hwnd, nullptr, TRUE);
		}
		return 0;
	case WM_LBUTTONUP:
		if(s != nullptr)
		{
			Relay(hwnd, *s, msg, wp, lp);
			if((s = Control<ToolbarState>(hwnd)) == nullptr)
				return 0;
			const int index = s->pressed;
			const bool inside = s->pressedInside;
			s->pressed = -1;
			s->pressedInside = false;
			if(::GetCapture() == hwnd)
				::ReleaseCapture();
			if((s = Control<ToolbarState>(hwnd)) == nullptr)
				return 0;
			::InvalidateRect(hwnd, nullptr, TRUE);
			POINT pt{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
			if(index >= 0 && inside)
			{
				// NM_CLICK first; a nonzero answer keeps the WM_COMMAND back.
				if(MouseNotify(hwnd, *s, NM_CLICK, index, pt) == 0)
					Click(hwnd, index);
			} else if(index < 0)
			{
				MouseNotify(hwnd, *s, NM_CLICK, ButtonAt(*s, pt), pt);
			}
		}
		return 0;
	case WM_RBUTTONUP:
		if(s != nullptr)
		{
			Relay(hwnd, *s, msg, wp, lp);
			if((s = Control<ToolbarState>(hwnd)) == nullptr)
				return 0;
			POINT pt{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
			if(MouseNotify(hwnd, *s, NM_RCLICK, ButtonAt(*s, pt), pt) == 0)
				return ::DefWindowProc(hwnd, msg, wp, lp);
		}
		return 0;
	case WM_RBUTTONDOWN:
	case WM_MBUTTONDOWN:
	case WM_MBUTTONUP:
		if(s != nullptr)
			Relay(hwnd, *s, msg, wp, lp);
		return 0;
	case WM_CAPTURECHANGED:
		if(s != nullptr && reinterpret_cast<HWND>(lp) != hwnd && s->pressed >= 0)
		{
			s->pressed = -1;
			s->pressedInside = false;
			::InvalidateRect(hwnd, nullptr, TRUE);
		}
		return 0;
	case WM_NOTIFY:
		if(s != nullptr && lp != 0)
		{
			auto *hdr = reinterpret_cast<NMHDR *>(lp);
			if(hdr->hwndFrom == s->tooltip && s->tooltip != nullptr)
			{
				// The tooltip asks the toolbar; the toolbar asks its parent.
				if(hdr->code == TTN_GETDISPINFO)
				{
					auto *di = reinterpret_cast<NMTTDISPINFO *>(lp);
					di->lpszText = di->szText;
					di->szText[0] = 0;
					const HWND target = NotifyWindow(hwnd, *s);
					if(target != nullptr)
						::SendMessage(target, WM_NOTIFY, hdr->idFrom, lp);
					if((s = Control<ToolbarState>(hwnd)) != nullptr && (di->lpszText == nullptr || (!IS_INTRESOURCE(di->lpszText) && di->lpszText[0] == 0)))
					{
						const int index = IndexOfCommand(*s, static_cast<int>(hdr->idFrom));
						if(index >= 0 && !s->buttons[static_cast<std::size_t>(index)].text.empty())
						{
							const std::wstring &t = s->buttons[static_cast<std::size_t>(index)].text;
							const std::size_t n = std::min<std::size_t>(t.size(), 79);
							std::copy_n(t.begin(), n, di->szText);
							di->szText[n] = 0;
							di->lpszText = di->szText;
						}
					}
					return 0;
				}
				const HWND target = NotifyWindow(hwnd, *s);
				return target != nullptr ? ::SendMessage(target, WM_NOTIFY, hdr->idFrom, lp) : 0;
			}
		}
		return 0;

	case TB_BUTTONSTRUCTSIZE:
		if(s != nullptr)
			s->structSize = std::max<UINT>(static_cast<UINT>(wp), offsetof(TBBUTTON, dwData));
		return 0;
	case TB_ADDBUTTONS:
	case TB_INSERTBUTTON:
		if(s != nullptr && lp != 0)
		{
			if(msg == TB_ADDBUTTONS)
			{
				for(int i = 0; i < static_cast<int>(wp); i++)
					InsertButton(hwnd, *s, -1, *ButtonAtArray(*s, lp, i));
			} else
			{
				InsertButton(hwnd, *s, static_cast<int>(wp), *reinterpret_cast<const TBBUTTON *>(lp));
			}
			if((s = Control<ToolbarState>(hwnd)) != nullptr)
			{
				Layout(hwnd, *s);
				::InvalidateRect(hwnd, nullptr, TRUE);
			}
			return TRUE;
		}
		return FALSE;
	case TB_DELETEBUTTON:
		if(s != nullptr && wp < s->buttons.size())
		{
			RemoveTool(hwnd, *s, s->buttons[wp]);
			s->buttons.erase(s->buttons.begin() + static_cast<std::ptrdiff_t>(wp));
			if(s->hot == static_cast<int>(wp))
				s->hot = -1;
			else if(s->hot > static_cast<int>(wp))
				s->hot--;
			s->pressed = -1;
			Layout(hwnd, *s);
			::InvalidateRect(hwnd, nullptr, TRUE);
			return TRUE;
		}
		return FALSE;
	case TB_BUTTONCOUNT:
		return s ? static_cast<LRESULT>(s->buttons.size()) : 0;
	case TB_GETBUTTON:
		if(s != nullptr && lp != 0 && wp < s->buttons.size())
		{
			CopyButton(s->buttons[wp], reinterpret_cast<TBBUTTON *>(lp));
			return TRUE;
		}
		return FALSE;
	case TB_COMMANDTOINDEX:
		return s ? IndexOfCommand(*s, static_cast<int>(wp)) : -1;
	case TB_GETBUTTONINFO:
		if(s != nullptr && lp != 0)
		{
			auto *info = reinterpret_cast<TBBUTTONINFO *>(lp);
			const int index = ResolveIndex(*s, info, wp);
			if(index < 0)
				return -1;
			const Button &b = s->buttons[static_cast<std::size_t>(index)];
			if(info->dwMask & TBIF_COMMAND)
				info->idCommand = b.command;
			if(info->dwMask & TBIF_IMAGE)
				info->iImage = b.image;
			if(info->dwMask & TBIF_LPARAM)
				info->lParam = b.data;
			if(info->dwMask & TBIF_SIZE)
				info->cx = static_cast<WORD>(b.rect.width());
			if(info->dwMask & TBIF_STATE)
				info->fsState = b.state;
			if(info->dwMask & TBIF_STYLE)
				info->fsStyle = b.style;
			if((info->dwMask & TBIF_TEXT) && info->pszText != nullptr && info->cchText > 0)
			{
				const std::size_t n = std::min(b.text.size(), static_cast<std::size_t>(info->cchText - 1));
				std::copy_n(b.text.begin(), n, info->pszText);
				info->pszText[n] = 0;
			}
			return index;
		}
		return -1;
	case TB_SETBUTTONINFO:
		if(s != nullptr && lp != 0)
		{
			const auto *info = reinterpret_cast<const TBBUTTONINFO *>(lp);
			const int index = ResolveIndex(*s, info, wp);
			if(index < 0)
				return FALSE;
			Button &b = s->buttons[static_cast<std::size_t>(index)];
			const int oldCommand = b.command;
			if(info->dwMask & TBIF_COMMAND)
				b.command = info->idCommand;
			if(info->dwMask & TBIF_IMAGE)
				b.image = info->iImage;
			if(info->dwMask & TBIF_LPARAM)
				b.data = info->lParam;
			if(info->dwMask & TBIF_SIZE)
				b.cx = info->cx;
			if(info->dwMask & TBIF_STATE)
				b.state = info->fsState;
			if(info->dwMask & TBIF_STYLE)
				b.style = info->fsStyle;
			if(info->dwMask & TBIF_TEXT)
				b.text = info->pszText != nullptr ? info->pszText : L"";
			if(oldCommand != b.command && s->tooltip != nullptr)
			{
				Button old = b;
				old.command = oldCommand;
				RemoveTool(hwnd, *s, old);
				AddTool(hwnd, *s, b);
			}
			Layout(hwnd, *s);
			::InvalidateRect(hwnd, nullptr, TRUE);
			return TRUE;
		}
		return FALSE;
	case TB_CHECKBUTTON:
	case TB_ENABLEBUTTON:
	case TB_PRESSBUTTON:
	case TB_HIDEBUTTON:
	case TB_INDETERMINATE:
	case TB_MARKBUTTON:
		if(s != nullptr)
		{
			const int index = IndexOfCommand(*s, static_cast<int>(wp));
			if(index < 0)
				return FALSE;
			Button &b = s->buttons[static_cast<std::size_t>(index)];
			BYTE bit = 0;
			switch(msg)
			{
			case TB_CHECKBUTTON: bit = TBSTATE_CHECKED; break;
			case TB_ENABLEBUTTON: bit = TBSTATE_ENABLED; break;
			case TB_PRESSBUTTON: bit = TBSTATE_PRESSED; break;
			case TB_HIDEBUTTON: bit = TBSTATE_HIDDEN; break;
			case TB_INDETERMINATE: bit = 0x10; break;
			default: bit = TBSTATE_MARKED; break;
			}
			const bool on = LOWORD(lp) != 0;
			if(msg == TB_CHECKBUTTON && on && (b.style & BTNS_GROUP))
			{
				// Checking one of a group unchecks the others.
				for(int k = index - 1; k >= 0 && (s->buttons[static_cast<std::size_t>(k)].style & BTNS_GROUP); k--)
					s->buttons[static_cast<std::size_t>(k)].state &= ~TBSTATE_CHECKED;
				for(std::size_t k = static_cast<std::size_t>(index) + 1; k < s->buttons.size() && (s->buttons[k].style & BTNS_GROUP); k++)
					s->buttons[k].state &= ~TBSTATE_CHECKED;
			}
			if(on)
				b.state |= bit;
			else
				b.state &= ~bit;
			if(msg == TB_ENABLEBUTTON && !on && s->hot == index)
				s->hot = -1;
			if(msg == TB_HIDEBUTTON)
				Layout(hwnd, *s);
			::InvalidateRect(hwnd, nullptr, TRUE);
			return TRUE;
		}
		return FALSE;
	case TB_ISBUTTONENABLED:
	case TB_ISBUTTONCHECKED:
	case TB_ISBUTTONPRESSED:
	case TB_ISBUTTONHIDDEN:
	case TB_ISBUTTONINDETERMINATE:
	case TB_ISBUTTONHIGHLIGHTED:
		if(s != nullptr)
		{
			const int index = IndexOfCommand(*s, static_cast<int>(wp));
			if(index < 0)
				return -1;
			const BYTE state = s->buttons[static_cast<std::size_t>(index)].state;
			switch(msg)
			{
			case TB_ISBUTTONENABLED: return (state & TBSTATE_ENABLED) != 0;
			case TB_ISBUTTONCHECKED: return (state & TBSTATE_CHECKED) != 0;
			case TB_ISBUTTONPRESSED: return (state & TBSTATE_PRESSED) != 0;
			case TB_ISBUTTONHIDDEN: return (state & TBSTATE_HIDDEN) != 0;
			case TB_ISBUTTONINDETERMINATE: return (state & 0x10) != 0;
			default: return (state & TBSTATE_MARKED) != 0;
			}
		}
		return -1;
	case TB_SETSTATE:
		if(s != nullptr)
		{
			const int index = IndexOfCommand(*s, static_cast<int>(wp));
			if(index < 0)
				return FALSE;
			Button &b = s->buttons[static_cast<std::size_t>(index)];
			const bool hiddenChanged = ((b.state ^ static_cast<BYTE>(LOWORD(lp))) & TBSTATE_HIDDEN) != 0;
			b.state = static_cast<BYTE>(LOWORD(lp));
			if(hiddenChanged)
				Layout(hwnd, *s);
			::InvalidateRect(hwnd, nullptr, TRUE);
			return TRUE;
		}
		return FALSE;
	case TB_GETSTATE:
		if(s != nullptr)
		{
			const int index = IndexOfCommand(*s, static_cast<int>(wp));
			return index < 0 ? -1 : s->buttons[static_cast<std::size_t>(index)].state;
		}
		return -1;
	case TB_GETITEMRECT:
	case TB_GETRECT:
		if(s != nullptr && lp != 0)
		{
			Layout(hwnd, *s);
			const int index = msg == TB_GETRECT ? IndexOfCommand(*s, static_cast<int>(wp)) : static_cast<int>(wp);
			if(index < 0 || index >= static_cast<int>(s->buttons.size()))
				return FALSE;
			const QRect r = s->buttons[static_cast<std::size_t>(index)].rect;
			if(r.isNull() && msg == TB_GETITEMRECT)
				return FALSE;
			*reinterpret_cast<RECT *>(lp) = RECT{r.left(), r.top(), r.left() + r.width(), r.top() + r.height()};
			return TRUE;
		}
		return FALSE;
	case TB_SETBITMAPSIZE:
		if(s != nullptr)
		{
			s->bitmap = QSize(std::max(0, static_cast<int>(LOWORD(lp))), std::max(0, static_cast<int>(HIWORD(lp))));
			Layout(hwnd, *s);
			::InvalidateRect(hwnd, nullptr, TRUE);
			return TRUE;
		}
		return FALSE;
	case TB_SETBUTTONSIZE:
		if(s != nullptr)
		{
			s->button = QSize(static_cast<int>(LOWORD(lp)), static_cast<int>(HIWORD(lp)));
			Layout(hwnd, *s);
			::InvalidateRect(hwnd, nullptr, TRUE);
			return TRUE;
		}
		return FALSE;
	case TB_GETBUTTONSIZE:
		if(s != nullptr)
		{
			const QSize size = ButtonSize(hwnd, *s);
			return MAKELONG(size.width(), size.height());
		}
		return 0;
	case TB_SETBUTTONWIDTH:
		if(s != nullptr)
		{
			s->minWidth = LOWORD(lp);
			s->maxWidth = HIWORD(lp);
			Layout(hwnd, *s);
			::InvalidateRect(hwnd, nullptr, TRUE);
			return TRUE;
		}
		return FALSE;
	case TB_SETINDENT:
		if(s != nullptr)
		{
			s->indent = static_cast<int>(wp);
			Layout(hwnd, *s);
			::InvalidateRect(hwnd, nullptr, TRUE);
			return TRUE;
		}
		return FALSE;
	case TB_SETPADDING:
		if(s != nullptr)
		{
			const QSize old = s->padding;
			s->padding = QSize(LOWORD(lp), HIWORD(lp));
			Layout(hwnd, *s);
			return MAKELONG(old.width(), old.height());
		}
		return 0;
	case TB_GETPADDING:
		return s ? MAKELONG(s->padding.width(), s->padding.height()) : 0;
	case TB_SETEXTENDEDSTYLE:
		if(s != nullptr)
		{
			const DWORD old = s->exStyle;
			s->exStyle = wp != 0 ? ((s->exStyle & ~static_cast<DWORD>(wp)) | (static_cast<DWORD>(lp) & static_cast<DWORD>(wp))) : static_cast<DWORD>(lp);
			Layout(hwnd, *s);
			::InvalidateRect(hwnd, nullptr, TRUE);
			return old;
		}
		return 0;
	case TB_GETEXTENDEDSTYLE:
		return s ? s->exStyle : 0;
	case TB_SETSTYLE:
		::SetWindowLongPtr(hwnd, GWL_STYLE, static_cast<LONG_PTR>(lp));
		if(s != nullptr)
		{
			Layout(hwnd, *s);
			::InvalidateRect(hwnd, nullptr, TRUE);
		}
		return 0;
	case TB_GETSTYLE:
		return StyleOf(hwnd);
	case TB_SETIMAGELIST:
	case TB_SETDISABLEDIMAGELIST:
	case TB_SETHOTIMAGELIST:
		if(s != nullptr)
		{
			HIMAGELIST &slot = msg == TB_SETIMAGELIST ? s->images : (msg == TB_SETDISABLEDIMAGELIST ? s->disabled : s->hotImages);
			HIMAGELIST old = slot;
			slot = reinterpret_cast<HIMAGELIST>(lp);
			if(msg == TB_SETIMAGELIST)
			{
				int cx = 0, cy = 0;
				if(slot != nullptr && ImageList_GetIconSize(slot, &cx, &cy))
					s->bitmap = QSize(cx, cy);
			}
			Layout(hwnd, *s);
			::InvalidateRect(hwnd, nullptr, TRUE);
			return reinterpret_cast<LRESULT>(old);
		}
		return 0;
	case TB_GETIMAGELIST:
		return s ? reinterpret_cast<LRESULT>(s->images) : 0;
	case TB_GETDISABLEDIMAGELIST:
		return s ? reinterpret_cast<LRESULT>(s->disabled) : 0;
	case TB_GETHOTIMAGELIST:
		return s ? reinterpret_cast<LRESULT>(s->hotImages) : 0;
	case TB_ADDBITMAP:
		return s ? AddBitmap(hwnd, *s, static_cast<int>(wp), reinterpret_cast<const TBADDBITMAP *>(lp)) : -1;
	case TB_REPLACEBITMAP:
		// The images made from one bitmap are replaced with those of another;
		// the toolbar's own list holds a single bitmap here, so all of them.
		if(s != nullptr && lp != 0)
		{
			const auto *replace = reinterpret_cast<const TBREPLACEBITMAP *>(lp);
			if(s->ownImages != nullptr && s->images == s->ownImages)
				ImageList_RemoveAll(s->ownImages);
			TBADDBITMAP add{replace->hInstNew, replace->nIDNew};
			return AddBitmap(hwnd, *s, replace->nButtons, &add) >= 0;
		}
		return FALSE;
	case TB_CHANGEBITMAP:
		if(s != nullptr)
		{
			const int index = IndexOfCommand(*s, static_cast<int>(wp));
			if(index < 0)
				return FALSE;
			s->buttons[static_cast<std::size_t>(index)].image = static_cast<int>(LOWORD(lp)) == 0xFFFF ? -1 : static_cast<short>(LOWORD(lp));
			::InvalidateRect(hwnd, nullptr, TRUE);
			return TRUE;
		}
		return FALSE;
	case TB_GETBITMAP:
		if(s != nullptr)
		{
			const int index = IndexOfCommand(*s, static_cast<int>(wp));
			return index < 0 ? 0 : s->buttons[static_cast<std::size_t>(index)].image;
		}
		return 0;
	case TB_ADDSTRING:
		if(s != nullptr)
		{
			const int first = static_cast<int>(s->strings.size());
			if(wp != 0)
			{
				// A string resource; the first character separates the strings.
				wchar_t buffer[1024] = {};
				const int len = ::LoadString(reinterpret_cast<HINSTANCE>(wp), static_cast<UINT>(lp), buffer, 1024);
				if(len > 0)
				{
					const wchar_t sep = buffer[len - 1];
					std::wstring current;
					for(int i = 0; i < len - 1; i++)
					{
						if(buffer[i] == sep)
						{
							s->strings.push_back(current);
							current.clear();
						} else
						{
							current.push_back(buffer[i]);
						}
					}
					if(!current.empty())
						s->strings.push_back(current);
				}
			} else if(lp != 0)
			{
				// Strings one after the other, ended by an empty one.
				for(auto *p = reinterpret_cast<LPCWSTR>(lp); *p != 0; p += std::wcslen(p) + 1)
					s->strings.emplace_back(p);
			}
			return first;
		}
		return -1;
	case TB_GETBUTTONTEXT:
		if(s != nullptr)
		{
			const int index = IndexOfCommand(*s, static_cast<int>(wp));
			if(index < 0)
				return -1;
			const std::wstring &t = s->buttons[static_cast<std::size_t>(index)].text;
			if(lp != 0)
			{
				std::copy(t.begin(), t.end(), reinterpret_cast<LPWSTR>(lp));
				reinterpret_cast<LPWSTR>(lp)[t.size()] = 0;
			}
			return static_cast<LRESULT>(t.size());
		}
		return -1;
	case TB_SETMAXTEXTROWS:
		if(s != nullptr)
		{
			s->maxTextRows = std::max(0, static_cast<int>(wp));
			Layout(hwnd, *s);
			return TRUE;
		}
		return FALSE;
	case TB_GETTEXTROWS:
		return s ? s->maxTextRows : 0;
	case TB_AUTOSIZE:
		if(s != nullptr)
			AutoSize(hwnd, *s);
		return 0;
	case TB_GETMAXSIZE:
	case TB_GETIDEALSIZE:
		if(s != nullptr && lp != 0)
		{
			const QSize ideal = IdealSize(hwnd, *s);
			auto *size = reinterpret_cast<SIZE *>(lp);
			if(msg == TB_GETIDEALSIZE)
			{
				if(wp)
					size->cy = ideal.height();
				else
					size->cx = ideal.width();
			} else
			{
				size->cx = ideal.width();
				size->cy = ideal.height();
			}
			return TRUE;
		}
		return FALSE;
	case TB_HITTEST:
		if(s != nullptr && lp != 0)
		{
			Layout(hwnd, *s);
			const POINT pt = *reinterpret_cast<const POINT *>(lp);
			const int index = ButtonAt(*s, pt);
			if(index < 0)
				return -1 - static_cast<int>(s->buttons.size());
			// Separators answer negative indices.
			return IsSeparator(s->buttons[static_cast<std::size_t>(index)]) ? -index - 1 : index;
		}
		return -1;
	case TB_GETHOTITEM:
		return s ? s->hot : -1;
	case TB_SETHOTITEM:
		if(s != nullptr)
		{
			const int old = s->hot;
			const int index = static_cast<int>(wp) < static_cast<int>(s->buttons.size()) ? static_cast<int>(wp) : -1;
			SetHot(hwnd, *s, index, HICF_OTHER);
			return old;
		}
		return -1;
	case TB_GETTOOLTIPS:
		if(s != nullptr)
			EnsureTooltip(hwnd, *s);
		s = Control<ToolbarState>(hwnd);
		return s ? reinterpret_cast<LRESULT>(s->tooltip) : 0;
	case TB_SETTOOLTIPS:
		if(s != nullptr)
		{
			s->tooltip = reinterpret_cast<HWND>(wp);
			s->tooltipTried = true;
		}
		return 0;
	case TB_SETPARENT:
		if(s != nullptr)
		{
			const HWND old = NotifyWindow(hwnd, *s);
			s->notify = reinterpret_cast<HWND>(wp);
			return reinterpret_cast<LRESULT>(old);
		}
		return 0;
	case TB_GETROWS:
		if(s != nullptr)
			Layout(hwnd, *s);
		return s ? s->rows : 0;
	case TB_SETROWS:
		return 0;
	case TB_SETCMDID:
		if(s != nullptr && wp < s->buttons.size())
		{
			s->buttons[wp].command = static_cast<int>(lp);
			return TRUE;
		}
		return FALSE;
	case TB_SETDRAWTEXTFLAGS:
	case TB_SETANCHORHIGHLIGHT:
	case TB_GETANCHORHIGHLIGHT:
	case TB_SETINSERTMARK:
	case TB_GETINSERTMARK:
	case TB_CUSTOMIZE:
		return 0;
	default:
		return ::DefWindowProc(hwnd, msg, wp, lp);
	}
}

} // namespace

void RegisterToolbarClass()
{
	WNDCLASS wc{};
	wc.style = CS_DBLCLKS;
	wc.lpfnWndProc = ToolbarProc;
	wc.hCursor = ::LoadCursor(nullptr, IDC_ARROW);
	wc.lpszClassName = TOOLBARCLASSNAME;
	::RegisterClass(&wc);
}

} // namespace linopenmpt::qtwin
