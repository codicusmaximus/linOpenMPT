// The built-in window classes of USER: buttons, statics and scroll bar
// controls (edits, list boxes and combo boxes have files of their own).
//
// As on Windows, they are window procedures: upstream subclasses them,
// sends them messages and gets the same notifications back, and owner-drawn
// ones are drawn by their parent. Their look comes from the Qt style, drawn
// through the control's DC, so they match the rest of the desktop.
#include "QtWin.h"

#include <QtWidgets/QApplication>
#include <QtWidgets/QScrollBar>
#include <QtWidgets/QStyle>
#include <QtWidgets/QStyleOption>

#include <algorithm>

namespace linopenmpt::qtwin
{

void NotifyParent(HWND ctl, UINT code)
{
	const HWND parent = ::GetParent(ctl);
	if(parent != nullptr)
		::SendMessage(parent, WM_COMMAND, MAKEWPARAM(::GetDlgCtrlID(ctl), code), reinterpret_cast<LPARAM>(ctl));
}

HBRUSH ControlBrush(HWND ctl, HDC dc, UINT msg)
{
	HWND parent = ::GetParent(ctl);
	if(parent == nullptr)
		parent = ctl;
	auto brush = reinterpret_cast<HBRUSH>(::SendMessage(parent, msg, reinterpret_cast<WPARAM>(dc), reinterpret_cast<LPARAM>(ctl)));
	if(brush == nullptr)
		brush = reinterpret_cast<HBRUSH>(DefCtlColor(dc, msg));
	return brush;
}

LRESULT DefCtlColor(HDC dc, UINT msg)
{
	if(msg == WM_CTLCOLORSCROLLBAR)
	{
		::SetBkColor(dc, ::GetSysColor(COLOR_WINDOW));
		::SetTextColor(dc, ::GetSysColor(COLOR_3DFACE));
		return reinterpret_cast<LRESULT>(::GetSysColorBrush(COLOR_SCROLLBAR));
	}
	::SetTextColor(dc, ::GetSysColor(COLOR_WINDOWTEXT));
	if(msg == WM_CTLCOLOREDIT || msg == WM_CTLCOLORLISTBOX)
	{
		::SetBkColor(dc, ::GetSysColor(COLOR_WINDOW));
		return reinterpret_cast<LRESULT>(::GetSysColorBrush(COLOR_WINDOW));
	}
	::SetBkColor(dc, ::GetSysColor(COLOR_3DFACE));
	return reinterpret_cast<LRESULT>(::GetSysColorBrush(COLOR_3DFACE));
}

namespace
{

QString Text(HWND hwnd)
{
	const WindowData *d = Data(hwnd);
	return d != nullptr ? ToQString(d->text.c_str()) : QString();
}

HFONT ControlFont(HWND hwnd)
{
	const WindowData *d = Data(hwnd);
	return d != nullptr && d->font != nullptr ? d->font : static_cast<HFONT>(::GetStockObject(DEFAULT_GUI_FONT));
}

RECT ClientRectOf(HWND hwnd)
{
	RECT rc{};
	::GetClientRect(hwnd, &rc);
	return rc;
}

QRect ToRect(const RECT &r)
{
	return QRect(QPoint(r.left, r.top), QPoint(r.right - 1, r.bottom - 1));
}

// Paints a control: WM_PAINT and WM_PRINTCLIENT.
template <typename PaintFn>
LRESULT PaintMessage(HWND hwnd, UINT msg, WPARAM wp, PaintFn paint)
{
	if(msg == WM_PRINTCLIENT)
	{
		paint(reinterpret_cast<HDC>(wp));
		return 0;
	}
	PAINTSTRUCT ps;
	HDC dc = ::BeginPaint(hwnd, &ps);
	if(dc != nullptr)
	{
		HGDIOBJ oldFont = ::SelectObject(dc, ControlFont(hwnd));
		paint(dc);
		::SelectObject(dc, oldFont);
	}
	::EndPaint(hwnd, &ps);
	return 0;
}

void SetStyleBits(HWND hwnd, LONG_PTR style)
{
	if(WindowData *d = Data(hwnd))
		d->style = static_cast<DWORD>(style) & ~WS_VISIBLE;
}

// ---- buttons ---------------------------------------------------------------------------

struct ButtonState : ControlState
{
	UINT check = BST_UNCHECKED;
	bool pushed = false;
	bool tracking = false; // mouse button held
	bool spaceDown = false;
	bool focused = false;
	HANDLE image = nullptr;
	UINT imageType = IMAGE_BITMAP;
};

UINT ButtonType(HWND hwnd)
{
	return static_cast<UINT>(::GetWindowLongPtr(hwnd, GWL_STYLE) & BS_TYPEMASK);
}

bool IsCheckType(UINT type)
{
	return type == BS_CHECKBOX || type == BS_AUTOCHECKBOX || type == BS_3STATE || type == BS_AUTO3STATE;
}

bool IsRadioType(UINT type)
{
	return type == BS_RADIOBUTTON || type == BS_AUTORADIOBUTTON;
}

bool IsPushType(UINT type)
{
	return type == BS_PUSHBUTTON || type == BS_DEFPUSHBUTTON || type == BS_SPLITBUTTON || type == BS_DEFSPLITBUTTON || type == BS_COMMANDLINK || type == BS_DEFCOMMANDLINK || type == BS_USERBUTTON || type == BS_PUSHBOX;
}

void SetPushed(HWND hwnd, ButtonState &s, bool pushed)
{
	if(s.pushed == pushed)
		return;
	s.pushed = pushed;
	::InvalidateRect(hwnd, nullptr, FALSE);
}

void SetCheck(HWND hwnd, ButtonState &s, UINT check)
{
	const UINT type = ButtonType(hwnd);
	if(!IsCheckType(type) && !IsRadioType(type))
		return;
	if(check > BST_INDETERMINATE || (check == BST_INDETERMINATE && type != BS_3STATE && type != BS_AUTO3STATE))
		check = BST_CHECKED;
	if(IsRadioType(type))
	{
		// Checked radio buttons are tab stops, as on Windows.
		LONG_PTR style = ::GetWindowLongPtr(hwnd, GWL_STYLE);
		style = check ? (style | WS_TABSTOP) : (style & ~WS_TABSTOP);
		SetStyleBits(hwnd, style);
	}
	if(s.check != check)
	{
		s.check = check;
		::InvalidateRect(hwnd, nullptr, FALSE);
	}
}

void Click(HWND hwnd)
{
	ButtonState *s = Control<ButtonState>(hwnd);
	if(s == nullptr)
		return;
	switch(ButtonType(hwnd))
	{
	case BS_AUTOCHECKBOX:
		SetCheck(hwnd, *s, s->check ? BST_UNCHECKED : BST_CHECKED);
		break;
	case BS_AUTO3STATE:
		SetCheck(hwnd, *s, (s->check + 1) % 3);
		break;
	case BS_AUTORADIOBUTTON:
		{
			// Unchecks the other automatic radio buttons of its group.
			const HWND parent = ::GetParent(hwnd);
			HWND first = hwnd;
			while(!(::GetWindowLongPtr(first, GWL_STYLE) & WS_GROUP))
			{
				const HWND prev = ::GetWindow(first, GW_HWNDPREV);
				if(prev == nullptr)
					break;
				first = prev;
			}
			for(HWND c = first; c != nullptr; c = ::GetWindow(c, GW_HWNDNEXT))
			{
				if(c != first && (::GetWindowLongPtr(c, GWL_STYLE) & WS_GROUP))
					break;
				if(c != hwnd && ::GetParent(c) == parent && (::GetWindowLongPtr(c, GWL_STYLE) & BS_TYPEMASK) == BS_AUTORADIOBUTTON && Control<ButtonState>(c))
					::SendMessage(c, BM_SETCHECK, BST_UNCHECKED, 0);
			}
			if((s = Control<ButtonState>(hwnd)) != nullptr)
				SetCheck(hwnd, *s, BST_CHECKED);
		}
		break;
	default:
		break;
	}
	NotifyParent(hwnd, BN_CLICKED);
}

QIcon ButtonIcon(const ButtonState &s, QSize &size)
{
	if(s.image == nullptr)
		return QIcon();
	QImage image;
	if(s.imageType == IMAGE_ICON || s.imageType == IMAGE_CURSOR)
	{
		const auto *icon = reinterpret_cast<const IconData *>(s.image);
		image = icon->image;
	} else
	{
		image = BitmapImage(static_cast<HBITMAP>(s.image));
	}
	if(image.isNull())
		return QIcon();
	size = (QSizeF(image.size()) / image.devicePixelRatio()).toSize();
	return QIcon(QPixmap::fromImage(image));
}

void PaintButton(HWND hwnd, HDC dc)
{
	Win32Widget *w = FromHandle(hwnd);
	ButtonState *s = Control<ButtonState>(hwnd);
	if(w == nullptr || s == nullptr)
		return;
	const LONG_PTR style = ::GetWindowLongPtr(hwnd, GWL_STYLE);
	const UINT type = static_cast<UINT>(style & BS_TYPEMASK);
	const RECT rc = ClientRectOf(hwnd);
	const bool enabled = ::IsWindowEnabled(hwnd) != FALSE;

	if(type == BS_OWNERDRAW)
	{
		HBRUSH brush = ControlBrush(hwnd, dc, WM_CTLCOLORBTN);
		::FillRect(dc, &rc, brush);
		DRAWITEMSTRUCT dis{};
		dis.CtlType = ODT_BUTTON;
		dis.CtlID = static_cast<UINT>(::GetDlgCtrlID(hwnd));
		dis.itemAction = ODA_DRAWENTIRE;
		dis.itemState = (s->pushed ? ODS_SELECTED : 0) | (s->focused ? ODS_FOCUS : 0) | (enabled ? 0 : ODS_DISABLED);
		dis.hwndItem = hwnd;
		dis.hDC = dc;
		dis.rcItem = rc;
		::SendMessage(::GetParent(hwnd), WM_DRAWITEM, dis.CtlID, reinterpret_cast<LPARAM>(&dis));
		return;
	}

	const bool pushLike = IsPushType(type) || ((IsCheckType(type) || IsRadioType(type)) && (style & BS_PUSHLIKE));
	HBRUSH brush = ControlBrush(hwnd, dc, pushLike ? WM_CTLCOLORBTN : WM_CTLCOLORSTATIC);
	if(type != BS_GROUPBOX)
		::FillRect(dc, &rc, brush);
	const COLORREF textColor = ::GetTextColor(dc);

	QPainter *p = DCPainter(dc);
	if(p == nullptr)
		return;
	QStyle *qs = QApplication::style();
	const QFont font = DCFont(dc);
	const QRect rect = ToRect(rc);
	const QString text = Text(hwnd);
	p->save();
	p->setFont(font);
	auto initOption = [&](QStyleOption &opt)
	{
		opt.initFrom(w);
		opt.rect = rect;
		opt.fontMetrics = QFontMetrics(font);
		opt.state &= ~(QStyle::State_HasFocus | QStyle::State_MouseOver);
		if(enabled)
			opt.state |= QStyle::State_Enabled;
		else
			opt.state &= ~QStyle::State_Enabled;
		if(s->focused)
			opt.state |= QStyle::State_HasFocus | QStyle::State_KeyboardFocusChange;
		if(enabled)
		{
			opt.palette.setColor(QPalette::ButtonText, ToQColor(textColor));
			opt.palette.setColor(QPalette::WindowText, ToQColor(textColor));
		}
	};

	if(type == BS_GROUPBOX)
	{
		// Drawn as Windows draws it (an etched frame with the caption on its
		// top line, nothing filled), which upstream's layouts are made for:
		// controls and statics inside it sit on the dialog's background.
		p->restore();
		const std::wstring caption = Data(hwnd) != nullptr ? Data(hwnd)->text : std::wstring();
		TEXTMETRIC tm{};
		::GetTextMetrics(dc, &tm);
		RECT frame = rc;
		frame.top += tm.tmHeight / 2;
		::DrawEdge(dc, &frame, EDGE_ETCHED, BF_RECT);
		if(!caption.empty())
		{
			RECT textRect{0, 0, 0, 0};
			::DrawText(dc, caption.c_str(), -1, &textRect, DT_SINGLELINE | DT_CALCRECT);
			const int width = textRect.right + 4;
			int left = rc.left + 8;
			if((style & BS_CENTER) == BS_CENTER)
				left = rc.left + (rc.right - rc.left - width) / 2;
			else if(style & BS_RIGHT)
				left = rc.right - 8 - width;
			RECT back{left, rc.top, std::min(static_cast<LONG>(left + width), rc.right), rc.top + tm.tmHeight};
			::FillRect(dc, &back, brush);
			::SetBkMode(dc, TRANSPARENT);
			::SetTextColor(dc, enabled ? textColor : ::GetSysColor(COLOR_GRAYTEXT));
			back.left += 2;
			::DrawText(dc, caption.c_str(), -1, &back, DT_SINGLELINE | DT_LEFT | DT_TOP);
		}
		return;
	} else if(pushLike)
	{
		QStyleOptionButton opt;
		initOption(opt);
		opt.text = text;
		opt.state |= s->pushed ? QStyle::State_Sunken : QStyle::State_Raised;
		if(!IsPushType(type) && s->check)
			opt.state |= QStyle::State_On;
		if(type == BS_DEFPUSHBUTTON || type == BS_DEFSPLITBUTTON || type == BS_DEFCOMMANDLINK)
			opt.features |= QStyleOptionButton::DefaultButton;
		if(style & BS_FLAT)
			opt.features |= QStyleOptionButton::Flat;
		if(style & (BS_BITMAP | BS_ICON))
		{
			QSize size;
			opt.icon = ButtonIcon(*s, size);
			opt.iconSize = size;
			if(!opt.icon.isNull())
				opt.text.clear();
		}
		if(style & BS_MULTILINE)
		{
			qs->drawControl(QStyle::CE_PushButtonBevel, &opt, p, w);
			const QRect contents = qs->subElementRect(QStyle::SE_PushButtonContents, &opt, w);
			int flags = Qt::TextWordWrap | Qt::TextShowMnemonic | Qt::AlignVCenter;
			flags |= (style & BS_CENTER) == BS_LEFT ? Qt::AlignLeft : ((style & BS_CENTER) == BS_RIGHT ? Qt::AlignRight : Qt::AlignHCenter);
			qs->drawItemText(p, contents.translated(s->pushed ? 1 : 0, s->pushed ? 1 : 0), flags, opt.palette, enabled, opt.text, QPalette::ButtonText);
			if(s->focused)
			{
				QStyleOptionFocusRect focus;
				focus.QStyleOption::operator=(opt);
				focus.rect = qs->subElementRect(QStyle::SE_PushButtonFocusRect, &opt, w);
				qs->drawPrimitive(QStyle::PE_FrameFocusRect, &focus, p, w);
			}
		} else
		{
			qs->drawControl(QStyle::CE_PushButton, &opt, p, w);
		}
	} else
	{
		QStyleOptionButton opt;
		initOption(opt);
		opt.text = text;
		if(s->check == BST_CHECKED)
			opt.state |= QStyle::State_On;
		else if(s->check == BST_INDETERMINATE)
			opt.state |= QStyle::State_NoChange;
		else
			opt.state |= QStyle::State_Off;
		if(s->pushed)
			opt.state |= QStyle::State_Sunken;
		if(style & BS_LEFTTEXT)
			opt.direction = opt.direction == Qt::LeftToRight ? Qt::RightToLeft : Qt::LeftToRight;
		if(style & (BS_BITMAP | BS_ICON))
		{
			QSize size;
			opt.icon = ButtonIcon(*s, size);
			opt.iconSize = size;
			if(!opt.icon.isNull())
				opt.text.clear();
		}
		qs->drawControl(IsRadioType(type) ? QStyle::CE_RadioButton : QStyle::CE_CheckBox, &opt, p, w);
	}
	p->restore();
}

LRESULT CALLBACK ButtonProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
	ButtonState *s = Control<ButtonState>(hwnd);
	switch(msg)
	{
	case WM_NCCREATE:
		if(WindowData *d = Data(hwnd))
		{
			d->control = std::make_unique<ButtonState>();
			if((d->style & BS_TYPEMASK) == BS_GROUPBOX)
			{
				// Group boxes are drawn over by the controls inside them, and
				// clicks go through to those.
				SetTransparent(d->widget);
				d->widget->setAttribute(Qt::WA_TransparentForMouseEvents);
			}
		}
		return ::DefWindowProc(hwnd, msg, wp, lp);
	case WM_GETDLGCODE:
		switch(ButtonType(hwnd))
		{
		case BS_DEFPUSHBUTTON:
		case BS_DEFSPLITBUTTON:
		case BS_DEFCOMMANDLINK:
			return DLGC_BUTTON | DLGC_DEFPUSHBUTTON;
		case BS_PUSHBUTTON:
		case BS_SPLITBUTTON:
		case BS_COMMANDLINK:
		case BS_USERBUTTON:
		case BS_PUSHBOX:
			return DLGC_BUTTON | DLGC_UNDEFPUSHBUTTON;
		case BS_RADIOBUTTON:
		case BS_AUTORADIOBUTTON:
			return DLGC_BUTTON | DLGC_RADIOBUTTON;
		case BS_GROUPBOX:
			return DLGC_STATIC;
		default:
			return DLGC_BUTTON;
		}
	case WM_NCHITTEST:
		if(ButtonType(hwnd) == BS_GROUPBOX)
			return HTTRANSPARENT;
		return ::DefWindowProc(hwnd, msg, wp, lp);
	case WM_ERASEBKGND:
		return 1;
	case WM_PAINT:
	case WM_PRINTCLIENT:
		return PaintMessage(hwnd, msg, wp, [hwnd](HDC dc) { PaintButton(hwnd, dc); });
	case WM_ENABLE:
		::InvalidateRect(hwnd, nullptr, FALSE);
		return 0;
	case WM_SETTEXT:
		{
			const LRESULT r = ::DefWindowProc(hwnd, msg, wp, lp);
			::InvalidateRect(hwnd, nullptr, FALSE);
			return r;
		}
	case WM_SETFONT:
		::DefWindowProc(hwnd, msg, wp, 0);
		if(lp)
			::InvalidateRect(hwnd, nullptr, FALSE);
		return 0;
	case WM_LBUTTONDBLCLK:
		if(s != nullptr)
		{
			const UINT type = ButtonType(hwnd);
			if(IsRadioType(type) || type == BS_OWNERDRAW || (::GetWindowLongPtr(hwnd, GWL_STYLE) & BS_NOTIFY))
			{
				NotifyParent(hwnd, BN_DOUBLECLICKED);
				return 0;
			}
		}
		[[fallthrough]];
	case WM_LBUTTONDOWN:
		if(s != nullptr && ButtonType(hwnd) != BS_GROUPBOX)
		{
			::SetCapture(hwnd);
			if(::GetFocus() != hwnd)
				::SetFocus(hwnd);
			if((s = Control<ButtonState>(hwnd)) != nullptr)
			{
				s->tracking = true;
				SetPushed(hwnd, *s, true);
			}
		}
		return 0;
	case WM_MOUSEMOVE:
		if(s != nullptr && s->tracking)
		{
			const RECT rc = ClientRectOf(hwnd);
			const POINT pt{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
			SetPushed(hwnd, *s, ::PtInRect(&rc, pt) != FALSE);
		}
		return 0;
	case WM_LBUTTONUP:
		if(s != nullptr && s->tracking)
		{
			s->tracking = false;
			const bool clicked = s->pushed;
			SetPushed(hwnd, *s, false);
			::ReleaseCapture();
			if(clicked)
				Click(hwnd);
		}
		return 0;
	case WM_CAPTURECHANGED:
		if(s != nullptr && s->tracking)
		{
			s->tracking = false;
			SetPushed(hwnd, *s, false);
		}
		return 0;
	case WM_KEYDOWN:
		if(s != nullptr && wp == VK_SPACE && !(lp & 0x40000000))
		{
			s->spaceDown = true;
			SetPushed(hwnd, *s, true);
			return 0;
		}
		return ::DefWindowProc(hwnd, msg, wp, lp);
	case WM_KEYUP:
		if(s != nullptr && wp == VK_SPACE && s->spaceDown)
		{
			s->spaceDown = false;
			const bool clicked = s->pushed;
			SetPushed(hwnd, *s, false);
			if(clicked)
				Click(hwnd);
			return 0;
		}
		return ::DefWindowProc(hwnd, msg, wp, lp);
	case WM_CHAR:
		// Checkboxes take + and - as well.
		if(s != nullptr && IsCheckType(ButtonType(hwnd)) && (wp == '+' || wp == '=' || wp == '-'))
		{
			const UINT want = wp == '-' ? BST_UNCHECKED : BST_CHECKED;
			if(s->check != want)
				Click(hwnd);
			return 0;
		}
		return ::DefWindowProc(hwnd, msg, wp, lp);
	case WM_SETFOCUS:
		if(s != nullptr)
		{
			s->focused = true;
			::InvalidateRect(hwnd, nullptr, FALSE);
			if(::GetWindowLongPtr(hwnd, GWL_STYLE) & BS_NOTIFY)
				NotifyParent(hwnd, BN_SETFOCUS);
		}
		return 0;
	case WM_KILLFOCUS:
		if(s != nullptr)
		{
			s->focused = false;
			if(s->spaceDown)
			{
				s->spaceDown = false;
				SetPushed(hwnd, *s, false);
			}
			::InvalidateRect(hwnd, nullptr, FALSE);
			if(::GetWindowLongPtr(hwnd, GWL_STYLE) & BS_NOTIFY)
				NotifyParent(hwnd, BN_KILLFOCUS);
		}
		return 0;
	case BM_GETCHECK:
		return s != nullptr ? s->check : 0;
	case BM_SETCHECK:
		if(s != nullptr)
			SetCheck(hwnd, *s, static_cast<UINT>(wp));
		return 0;
	case BM_GETSTATE:
		return s != nullptr ? (s->check | (s->pushed ? BST_PUSHED : 0) | (s->focused ? BST_FOCUS : 0)) : 0;
	case BM_SETSTATE:
		if(s != nullptr)
			SetPushed(hwnd, *s, wp != 0);
		return 0;
	case BM_SETSTYLE:
		SetStyleBits(hwnd, (::GetWindowLongPtr(hwnd, GWL_STYLE) & ~LONG_PTR(0xFFFF)) | (wp & 0xFFFF));
		if(lp)
			::InvalidateRect(hwnd, nullptr, FALSE);
		return 0;
	case BM_CLICK:
		if(s != nullptr && ::IsWindowEnabled(hwnd) && ButtonType(hwnd) != BS_GROUPBOX)
			Click(hwnd);
		return 0;
	case BM_GETIMAGE:
		return s != nullptr && s->imageType == wp ? reinterpret_cast<LRESULT>(s->image) : 0;
	case BM_SETIMAGE:
		if(s != nullptr)
		{
			const HANDLE old = s->image;
			s->image = reinterpret_cast<HANDLE>(lp);
			s->imageType = static_cast<UINT>(wp);
			::InvalidateRect(hwnd, nullptr, FALSE);
			return reinterpret_cast<LRESULT>(old);
		}
		return 0;
	case BM_SETDONTCLICK:
		return 0;
	default:
		return ::DefWindowProc(hwnd, msg, wp, lp);
	}
}

// ---- statics -----------------------------------------------------------------------------

struct StaticState : ControlState
{
	HANDLE image = nullptr;
	UINT imageType = IMAGE_BITMAP;
};

UINT StaticType(HWND hwnd)
{
	return static_cast<UINT>(::GetWindowLongPtr(hwnd, GWL_STYLE) & SS_TYPEMASK);
}

void SizeToImage(HWND hwnd, const StaticState &s)
{
	const LONG_PTR style = ::GetWindowLongPtr(hwnd, GWL_STYLE);
	if(s.image == nullptr || (style & (SS_CENTERIMAGE | SS_REALSIZECONTROL)))
		return;
	SIZE size{};
	if(s.imageType == IMAGE_BITMAP)
	{
		BITMAP bm{};
		if(::GetObject(s.image, sizeof(bm), &bm))
			size = SIZE{bm.bmWidth, bm.bmHeight};
	} else if(const auto *icon = reinterpret_cast<const IconData *>(s.image))
	{
		const QSize logical = (QSizeF(icon->image.size()) / icon->image.devicePixelRatio()).toSize();
		size = SIZE{logical.width(), logical.height()};
	}
	if(size.cx > 0 && size.cy > 0)
		::SetWindowPos(hwnd, nullptr, 0, 0, size.cx, size.cy, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

void PaintStatic(HWND hwnd, HDC dc)
{
	StaticState *s = Control<StaticState>(hwnd);
	if(s == nullptr)
		return;
	const LONG_PTR style = ::GetWindowLongPtr(hwnd, GWL_STYLE);
	const UINT type = static_cast<UINT>(style & SS_TYPEMASK);
	RECT rc = ClientRectOf(hwnd);
	if(type == SS_OWNERDRAW)
	{
		DRAWITEMSTRUCT dis{};
		dis.CtlType = ODT_STATIC;
		dis.CtlID = static_cast<UINT>(::GetDlgCtrlID(hwnd));
		dis.itemAction = ODA_DRAWENTIRE;
		dis.itemState = ::IsWindowEnabled(hwnd) ? 0 : ODS_DISABLED;
		dis.hwndItem = hwnd;
		dis.hDC = dc;
		dis.rcItem = rc;
		::SendMessage(::GetParent(hwnd), WM_DRAWITEM, dis.CtlID, reinterpret_cast<LPARAM>(&dis));
		return;
	}
	HBRUSH brush = ControlBrush(hwnd, dc, WM_CTLCOLORSTATIC);
	switch(type)
	{
	case SS_BLACKRECT: ::FillRect(dc, &rc, ::GetSysColorBrush(COLOR_3DDKSHADOW)); return;
	case SS_GRAYRECT: ::FillRect(dc, &rc, ::GetSysColorBrush(COLOR_3DSHADOW)); return;
	case SS_WHITERECT: ::FillRect(dc, &rc, ::GetSysColorBrush(COLOR_3DHIGHLIGHT)); return;
	case SS_BLACKFRAME: ::FrameRect(dc, &rc, ::GetSysColorBrush(COLOR_3DDKSHADOW)); return;
	case SS_GRAYFRAME: ::FrameRect(dc, &rc, ::GetSysColorBrush(COLOR_3DSHADOW)); return;
	case SS_WHITEFRAME: ::FrameRect(dc, &rc, ::GetSysColorBrush(COLOR_3DHIGHLIGHT)); return;
	default: break;
	}
	::FillRect(dc, &rc, brush);
	switch(type)
	{
	case SS_ETCHEDHORZ:
		::DrawEdge(dc, &rc, EDGE_ETCHED, BF_TOP);
		return;
	case SS_ETCHEDVERT:
		::DrawEdge(dc, &rc, EDGE_ETCHED, BF_LEFT);
		return;
	case SS_ETCHEDFRAME:
		::DrawEdge(dc, &rc, EDGE_ETCHED, BF_RECT);
		return;
	case SS_ICON:
		if(s->image != nullptr)
		{
			const auto *icon = reinterpret_cast<const IconData *>(s->image);
			QSize size(rc.right, rc.bottom);
			int x = 0, y = 0;
			if(style & SS_CENTERIMAGE)
			{
				size = (QSizeF(icon->image.size()) / icon->image.devicePixelRatio()).toSize();
				x = (rc.right - size.width()) / 2;
				y = (rc.bottom - size.height()) / 2;
			}
			::DrawIconEx(dc, x, y, static_cast<HICON>(s->image), size.width(), size.height(), 0, nullptr, DI_NORMAL);
		}
		return;
	case SS_BITMAP:
		if(s->image != nullptr)
		{
			BITMAP bm{};
			::GetObject(s->image, sizeof(bm), &bm);
			HDC mem = ::CreateCompatibleDC(dc);
			HGDIOBJ old = ::SelectObject(mem, s->image);
			int x = 0, y = 0;
			if(style & SS_CENTERIMAGE)
			{
				x = (rc.right - bm.bmWidth) / 2;
				y = (rc.bottom - bm.bmHeight) / 2;
			}
			::BitBlt(dc, x, y, bm.bmWidth, bm.bmHeight, mem, 0, 0, SRCCOPY);
			::SelectObject(mem, old);
			::DeleteDC(mem);
		}
		return;
	default:
		break;
	}
	// Text.
	UINT flags = DT_EXPANDTABS;
	switch(type)
	{
	case SS_CENTER: flags |= DT_CENTER | DT_WORDBREAK; break;
	case SS_RIGHT: flags |= DT_RIGHT | DT_WORDBREAK; break;
	case SS_SIMPLE: flags |= DT_SINGLELINE | DT_NOCLIP; break;
	case SS_LEFTNOWORDWRAP: flags |= DT_SINGLELINE; break;
	default: flags |= DT_WORDBREAK; break;
	}
	if(style & SS_NOPREFIX)
		flags |= DT_NOPREFIX;
	if(style & SS_CENTERIMAGE)
		flags = (flags & ~DT_WORDBREAK) | DT_SINGLELINE | DT_VCENTER;
	switch(style & SS_ELLIPSISMASK)
	{
	case SS_ENDELLIPSIS: flags = (flags & ~DT_WORDBREAK) | DT_END_ELLIPSIS | DT_SINGLELINE; break;
	case SS_PATHELLIPSIS: flags = (flags & ~DT_WORDBREAK) | DT_PATH_ELLIPSIS | DT_SINGLELINE; break;
	case SS_WORDELLIPSIS: flags = (flags & ~DT_WORDBREAK) | DT_WORD_ELLIPSIS | DT_SINGLELINE; break;
	default: break;
	}
	if(!::IsWindowEnabled(hwnd))
		::SetTextColor(dc, ::GetSysColor(COLOR_GRAYTEXT));
	const WindowData *d = Data(hwnd);
	if(d != nullptr && !d->text.empty())
		::DrawText(dc, d->text.c_str(), -1, &rc, flags);
}

LRESULT CALLBACK StaticProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
	StaticState *s = Control<StaticState>(hwnd);
	switch(msg)
	{
	case WM_NCCREATE:
		if(WindowData *d = Data(hwnd))
		{
			d->control = std::make_unique<StaticState>();
			if(d->style & SS_SUNKEN)
				d->exStyle |= WS_EX_STATICEDGE;
			// Clicks go through to what is below, unless SS_NOTIFY.
			if(!(d->style & SS_NOTIFY))
				d->widget->setAttribute(Qt::WA_TransparentForMouseEvents);
			const auto *cs = reinterpret_cast<const CREATESTRUCT *>(lp);
			const UINT type = d->style & SS_TYPEMASK;
			if((type == SS_ICON || type == SS_BITMAP) && cs != nullptr && cs->lpszName != nullptr && cs->lpszName[0] != 0)
			{
				// The window name names the image: {0xFFFF, id} or a name.
				LPCTSTR name = cs->lpszName[0] == 0xFFFF ? MAKEINTRESOURCE(cs->lpszName[1]) : cs->lpszName;
				auto *state = static_cast<StaticState *>(d->control.get());
				if(type == SS_ICON)
				{
					state->image = ::LoadIcon(cs->hInstance, name);
					state->imageType = IMAGE_ICON;
				} else
				{
					state->image = ::LoadImage(cs->hInstance, name, IMAGE_BITMAP, 0, 0, 0);
					state->imageType = IMAGE_BITMAP;
				}
				d->text.clear();
				return TRUE;
			}
		}
		return ::DefWindowProc(hwnd, msg, wp, lp);
	case WM_CREATE:
		if(s != nullptr)
			SizeToImage(hwnd, *s);
		return 0;
	case WM_GETDLGCODE:
		return DLGC_STATIC;
	case WM_NCHITTEST:
		if(!(::GetWindowLongPtr(hwnd, GWL_STYLE) & SS_NOTIFY))
			return HTTRANSPARENT;
		return ::DefWindowProc(hwnd, msg, wp, lp);
	case WM_ERASEBKGND:
		return 1;
	case WM_PAINT:
	case WM_PRINTCLIENT:
		return PaintMessage(hwnd, msg, wp, [hwnd](HDC dc) { PaintStatic(hwnd, dc); });
	case WM_ENABLE:
		::InvalidateRect(hwnd, nullptr, FALSE);
		if(::GetWindowLongPtr(hwnd, GWL_STYLE) & SS_NOTIFY)
			NotifyParent(hwnd, wp ? STN_ENABLE : STN_DISABLE);
		return 0;
	case WM_SETTEXT:
		{
			const UINT type = StaticType(hwnd);
			if(type == SS_ICON || type == SS_BITMAP)
				return TRUE;
			const LRESULT r = ::DefWindowProc(hwnd, msg, wp, lp);
			::InvalidateRect(hwnd, nullptr, FALSE);
			return r;
		}
	case WM_SETFONT:
		::DefWindowProc(hwnd, msg, wp, 0);
		if(lp)
			::InvalidateRect(hwnd, nullptr, FALSE);
		return 0;
	case WM_LBUTTONDOWN:
		if(::GetWindowLongPtr(hwnd, GWL_STYLE) & SS_NOTIFY)
			NotifyParent(hwnd, STN_CLICKED);
		return 0;
	case WM_LBUTTONDBLCLK:
		if(::GetWindowLongPtr(hwnd, GWL_STYLE) & SS_NOTIFY)
			NotifyParent(hwnd, STN_DBLCLK);
		return 0;
	case STM_SETICON:
	case STM_SETIMAGE:
		if(s != nullptr)
		{
			const HANDLE old = s->image;
			s->image = reinterpret_cast<HANDLE>(msg == STM_SETICON ? wp : lp);
			s->imageType = msg == STM_SETICON ? IMAGE_ICON : static_cast<UINT>(wp);
			SizeToImage(hwnd, *s);
			::InvalidateRect(hwnd, nullptr, FALSE);
			return reinterpret_cast<LRESULT>(old);
		}
		return 0;
	case STM_GETICON:
		return s != nullptr && s->imageType == IMAGE_ICON ? reinterpret_cast<LRESULT>(s->image) : 0;
	case STM_GETIMAGE:
		return s != nullptr && s->imageType == wp ? reinterpret_cast<LRESULT>(s->image) : 0;
	default:
		return ::DefWindowProc(hwnd, msg, wp, lp);
	}
}

// ---- scroll bar controls ---------------------------------------------------------------

struct ScrollBarState : ControlState
{
	ScrollState s;
	QScrollBar *bar = nullptr;
};

int MaxThumb(const ScrollState &s)
{
	return std::max(s.min, s.max - std::max<int>(static_cast<int>(s.page), 1) + 1);
}

void SyncControl(HWND hwnd, ScrollBarState &st)
{
	if(st.bar == nullptr)
		return;
	const QSignalBlocker block(st.bar);
	st.bar->setRange(st.s.min, MaxThumb(st.s));
	st.bar->setPageStep(std::max<int>(static_cast<int>(st.s.page), 1));
	if(!st.s.tracking)
		st.bar->setValue(st.s.pos);
	st.bar->setEnabled(st.s.enabled && ::IsWindowEnabled(hwnd));
}

void SendControlScroll(HWND hwnd, UINT code, int pos)
{
	const bool vert = (::GetWindowLongPtr(hwnd, GWL_STYLE) & SBS_VERT) != 0;
	const HWND parent = ::GetParent(hwnd);
	if(parent != nullptr)
		::SendMessage(parent, vert ? WM_VSCROLL : WM_HSCROLL, MAKEWPARAM(code, static_cast<WORD>(pos)), reinterpret_cast<LPARAM>(hwnd));
}

LRESULT CALLBACK ScrollBarProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
	ScrollBarState *st = Control<ScrollBarState>(hwnd);
	switch(msg)
	{
	case WM_NCCREATE:
		if(WindowData *d = Data(hwnd))
		{
			auto state = std::make_unique<ScrollBarState>();
			const bool vert = (d->style & SBS_VERT) != 0;
			auto *bar = new QScrollBar(vert ? Qt::Vertical : Qt::Horizontal, d->widget);
			bar->setFocusPolicy(Qt::NoFocus);
			state->bar = bar;
			state->s.max = 100;
			if(d->style & (SBS_SIZEBOX | SBS_SIZEGRIP))
				bar->hide();
			else
				bar->show();
			QObject::connect(bar, &QAbstractSlider::actionTriggered, bar, [hwnd, bar](int action)
			{
				ScrollBarState *st = Control<ScrollBarState>(hwnd);
				if(st == nullptr)
					return;
				UINT code;
				switch(action)
				{
				case QAbstractSlider::SliderSingleStepAdd: code = SB_LINEDOWN; break;
				case QAbstractSlider::SliderSingleStepSub: code = SB_LINEUP; break;
				case QAbstractSlider::SliderPageStepAdd: code = SB_PAGEDOWN; break;
				case QAbstractSlider::SliderPageStepSub: code = SB_PAGEUP; break;
				case QAbstractSlider::SliderToMinimum: code = SB_TOP; break;
				case QAbstractSlider::SliderToMaximum: code = SB_BOTTOM; break;
				case QAbstractSlider::SliderMove:
					st->s.tracking = true;
					st->s.trackPos = bar->sliderPosition();
					SendControlScroll(hwnd, SB_THUMBTRACK, st->s.trackPos);
					return;
				default:
					return;
				}
				SendControlScroll(hwnd, code, 0);
				SendControlScroll(hwnd, SB_ENDSCROLL, 0);
				if((st = Control<ScrollBarState>(hwnd)) != nullptr)
					bar->setSliderPosition(st->s.pos);
			});
			QObject::connect(bar, &QAbstractSlider::sliderReleased, bar, [hwnd]()
			{
				ScrollBarState *st = Control<ScrollBarState>(hwnd);
				if(st == nullptr || !st->s.tracking)
					return;
				SendControlScroll(hwnd, SB_THUMBPOSITION, st->s.trackPos);
				SendControlScroll(hwnd, SB_ENDSCROLL, 0);
				if((st = Control<ScrollBarState>(hwnd)) != nullptr)
				{
					st->s.tracking = false;
					SyncControl(hwnd, *st);
				}
			});
			d->control = std::move(state);
			SyncControl(hwnd, *Control<ScrollBarState>(hwnd));
		}
		return ::DefWindowProc(hwnd, msg, wp, lp);
	case WM_SIZE:
		if(st != nullptr && st->bar != nullptr)
		{
			const QRect client = ClientRect(FromHandle(hwnd));
			st->bar->setGeometry(client);
		}
		return 0;
	case WM_ENABLE:
		if(st != nullptr)
			SyncControl(hwnd, *st);
		return 0;
	case WM_ERASEBKGND:
		return 1;
	case WM_PAINT:
		return PaintMessage(hwnd, msg, wp, [hwnd](HDC dc)
		{
			const RECT rc = ClientRectOf(hwnd);
			::FillRect(dc, &rc, ::GetSysColorBrush(COLOR_3DFACE));
			if(::GetWindowLongPtr(hwnd, GWL_STYLE) & (SBS_SIZEBOX | SBS_SIZEGRIP))
				::DrawFrameControl(dc, const_cast<RECT *>(&rc), DFC_SCROLL, DFCS_SCROLLSIZEGRIP);
		});
	case SBM_SETSCROLLINFO:
		if(st != nullptr && lp != 0)
		{
			const auto *info = reinterpret_cast<const SCROLLINFO *>(lp);
			ScrollState &s = st->s;
			if(info->fMask & SIF_RANGE)
			{
				s.min = info->nMin;
				s.max = std::max(info->nMin, info->nMax);
			}
			if(info->fMask & SIF_PAGE)
				s.page = std::min<UINT>(info->nPage, static_cast<UINT>(s.max - s.min + 1));
			if(info->fMask & SIF_POS)
				s.pos = info->nPos;
			s.pos = std::clamp(s.pos, s.min, MaxThumb(s));
			SyncControl(hwnd, *st);
			return s.pos;
		}
		return 0;
	case SBM_GETSCROLLINFO:
		if(st != nullptr && lp != 0)
		{
			auto *info = reinterpret_cast<SCROLLINFO *>(lp);
			const ScrollState &s = st->s;
			if(info->fMask & SIF_RANGE)
			{
				info->nMin = s.min;
				info->nMax = s.max;
			}
			if(info->fMask & SIF_PAGE)
				info->nPage = s.page;
			if(info->fMask & SIF_POS)
				info->nPos = s.pos;
			if(info->fMask & SIF_TRACKPOS)
				info->nTrackPos = s.tracking ? s.trackPos : s.pos;
			return TRUE;
		}
		return FALSE;
	case SBM_SETPOS:
		if(st != nullptr)
		{
			const int old = st->s.pos;
			st->s.pos = std::clamp(static_cast<int>(wp), st->s.min, MaxThumb(st->s));
			SyncControl(hwnd, *st);
			return old;
		}
		return 0;
	case SBM_GETPOS:
		return st != nullptr ? st->s.pos : 0;
	case SBM_SETRANGE:
	case SBM_SETRANGEREDRAW:
		if(st != nullptr)
		{
			const int old = st->s.pos;
			st->s.min = static_cast<int>(wp);
			st->s.max = std::max(static_cast<int>(wp), static_cast<int>(lp));
			st->s.pos = std::clamp(st->s.pos, st->s.min, MaxThumb(st->s));
			SyncControl(hwnd, *st);
			return old;
		}
		return 0;
	case SBM_GETRANGE:
		if(st != nullptr)
		{
			if(wp)
				*reinterpret_cast<LPINT>(wp) = st->s.min;
			if(lp)
				*reinterpret_cast<LPINT>(lp) = st->s.max;
		}
		return 0;
	case SBM_ENABLE_ARROWS:
		if(st != nullptr)
		{
			st->s.enabled = wp != ESB_DISABLE_BOTH;
			SyncControl(hwnd, *st);
		}
		return TRUE;
	default:
		return ::DefWindowProc(hwnd, msg, wp, lp);
	}
}

void Register(LPCTSTR name, WNDPROC proc, UINT style = CS_DBLCLKS | CS_PARENTDC)
{
	WNDCLASS wc{};
	wc.style = style;
	wc.lpfnWndProc = proc;
	wc.hCursor = ::LoadCursor(nullptr, IDC_ARROW);
	wc.lpszClassName = name;
	::RegisterClass(&wc);
}

} // namespace

void RegisterEditClass();
void RegisterListBoxClasses();
void RegisterComboBoxClass();
void RegisterCommonControlClasses();
void RegisterTabControlClass();
void RegisterHeaderClass();
void RegisterListViewClass();
void RegisterTreeViewClass();
void RegisterToolTipClass();
void RegisterToolbarClass();

void RegisterSystemClasses()
{
	Register(L"Button", ButtonProc);
	Register(L"Static", StaticProc);
	Register(L"ScrollBar", ScrollBarProc);
	RegisterEditClass();
	RegisterListBoxClasses();
	RegisterComboBoxClass();
	RegisterCommonControlClasses();
	RegisterTabControlClass();
	RegisterHeaderClass();
	RegisterListViewClass();
	RegisterTreeViewClass();
	RegisterToolTipClass();
	RegisterToolbarClass();
}

} // namespace linopenmpt::qtwin
