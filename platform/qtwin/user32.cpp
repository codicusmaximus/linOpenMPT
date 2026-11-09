// linOpenMPT platform layer: user32 windows and messages on Qt.
// See QtWin.h for the model.

#include "QtWin.h"

#include <QtCore/QAbstractEventDispatcher>
#include <QtCore/QCoreApplication>
#include <QtCore/QEventLoop>
#include <QtCore/QDateTime>
#include <QtCore/QDeadlineTimer>
#include <QtCore/QElapsedTimer>
#include <QtGui/QCloseEvent>
#include <QtGui/QContextMenuEvent>
#include <QtGui/QCursor>
#include <QtGui/QGuiApplication>
#include <QtGui/QIcon>
#include <QtGui/QKeyEvent>
#include <QtGui/QMouseEvent>
#include <QtGui/QResizeEvent>
#include <QtGui/QScreen>
#include <QtGui/QWheelEvent>
#include <QtWidgets/QApplication>

#include <algorithm>
#include <cwctype>
#include <set>

namespace linopenmpt::qtwin
{

namespace
{
std::map<std::wstring, std::unique_ptr<WindowClass>> &Classes()
{
	static std::map<std::wstring, std::unique_ptr<WindowClass>> classes;
	return classes;
}
std::set<Win32Widget *> &Windows()
{
	static std::set<Win32Widget *> windows;
	return windows;
}

std::wstring Lower(std::wstring s)
{
	for(auto &c : s)
		c = static_cast<wchar_t>(std::towlower(c));
	return s;
}

// Class names compare case-insensitively, as on Windows. Atoms (from
// MAKEINTATOM) are not used by OpenMPT.
const WindowClass *FindClass(LPCTSTR name)
{
	if(name == nullptr)
		return nullptr;
	auto it = Classes().find(Lower(name));
	if(it == Classes().end())
	{
		// The system classes exist from the start on Windows.
		static bool systemClasses = false;
		if(!systemClasses)
		{
			systemClasses = true;
			RegisterSystemClasses();
			it = Classes().find(Lower(name));
		}
	}
	return it != Classes().end() ? it->second.get() : nullptr;
}

PreTranslateHook g_preTranslate = nullptr;
// Whether the key down being delivered went through TranslateMessage().
bool g_keyTranslated = false;

// Posted messages, oldest first.
std::deque<MSG> g_queue;
bool g_quitPosted = false;
int g_quitCode = 0;
LPARAM g_messageExtraInfo = 0;

// Delivers posted messages from the Qt event loop.
class PostedMessageEvent : public QEvent
{
public:
	static QEvent::Type EventType()
	{
		static const auto type = static_cast<QEvent::Type>(QEvent::registerEventType());
		return type;
	}
	PostedMessageEvent() : QEvent(EventType()) { }
};

class Poster : public QObject
{
public:
	bool event(QEvent *e) override
	{
		if(e->type() != PostedMessageEvent::EventType())
			return QObject::event(e);
		// Only the messages present now: handlers may post more, and those
		// get their own event.
		std::size_t count = g_queue.size();
		while(count-- > 0 && !g_queue.empty())
		{
			MSG msg = g_queue.front();
			g_queue.pop_front();
			PumpInputMessage(msg);
		}
		return true;
	}
};

Poster &ThePoster()
{
	static Poster poster;
	return poster;
}

POINT CursorPos()
{
	const QPoint p = QCursor::pos();
	return POINT{p.x(), p.y()};
}

MSG MakeMsg(HWND hwnd, UINT message, WPARAM wp, LPARAM lp)
{
	MSG msg{};
	msg.hwnd = hwnd;
	msg.message = message;
	msg.wParam = wp;
	msg.lParam = lp;
	msg.time = static_cast<DWORD>(QDateTime::currentMSecsSinceEpoch());
	msg.pt = CursorPos();
	return msg;
}

LPARAM PointParam(const QPoint &p)
{
	return MAKELPARAM(static_cast<WORD>(static_cast<short>(p.x())), static_cast<WORD>(static_cast<short>(p.y())));
}

HWND g_capture = nullptr;
HWND g_focus = nullptr; // the last window that had the focus
bool g_hasFocus = false; // and whether it still has it

// Window hooks, most recently installed first, as Windows calls them.
struct Hook
{
	int id;
	HOOKPROC proc;
};
std::vector<Hook *> &Hooks()
{
	static std::vector<Hook *> hooks;
	return hooks;
}

LRESULT CallHookChain(int id, std::size_t from, int code, WPARAM wp, LPARAM lp)
{
	auto &hooks = Hooks();
	for(std::size_t i = from; i < hooks.size(); i++)
	{
		if(hooks[i]->id == id)
			return hooks[i]->proc(code, wp, lp);
	}
	return 0;
}

LRESULT CallHooks(int id, int code, WPARAM wp, LPARAM lp)
{
	return CallHookChain(id, 0, code, wp, lp);
}

} // namespace

Win32Widget *FromHandle(HWND hwnd)
{
	auto *w = reinterpret_cast<Win32Widget *>(hwnd);
	return (w != nullptr && Windows().count(w)) ? w : nullptr;
}

WindowData *Data(HWND hwnd)
{
	Win32Widget *w = FromHandle(hwnd);
	return w ? &w->data : nullptr;
}

void SetPreTranslateHook(PreTranslateHook hook)
{
	g_preTranslate = hook;
}

void PumpInputMessage(const MSG &msg)
{
	MSG m = msg;
	if(m.message == WM_QUIT)
	{
		QCoreApplication::exit(static_cast<int>(m.wParam));
		return;
	}
	if(g_preTranslate && g_preTranslate(&m))
		return;
	if(HandleDialogMessage(m))
		return;
	::TranslateMessage(&m);
	::DispatchMessage(&m);
}

QString ToQString(LPCWSTR text, int count)
{
	if(text == nullptr)
		return QString();
	if(count < 0)
		count = static_cast<int>(std::wcslen(text));
	return QString::fromWCharArray(text, count);
}

std::wstring ToWString(const QString &text)
{
	return text.toStdWString();
}

// ---- Win32Widget -----------------------------------------------------------

Win32Widget::Win32Widget(QWidget *parent, Qt::WindowFlags flags)
	: QWidget(parent, flags)
{
	Windows().insert(this);
	setMouseTracking(true);
	setFocusPolicy(Qt::ClickFocus);
	// Upstream paints everything itself in WM_PAINT/WM_ERASEBKGND.
	setAttribute(Qt::WA_OpaquePaintEvent);
}

Win32Widget::~Win32Widget()
{
	ReleaseWindowDCs(this);
	Windows().erase(this);
	if(g_capture == hwnd())
		g_capture = nullptr;
}

bool Win32Widget::event(QEvent *e)
{
	if(e->type() == QEvent::ShortcutOverride)
	{
		// Keys go to the window first, as on Windows, where accelerators
		// are translated by the message loop (TranslateAccelerator).
		e->accept();
		return true;
	}
	if(e->type() == QEvent::Enter && data.trackLeave)
	{
		// TrackMouseEvent() requests are one-shot.
	}
	return QWidget::event(e);
}

void Win32Widget::paintEvent(QPaintEvent *e)
{
	EnsureSurface(this);
	if(data.ncDirty)
	{
		data.ncDirty = false;
		if(!data.nonClient.isNull())
			::SendMessage(hwnd(), WM_NCPAINT, 1, 0);
	}
	if(!invalid.isEmpty())
	{
		::SendMessage(hwnd(), WM_PAINT, 0, 0);
		// A window procedure that never calls BeginPaint would get WM_PAINT
		// forever on Windows; here it simply gets it once.
		invalid = QRegion();
		erasePending = false;
	}
	QPainter painter(this);
	painter.setCompositionMode(data.transparent ? QPainter::CompositionMode_SourceOver : QPainter::CompositionMode_Source);
	const QRect r = e->rect();
	const qreal dpr = surface.devicePixelRatio();
	painter.drawImage(r, surface, QRectF(r.x() * dpr, r.y() * dpr, r.width() * dpr, r.height() * dpr));
}

namespace
{
// Mouse messages for the non-client area (borders, the scroll bar corner):
// WM_NCHITTEST says what is under the mouse. Returns false for the client
// area, or when the mouse is captured (captured input is client input).
bool NonClientMouse(Win32Widget *w, QMouseEvent *e, UINT clientMessage)
{
	if(g_capture == w->hwnd() || ClientRect(w).contains(e->position().toPoint()))
		return false;
	const QPoint g = e->globalPosition().toPoint();
	const LRESULT hit = ::SendMessage(w->hwnd(), WM_NCHITTEST, 0, PointParam(g));
	if(hit == HTCLIENT)
		return false;
	UINT message = 0;
	switch(clientMessage)
	{
	case WM_MOUSEMOVE:
		::SendMessage(w->hwnd(), WM_SETCURSOR, reinterpret_cast<WPARAM>(w->hwnd()), MAKELPARAM(hit, WM_MOUSEMOVE));
		message = WM_NCMOUSEMOVE;
		break;
	case WM_LBUTTONDOWN: message = WM_NCLBUTTONDOWN; break;
	case WM_LBUTTONUP: message = WM_NCLBUTTONUP; break;
	case WM_LBUTTONDBLCLK: message = WM_NCLBUTTONDBLCLK; break;
	case WM_RBUTTONDOWN: message = WM_NCRBUTTONDOWN; break;
	case WM_RBUTTONUP: message = WM_NCRBUTTONUP; break;
	case WM_RBUTTONDBLCLK: message = WM_NCRBUTTONDBLCLK; break;
	case WM_MBUTTONDOWN: message = WM_NCMBUTTONDOWN; break;
	case WM_MBUTTONUP: message = WM_NCMBUTTONUP; break;
	case WM_MBUTTONDBLCLK: message = WM_NCMBUTTONDBLCLK; break;
	default: return true;
	}
	PumpInputMessage(MakeMsg(w->hwnd(), message, static_cast<WPARAM>(hit), PointParam(g)));
	return true;
}

LPARAM ClientPointParam(const Win32Widget *w, const QMouseEvent *e)
{
	return PointParam(e->position().toPoint() - ClientOrigin(w));
}
} // namespace

void Win32Widget::mousePressEvent(QMouseEvent *e)
{
	UINT message = 0;
	switch(e->button())
	{
	case Qt::LeftButton: message = WM_LBUTTONDOWN; SetKeyState(VK_LBUTTON, true); break;
	case Qt::RightButton: message = WM_RBUTTONDOWN; SetKeyState(VK_RBUTTON, true); break;
	case Qt::MiddleButton: message = WM_MBUTTONDOWN; SetKeyState(VK_MBUTTON, true); break;
	default: QWidget::mousePressEvent(e); return;
	}
	// WM_MOUSEACTIVATE first: the window (and its parents) can refuse the
	// activation, or eat the click.
	const HWND self = hwnd();
	if(g_capture == nullptr)
	{
		const QPoint g = e->globalPosition().toPoint();
		const LRESULT hit = ClientRect(this).contains(e->position().toPoint()) ? HTCLIENT : ::SendMessage(self, WM_NCHITTEST, 0, PointParam(g));
		const LRESULT activate = ::SendMessage(self, WM_MOUSEACTIVATE, reinterpret_cast<WPARAM>(::GetAncestor(self, GA_ROOT)), MAKELPARAM(hit, message));
		if(FromHandle(self) != this || activate == MA_ACTIVATEANDEAT || activate == MA_NOACTIVATEANDEAT)
			return;
	}
	if(focusPolicy() != Qt::NoFocus && !hasFocus())
		setFocus(Qt::MouseFocusReason);
	if(NonClientMouse(this, e, message))
		return;
	PumpInputMessage(MakeMsg(hwnd(), message, MouseKeyFlags(e->buttons(), e->modifiers()), ClientPointParam(this, e)));
}

void Win32Widget::mouseReleaseEvent(QMouseEvent *e)
{
	UINT message = 0;
	switch(e->button())
	{
	case Qt::LeftButton: message = WM_LBUTTONUP; SetKeyState(VK_LBUTTON, false); break;
	case Qt::RightButton: message = WM_RBUTTONUP; SetKeyState(VK_RBUTTON, false); break;
	case Qt::MiddleButton: message = WM_MBUTTONUP; SetKeyState(VK_MBUTTON, false); break;
	default: QWidget::mouseReleaseEvent(e); return;
	}
	if(NonClientTrackingEvent(this, e))
		return;
	if(NonClientMouse(this, e, message))
		return;
	PumpInputMessage(MakeMsg(hwnd(), message, MouseKeyFlags(e->buttons(), e->modifiers()), ClientPointParam(this, e)));
	// Windows sends WM_CONTEXTMENU after the right button is released.
	if(e->button() == Qt::RightButton)
	{
		const QPoint g = e->globalPosition().toPoint();
		::SendMessage(hwnd(), WM_CONTEXTMENU, reinterpret_cast<WPARAM>(hwnd()), PointParam(g));
	}
}

void Win32Widget::mouseDoubleClickEvent(QMouseEvent *e)
{
	// Without CS_DBLCLKS, Windows reports the second click as a normal one.
	const bool dblClks = data.windowClass && (data.windowClass->wc.style & CS_DBLCLKS);
	UINT message = 0;
	switch(e->button())
	{
	case Qt::LeftButton: message = dblClks ? WM_LBUTTONDBLCLK : WM_LBUTTONDOWN; break;
	case Qt::RightButton: message = dblClks ? WM_RBUTTONDBLCLK : WM_RBUTTONDOWN; break;
	case Qt::MiddleButton: message = dblClks ? WM_MBUTTONDBLCLK : WM_MBUTTONDOWN; break;
	default: QWidget::mouseDoubleClickEvent(e); return;
	}
	if(NonClientMouse(this, e, message))
		return;
	PumpInputMessage(MakeMsg(hwnd(), message, MouseKeyFlags(e->buttons(), e->modifiers()), ClientPointParam(this, e)));
}

void Win32Widget::mouseMoveEvent(QMouseEvent *e)
{
	if(NonClientTrackingEvent(this, e))
		return;
	if(NonClientMouse(this, e, WM_MOUSEMOVE))
		return;
	// The window (or its parents) picks the cursor; not while captured.
	if(g_capture == nullptr)
		::SendMessage(hwnd(), WM_SETCURSOR, reinterpret_cast<WPARAM>(hwnd()), MAKELPARAM(HTCLIENT, WM_MOUSEMOVE));
	PumpInputMessage(MakeMsg(hwnd(), WM_MOUSEMOVE, MouseKeyFlags(e->buttons(), e->modifiers()), ClientPointParam(this, e)));
}

void Win32Widget::wheelEvent(QWheelEvent *e)
{
	// One notch is 120 on both systems.
	const QPoint delta = e->angleDelta();
	const QPoint g = e->globalPosition().toPoint();
	const WORD keys = static_cast<WORD>(MouseKeyFlags(e->buttons(), e->modifiers()));
	if(delta.y() != 0)
		PumpInputMessage(MakeMsg(hwnd(), WM_MOUSEWHEEL, MAKEWPARAM(keys, static_cast<WORD>(static_cast<short>(delta.y()))), PointParam(g)));
	if(delta.x() != 0)
		PumpInputMessage(MakeMsg(hwnd(), WM_MOUSEHWHEEL, MAKEWPARAM(keys, static_cast<WORD>(static_cast<short>(-delta.x()))), PointParam(g)));
	e->accept();
}

namespace
{
// lParam of keyboard messages: repeat count, scan code, extended flag,
// context (Alt), previous state and transition.
LPARAM KeyParam(const QKeyEvent *e, const KeyInfo &key, bool up)
{
	LPARAM lp = 1;
	lp |= static_cast<LPARAM>(key.scanCode & 0xFF) << 16;
	if(key.extended)
		lp |= LPARAM(1) << 24;
	if(e->modifiers() & Qt::AltModifier)
		lp |= LPARAM(1) << 29;
	if(up || e->isAutoRepeat())
		lp |= LPARAM(1) << 30;
	if(up)
		lp |= LPARAM(1) << 31;
	return lp;
}
} // namespace

void Win32Widget::keyPressEvent(QKeyEvent *e)
{
	SyncModifierState(e->modifiers());
	// An edit's Qt widget ignored a key the edit gave back to it: done.
	if(g_reinjecting)
	{
		e->accept();
		return;
	}
	const KeyInfo key = KeyFromQt(e);
	const UINT vk = key.vk;
	const bool alt = (e->modifiers() & Qt::AltModifier) != 0;
	const HWND self = hwnd();
	if(vk != 0)
	{
		SetKeyState(vk, true, key.scanCode, key.extended);
		g_keyTranslated = false;
		PumpInputMessage(MakeMsg(self, alt || vk == VK_F10 ? WM_SYSKEYDOWN : WM_KEYDOWN, vk, KeyParam(e, key, false)));
		// Keys that an accelerator or the dialog manager consumed are never
		// passed to TranslateMessage(), so they produce no WM_CHAR.
		if(!g_keyTranslated || FromHandle(self) == nullptr || data.destroying)
		{
			e->accept();
			return;
		}
	}
	// Qt has already translated the key to text; send it as WM_CHAR, which
	// is what TranslateMessage() would post.
	const QString text = e->text();
	if(!text.isEmpty() && !(e->modifiers() & Qt::ControlModifier && text.at(0).isLetter()))
	{
		for(const QChar c : text)
		{
			if(c.unicode() >= 0x20 || c.unicode() == '\r' || c.unicode() == '\b' || c.unicode() == '\t' || c.unicode() == 0x1B)
				PumpInputMessage(MakeMsg(hwnd(), alt ? WM_SYSCHAR : WM_CHAR, c.unicode(), KeyParam(e, key, false)));
		}
	} else if(e->modifiers() & Qt::ControlModifier && vk >= 'A' && vk <= 'Z')
	{
		// Ctrl+letter produces a control character on Windows.
		PumpInputMessage(MakeMsg(hwnd(), WM_CHAR, vk - 'A' + 1, KeyParam(e, key, false)));
	}
	e->accept();
}

void Win32Widget::keyReleaseEvent(QKeyEvent *e)
{
	SyncModifierState(e->modifiers());
	// An edit's Qt widget ignored a key the edit gave back to it: done.
	if(g_reinjecting)
	{
		e->accept();
		return;
	}
	if(e->isAutoRepeat())
		return;
	const KeyInfo key = KeyFromQt(e);
	const UINT vk = key.vk;
	if(vk == 0)
		return;
	SetKeyState(vk, false, key.scanCode, key.extended);
	const bool alt = (e->modifiers() & Qt::AltModifier) != 0 || vk == VK_MENU;
	PumpInputMessage(MakeMsg(hwnd(), alt || vk == VK_F10 ? WM_SYSKEYUP : WM_KEYUP, vk, KeyParam(e, key, true)));
	e->accept();
}

void Win32Widget::focusInEvent(QFocusEvent *e)
{
	// A popup (a combo box's list) takes Qt's keyboard focus, but on Windows
	// the focus stays where it was.
	if(e->reason() != Qt::PopupFocusReason)
		HandleFocusIn();
}

void Win32Widget::focusOutEvent(QFocusEvent *e)
{
	if(e->reason() != Qt::PopupFocusReason)
		HandleFocusOut();
}

void Win32Widget::HandleFocusIn()
{
	// Qt has already moved the focus, so a CBT hook can watch but not veto.
	const HWND self = hwnd();
	if(g_focus == self && g_hasFocus)
		return;
	const HWND old = (g_focus != self && FromHandle(g_focus)) ? g_focus : nullptr;
	g_focus = self;
	g_hasFocus = true;
	CallHooks(WH_CBT, HCBT_SETFOCUS, reinterpret_cast<WPARAM>(self), reinterpret_cast<LPARAM>(old));
	::SendMessage(self, WM_SETFOCUS, reinterpret_cast<WPARAM>(old), 0);
	if(FromHandle(self))
		DialogFocusChanged(self);
}

void Win32Widget::HandleFocusOut()
{
	if(g_focus != hwnd() || !g_hasFocus)
		return;
	QWidget *next = QApplication::focusWidget();
	const HWND nextWin = WindowOf(next);
	if(next == nullptr)
		ResetKeyState(); // keys released elsewhere never reach us
	if(nextWin == hwnd())
		return;
	g_hasFocus = false;
	::SendMessage(hwnd(), WM_KILLFOCUS, reinterpret_cast<WPARAM>(nextWin), 0);
}

void SetTransparent(Win32Widget *w)
{
	if(w->data.transparent)
		return;
	w->data.transparent = true;
	w->setAttribute(Qt::WA_OpaquePaintEvent, false);
	w->surfacePainter.reset();
	w->surface = QImage();
	EnsureSurface(w);
	w->update();
}

HWND WindowOf(QWidget *widget)
{
	// Controls built on Qt widgets (edits) hold the focus in a child widget.
	for(QWidget *w = widget; w != nullptr; w = w->parentWidget())
	{
		if(auto *win = dynamic_cast<Win32Widget *>(w); win && FromHandle(win->hwnd()))
			return win->hwnd();
	}
	return nullptr;
}

void Win32Widget::resizeEvent(QResizeEvent *)
{
	WPARAM type = SIZE_RESTORED;
	if(isWindow())
	{
		if(isMinimized())
			type = SIZE_MINIMIZED;
		else if(isMaximized())
			type = SIZE_MAXIMIZED;
	} else
	{
		type = data.childShow;
	}
	EnsureSurface(this);
	UpdateNonClient(this);
	if(!data.nonClient.isNull())
		data.ncDirty = true;
	const QSize oldSize = data.clientSize;
	const QSize client = ClientRect(this).size();
	// Windows invalidates the whole window for CS_HREDRAW / CS_VREDRAW
	// classes, and only the newly exposed parts otherwise.
	const UINT classStyle = data.windowClass ? data.windowClass->wc.style : 0;
	const bool redrawAll = oldSize.isEmpty()
		|| ((classStyle & CS_HREDRAW) && oldSize.width() != client.width())
		|| ((classStyle & CS_VREDRAW) && oldSize.height() != client.height());
	const QRegion clientArea(QRect(QPoint(0, 0), client));
	const QRegion exposed = redrawAll ? clientArea : clientArea.subtracted(QRegion(QRect(QPoint(0, 0), oldSize)));
	if(!exposed.isEmpty())
	{
		invalid += exposed;
		erasePending = true;
	}
	data.clientSize = client;
	ClipToParentClient(this);
	::SendMessage(hwnd(), WM_SIZE, type, MAKELPARAM(client.width(), client.height()));
}

void Win32Widget::moveEvent(QMoveEvent *)
{
	ClipToParentClient(this);
	// The client area's position: on the screen, or in the parent's client area.
	const QPoint p = isWindow() ? mapToGlobal(ClientOrigin(this)) : pos() + ClientOrigin(this) - ClientOrigin(parentWidget());
	::SendMessage(hwnd(), WM_MOVE, 0, PointParam(p));
}

void Win32Widget::closeEvent(QCloseEvent *e)
{
	// The window procedure decides; DefWindowProc destroys the window.
	e->ignore();
	if(!data.destroying)
		::SendMessage(hwnd(), WM_SYSCOMMAND, SC_CLOSE, 0);
}

void Win32Widget::leaveEvent(QEvent *)
{
	if(data.trackLeave)
	{
		data.trackLeave = false;
		::PostMessage(hwnd(), WM_MOUSELEAVE, 0, 0);
	}
}

void Win32Widget::contextMenuEvent(QContextMenuEvent *e)
{
	// Mouse context menus are sent on right button release; this is the
	// keyboard (menu key) case, with the "no position" convention.
	if(e->reason() == QContextMenuEvent::Keyboard)
		::SendMessage(hwnd(), WM_CONTEXTMENU, reinterpret_cast<WPARAM>(hwnd()), static_cast<LPARAM>(-1));
	e->accept();
}

// Top-level windows: the procedure's WM_GETMINMAXINFO limits become the
// widget's minimum and maximum sizes, which the window manager enforces while
// the user resizes.
void ApplyMinMaxInfo(Win32Widget *w)
{
	if(!w->isWindow())
		return;
	const QRect screen = w->screen() ? w->screen()->availableGeometry() : QRect(0, 0, 1920, 1080);
	MINMAXINFO mmi{};
	mmi.ptMaxSize = {screen.width(), screen.height()};
	mmi.ptMaxPosition = {0, 0};
	const POINT defMin = {::GetSystemMetrics(SM_CXMIN), ::GetSystemMetrics(SM_CYMIN)};
	const POINT defMax = {::GetSystemMetrics(SM_CXVIRTUALSCREEN), ::GetSystemMetrics(SM_CYVIRTUALSCREEN)};
	mmi.ptMinTrackSize = defMin;
	mmi.ptMaxTrackSize = defMax;
	::SendMessage(w->hwnd(), WM_GETMINMAXINFO, 0, reinterpret_cast<LPARAM>(&mmi));
	w = FromHandle(w->hwnd());
	if(w == nullptr)
		return;
	// The sizes are window sizes, with the frame (as in MoveWindow).
	const QRect frame = w->frameGeometry(), inner = w->geometry();
	const int fw = frame.width() - inner.width(), fh = frame.height() - inner.height();
	const QSize minSize(std::max(0, static_cast<int>(mmi.ptMinTrackSize.x) - fw), std::max(0, static_cast<int>(mmi.ptMinTrackSize.y) - fh));
	QSize maxSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
	if(mmi.ptMaxTrackSize.x != defMax.x)
		maxSize.setWidth(std::max(minSize.width(), static_cast<int>(mmi.ptMaxTrackSize.x) - fw));
	if(mmi.ptMaxTrackSize.y != defMax.y)
		maxSize.setHeight(std::max(minSize.height(), static_cast<int>(mmi.ptMaxTrackSize.y) - fh));
	if(w->minimumSize() != minSize)
		w->setMinimumSize(minSize);
	if(w->maximumSize() != maxSize)
		w->setMaximumSize(maxSize);
}

void Win32Widget::showEvent(QShowEvent *)
{
	data.style |= WS_VISIBLE;
	ApplyMinMaxInfo(this);
	::SendMessage(hwnd(), WM_SHOWWINDOW, TRUE, 0);
}

void Win32Widget::hideEvent(QHideEvent *)
{
	data.style &= ~WS_VISIBLE;
	::SendMessage(hwnd(), WM_SHOWWINDOW, FALSE, 0);
}

void SetChildShowState(Win32Widget *w, UINT sizeType, const QRect &rect)
{
	const HWND hwnd = w->hwnd();
	if(w->data.childShow == SIZE_RESTORED && sizeType != SIZE_RESTORED)
		w->data.restoreRect = ChildWindowRect(w);
	const UINT old = w->data.childShow;
	w->data.childShow = sizeType;
	const QRect r = sizeType == SIZE_RESTORED ? (old == SIZE_RESTORED ? ChildWindowRect(w) : w->data.restoreRect) : rect;
	const QSize before = w->size();
	::SetWindowPos(hwnd, nullptr, r.left(), r.top(), r.width(), r.height(), SWP_NOZORDER | SWP_NOACTIVATE);
	if(FromHandle(hwnd) != w)
		return;
	// The caption buttons change; WM_SIZE says the new state even when the
	// size stays.
	w->data.ncDirty = true;
	w->update();
	if(w->size() == before && old != sizeType)
	{
		const QSize client = ClientRect(w).size();
		::SendMessage(hwnd, WM_SIZE, sizeType, MAKELPARAM(client.width(), client.height()));
	}
}

} // namespace linopenmpt::qtwin

using namespace linopenmpt::qtwin;

// ---- classes ---------------------------------------------------------------

ATOM RegisterClass(const WNDCLASS *wc)
{
	if(wc == nullptr || wc->lpszClassName == nullptr)
		return 0;
	const std::wstring key = Lower(wc->lpszClassName);
	auto &classes = Classes();
	if(classes.count(key))
		return 0;  // ERROR_CLASS_ALREADY_EXISTS, as on Windows
	auto cls = std::make_unique<WindowClass>();
	cls->name = wc->lpszClassName;
	cls->wc = *wc;
	cls->wc.lpszClassName = cls->name.c_str();
	classes[key] = std::move(cls);
	return static_cast<ATOM>(classes.size());
}

BOOL UnregisterClass(LPCTSTR className, HINSTANCE)
{
	return className && Classes().erase(Lower(className)) ? TRUE : FALSE;
}

// ---- creation and destruction ------------------------------------------------

HWND CreateWindowEx(DWORD exStyle, LPCTSTR className, LPCTSTR windowName, DWORD style, int x, int y, int w, int h, HWND parent, HMENU menu, HINSTANCE instance, LPVOID param)
{
	const WindowClass *cls = FindClass(className);
	if(cls == nullptr)
		return nullptr;
	Win32Widget *parentWidget = FromHandle(parent);
	const bool child = (style & WS_CHILD) != 0;
	if(child && parentWidget == nullptr)
		return nullptr;

	Qt::WindowFlags flags;
	if(!child)
	{
		// Owned top-level windows (dialogs, tool windows) stay above their owner.
		flags = (style & WS_POPUP) ? Qt::Window | Qt::FramelessWindowHint : Qt::Window;
		if(style & WS_CAPTION)
			flags = Qt::Window;
		if(exStyle & WS_EX_TOOLWINDOW)
			flags |= Qt::Tool;
		if(exStyle & WS_EX_TOPMOST)
			flags |= Qt::WindowStaysOnTopHint;
		// Tooltips never take the focus or show in the task bar.
		if(Lower(cls->name) == Lower(TOOLTIPS_CLASS))
			flags = Qt::ToolTip | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint;
	}
	auto *widget = new Win32Widget(parentWidget, flags);
	WindowData &d = widget->data;
	d.widget = widget;
	d.windowClass = cls;
	d.proc = cls->wc.lpfnWndProc;
	d.style = style & ~WS_VISIBLE;
	d.exStyle = exStyle;
	d.extra.assign((cls->wc.cbWndExtra + sizeof(LONG_PTR) - 1) / sizeof(LONG_PTR), 0);
	if(child)
		d.id = reinterpret_cast<UINT_PTR>(menu);
	else
		d.menu = menu;
	if(windowName)
		d.text = windowName;
	if(!child)
		widget->setWindowTitle(ToQString(d.text.c_str()));
	if(style & WS_DISABLED)
		widget->setEnabled(false);
	else if(!child && parentWidget != nullptr && !parentWidget->isEnabled())
		widget->setEnabled(true); // owned by a disabled window, as modal dialogs are
	if(style & WS_TABSTOP)
		widget->setFocusPolicy(Qt::StrongFocus);

	if(child && (exStyle & WS_EX_MDICHILD))
		MdiDefaultRect(parent, x, y, w, h);
	if(x == CW_USEDEFAULT)
		x = y = 0;
	if(w == CW_USEDEFAULT)
	{
		w = 640;
		h = 480;
	}
	const QPoint parentOrigin = child ? ClientOrigin(parentWidget) : QPoint();
	widget->setGeometry(x + parentOrigin.x(), y + parentOrigin.y(), std::max(w, 0), std::max(h, 0));
	// Children without WS_VISIBLE stay hidden when their parent shows, and
	// new children go to the bottom of the Z order, as on Windows.
	if(child)
	{
		widget->hide();
		widget->lower();
	}
	InitScrollBars(widget);

	CREATESTRUCT cs{};
	cs.lpCreateParams = param;
	cs.hInstance = instance;
	cs.hMenu = menu;
	cs.hwndParent = parent;
	cs.cx = w;
	cs.cy = h;
	cs.x = x;
	cs.y = y;
	cs.style = static_cast<LONG>(style);
	cs.lpszName = windowName;
	cs.lpszClass = className;
	cs.dwExStyle = exStyle;
	const HWND hwnd = widget->hwnd();
	// MFC attaches its CWnd and subclasses the window here.
	CBT_CREATEWND cbt{&cs, HWND_TOP};
	if(CallHooks(WH_CBT, HCBT_CREATEWND, reinterpret_cast<WPARAM>(hwnd), reinterpret_cast<LPARAM>(&cbt)) != 0)
	{
		Windows().erase(widget);
		delete widget;
		return nullptr;
	}
	if(!::SendMessage(hwnd, WM_NCCREATE, 0, reinterpret_cast<LPARAM>(&cs)) || !FromHandle(hwnd))
	{
		if(FromHandle(hwnd))
			::DestroyWindow(hwnd);
		return nullptr;
	}
	if(!child && menu != nullptr)
		AttachMenuBar(widget, menu);
	UpdateNonClient(widget);
	if(::SendMessage(hwnd, WM_CREATE, 0, reinterpret_cast<LPARAM>(&cs)) == -1 || !FromHandle(hwnd))
	{
		if(FromHandle(hwnd))
			::DestroyWindow(hwnd);
		return nullptr;
	}
	{
		const QRect client = ClientRect(widget);
		widget->data.clientSize = client.size();
		// A new window is invalid all over, as on Windows.
		widget->invalid = QRegion(QRect(QPoint(0, 0), client.size()));
		widget->erasePending = true;
		::SendMessage(hwnd, WM_SIZE, SIZE_RESTORED, MAKELPARAM(client.width(), client.height()));
	}
	{
		const QPoint p = widget->pos() + ClientOrigin(widget) - parentOrigin;
		::SendMessage(hwnd, WM_MOVE, 0, PointParam(p));
	}
	if(child && parentWidget && !(exStyle & WS_EX_NOPARENTNOTIFY))
		::SendMessage(parent, WM_PARENTNOTIFY, MAKEWPARAM(WM_CREATE, d.id), reinterpret_cast<LPARAM>(hwnd));
	if(style & WS_VISIBLE)
		widget->show();
	if(child && (exStyle & WS_EX_MDICHILD) && FromHandle(hwnd))
		MdiChildCreated(hwnd);
	return hwnd;
}

namespace
{
void SendDestroy(Win32Widget *w)
{
	w->data.destroying = true;
	w->data.timers.clear();
	::SendMessage(w->hwnd(), WM_DESTROY, 0, 0);
	// Children get WM_DESTROY after their parent, as on Windows.
	for(QObject *o : w->children())
	{
		if(auto *c = dynamic_cast<Win32Widget *>(o); c && FromHandle(c->hwnd()) && !c->data.destroying)
			SendDestroy(c);
	}
}
void SendNcDestroy(Win32Widget *w)
{
	for(QObject *o : w->children())
	{
		if(auto *c = dynamic_cast<Win32Widget *>(o); c && FromHandle(c->hwnd()))
			SendNcDestroy(c);
	}
	::SendMessage(w->hwnd(), WM_NCDESTROY, 0, 0);
	Windows().erase(w);
	g_queue.erase(std::remove_if(g_queue.begin(), g_queue.end(), [w](const MSG &m) { return m.hwnd == w->hwnd(); }), g_queue.end());
}
} // namespace

BOOL DestroyWindow(HWND hwnd)
{
	Win32Widget *w = FromHandle(hwnd);
	if(w == nullptr)
		return FALSE;
	if(w->data.destroying)
		return TRUE;
	CallHooks(WH_CBT, HCBT_DESTROYWND, reinterpret_cast<WPARAM>(hwnd), 0);
	if(w->data.exStyle & WS_EX_MDICHILD)
		MdiChildDestroying(hwnd);
	w->hide();
	SendDestroy(w);
	SendNcDestroy(w);
	// The handle is invalid from here on; the widget goes when Qt is done
	// with any event it is delivering to it.
	w->deleteLater();
	return TRUE;
}

BOOL IsWindow(HWND hwnd)
{
	return FromHandle(hwnd) != nullptr;
}

// ---- window procedures and messages --------------------------------------------

LRESULT SendMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
	WindowData *d = Data(hwnd);
	if(d == nullptr || d->proc == nullptr)
		return 0;
	return d->proc(hwnd, msg, wp, lp);
}

LRESULT SendMessageW(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
	return ::SendMessage(hwnd, msg, wp, lp);
}

BOOL SendNotifyMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
	::SendMessage(hwnd, msg, wp, lp);
	return TRUE;
}

LRESULT CallWindowProc(WNDPROC proc, HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
	return proc ? proc(hwnd, msg, wp, lp) : 0;
}

BOOL PostMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
	if(hwnd != nullptr && FromHandle(hwnd) == nullptr)
		return FALSE;
	g_queue.push_back(MakeMsg(hwnd, msg, wp, lp));
	QCoreApplication::postEvent(&ThePoster(), new PostedMessageEvent());
	return TRUE;
}

BOOL PostThreadMessage(DWORD, UINT msg, WPARAM wp, LPARAM lp)
{
	return ::PostMessage(nullptr, msg, wp, lp);
}

void PostQuitMessage(int exitCode)
{
	g_quitPosted = true;
	g_quitCode = exitCode;
	::PostMessage(nullptr, WM_QUIT, static_cast<WPARAM>(exitCode), 0);
}

UINT RegisterWindowMessage(LPCTSTR name)
{
	// Registered messages are in 0xC000..0xFFFF on Windows.
	static std::map<std::wstring, UINT> messages;
	const std::wstring key = Lower(name ? name : L"");
	auto it = messages.find(key);
	if(it != messages.end())
		return it->second;
	const UINT id = 0xC000 + static_cast<UINT>(messages.size());
	messages[key] = id;
	return id;
}

LRESULT DispatchMessage(const MSG *msg)
{
	if(msg == nullptr || msg->hwnd == nullptr)
		return 0;
	if(msg->message == WM_TIMER && msg->lParam != 0)
	{
		reinterpret_cast<TIMERPROC>(msg->lParam)(msg->hwnd, WM_TIMER, msg->wParam, msg->time);
		return 0;
	}
	return ::SendMessage(msg->hwnd, msg->message, msg->wParam, msg->lParam);
}

BOOL TranslateMessage(const MSG *msg)
{
	// WM_CHAR is generated from Qt's own key translation (see keyPressEvent),
	// for key presses that reach this.
	if(msg != nullptr && (msg->message == WM_KEYDOWN || msg->message == WM_SYSKEYDOWN))
	{
		g_keyTranslated = true;
		return TRUE;
	}
	return FALSE;
}

namespace
{
bool Matches(const MSG &m, HWND hwnd, UINT filterMin, UINT filterMax)
{
	if(hwnd != nullptr && m.hwnd != hwnd && !IsChild(hwnd, m.hwnd))
		return false;
	if(filterMin == 0 && filterMax == 0)
		return true;
	return m.message >= filterMin && m.message <= filterMax;
}
} // namespace

BOOL PeekMessage(LPMSG msg, HWND hwnd, UINT filterMin, UINT filterMax, UINT remove)
{
	// Let Qt deliver pending input and paint first, as a Win32 PeekMessage
	// would process sent messages. Posted messages are left for us.
	QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
	QCoreApplication::processEvents(QEventLoop::AllEvents | QEventLoop::X11ExcludeTimers);
	for(auto it = g_queue.begin(); it != g_queue.end(); ++it)
	{
		if(!Matches(*it, hwnd, filterMin, filterMax))
			continue;
		if(msg)
			*msg = *it;
		if(remove & PM_REMOVE)
			g_queue.erase(it);
		return TRUE;
	}
	return FALSE;
}

BOOL GetMessage(LPMSG msg, HWND hwnd, UINT filterMin, UINT filterMax)
{
	for(;;)
	{
		for(auto it = g_queue.begin(); it != g_queue.end(); ++it)
		{
			if(!Matches(*it, hwnd, filterMin, filterMax))
				continue;
			MSG m = *it;
			g_queue.erase(it);
			if(msg)
				*msg = m;
			return m.message == WM_QUIT ? FALSE : TRUE;
		}
		QCoreApplication::processEvents(QEventLoop::AllEvents | QEventLoop::WaitForMoreEvents);
	}
}

LPARAM GetMessageExtraInfo()
{
	return g_messageExtraInfo;
}

LPARAM SetMessageExtraInfo(LPARAM info)
{
	const LPARAM old = g_messageExtraInfo;
	g_messageExtraInfo = info;
	return old;
}

// ---- DefWindowProc ---------------------------------------------------------------

namespace
{
// Alt pressed and released with no other key: SC_KEYMENU, as on Windows.
bool g_altAlone = false;
} // namespace

LRESULT DefWindowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
	Win32Widget *w = FromHandle(hwnd);
	if(w == nullptr)
		return 0;
	WindowData &d = w->data;
	switch(msg)
	{
	case WM_NCCREATE:
		return TRUE;
	case WM_CLOSE:
		::DestroyWindow(hwnd);
		return 0;
	case WM_SYSCOMMAND:
		if((wp & 0xFFF0) == SC_CLOSE)
			::SendMessage(hwnd, WM_CLOSE, 0, 0);
		else if((wp & 0xFFF0) == SC_MINIMIZE)
			::ShowWindow(hwnd, SW_MINIMIZE);
		else if((wp & 0xFFF0) == SC_MAXIMIZE)
			::ShowWindow(hwnd, SW_MAXIMIZE);
		else if((wp & 0xFFF0) == SC_RESTORE)
			::ShowWindow(hwnd, SW_RESTORE);
		else if((wp & 0xFFF0) == SC_KEYMENU)
		{
			// Alt+letter opens that menu, F10 or Alt alone the menu bar.
			if(!OpenMenuByKey(w, static_cast<WCHAR>(lp)) && lp != 0)
				::MessageBeep(0);
		}
		return 0;
	case WM_SYSKEYDOWN:
		g_altAlone = wp == VK_MENU && !(lp & 0x40000000);
		if(wp == VK_F4 && (lp & 0x20000000))
			::PostMessage(::GetAncestor(hwnd, GA_ROOT), WM_SYSCOMMAND, SC_CLOSE, 0);
		return 0;
	case WM_SYSKEYUP:
	case WM_KEYUP:
		if((wp == VK_MENU && g_altAlone) || (msg == WM_SYSKEYUP && wp == VK_F10))
			::SendMessage(::GetAncestor(hwnd, GA_ROOT), WM_SYSCOMMAND, SC_KEYMENU, 0);
		g_altAlone = false;
		return 0;
	case WM_KEYDOWN:
		g_altAlone = false;
		return 0;
	case WM_SYSCHAR:
		g_altAlone = false;
		if(wp != VK_RETURN && wp != VK_ESCAPE && (lp & 0x20000000))
			::SendMessage(::GetAncestor(hwnd, GA_ROOT), WM_SYSCOMMAND, SC_KEYMENU, wp);
		return 0;
	case WM_PAINT:
		{
			PAINTSTRUCT ps;
			::BeginPaint(hwnd, &ps);
			::EndPaint(hwnd, &ps);
		}
		return 0;
	case WM_ERASEBKGND:
		if(d.windowClass && d.windowClass->wc.hbrBackground)
		{
			RECT rc;
			::GetClientRect(hwnd, &rc);
			HBRUSH brush = d.windowClass->wc.hbrBackground;
			// (HBRUSH)(COLOR_xxx + 1) means a system colour.
			if(reinterpret_cast<UINT_PTR>(brush) <= 31)
				brush = ::GetSysColorBrush(static_cast<int>(reinterpret_cast<UINT_PTR>(brush) - 1));
			::FillRect(reinterpret_cast<HDC>(wp), &rc, brush);
			return 1;
		}
		return 0;
	case WM_SETTEXT:
		d.text = lp ? reinterpret_cast<LPCWSTR>(lp) : L"";
		if(w->isWindow())
			w->setWindowTitle(ToQString(d.text.c_str()));
		return TRUE;
	case WM_GETTEXT:
		{
			auto *buf = reinterpret_cast<LPWSTR>(lp);
			if(buf == nullptr || wp == 0)
				return 0;
			const std::size_t n = std::min<std::size_t>(d.text.size(), wp - 1);
			std::copy_n(d.text.c_str(), n, buf);
			buf[n] = 0;
			return static_cast<LRESULT>(n);
		}
	case WM_GETTEXTLENGTH:
		return static_cast<LRESULT>(d.text.size());
	case WM_SETFONT:
		d.font = reinterpret_cast<HFONT>(wp);
		if(LOWORD(lp))
			w->update();
		return 0;
	case WM_GETFONT:
		return reinterpret_cast<LRESULT>(d.font);
	case WM_MOUSEWHEEL:
	case WM_MOUSEHWHEEL:
	case WM_CONTEXTMENU:
		// Unhandled wheel and context menu messages go to the parent.
		if((d.style & WS_CHILD) && w->parentWidget())
			if(auto *p = dynamic_cast<Win32Widget *>(w->parentWidget()))
				return ::SendMessage(p->hwnd(), msg, wp, lp);
		return 0;
	case WM_NCHITTEST:
		return DefNcHitTest(w, QPoint(GET_X_LPARAM(lp), GET_Y_LPARAM(lp)));
	case WM_NCLBUTTONDOWN:
		// Child windows move and size by their frames here; top-level
		// windows belong to the window manager.
		if((d.style & WS_CHILD) && w->parentWidget())
		{
			const bool sizing = wp >= HTLEFT && wp <= HTBOTTOMRIGHT && (d.style & WS_THICKFRAME) && d.childShow == SIZE_RESTORED;
			const bool moving = wp == HTCAPTION && d.childShow != SIZE_MAXIMIZED;
			const bool button = wp == HTCLOSE || wp == HTMAXBUTTON || wp == HTMINBUTTON;
			if(sizing || moving || button)
				BeginNonClientTracking(w, static_cast<LRESULT>(wp), QPoint(GET_X_LPARAM(lp), GET_Y_LPARAM(lp)));
		}
		return 0;
	case WM_NCLBUTTONDBLCLK:
		if(wp == HTCAPTION && (d.style & WS_CHILD) && (d.style & WS_MAXIMIZEBOX))
			::SendMessage(hwnd, WM_SYSCOMMAND, d.childShow == SIZE_RESTORED ? SC_MAXIMIZE : SC_RESTORE, lp);
		return 0;
	case WM_CTLCOLORMSGBOX:
	case WM_CTLCOLOREDIT:
	case WM_CTLCOLORLISTBOX:
	case WM_CTLCOLORBTN:
	case WM_CTLCOLORDLG:
	case WM_CTLCOLORSCROLLBAR:
	case WM_CTLCOLORSTATIC:
		return DefCtlColor(reinterpret_cast<HDC>(wp), msg);
	case WM_NCCALCSIZE:
		DefNcCalcSize(w, reinterpret_cast<RECT *>(lp));
		return 0;
	case WM_NCPAINT:
		DefNcPaint(w);
		return 0;
	case WM_NCACTIVATE:
		return TRUE;
	case WM_MOUSEACTIVATE:
		// Child windows ask their parent first.
		if((d.style & WS_CHILD) && w->parentWidget())
		{
			if(auto *p = dynamic_cast<Win32Widget *>(w->parentWidget()))
			{
				if(const LRESULT r = ::SendMessage(p->hwnd(), WM_MOUSEACTIVATE, wp, lp))
					return r;
			}
		}
		return MA_ACTIVATE;
	case WM_SETCURSOR:
		// Parents get the first say, then the class cursor applies.
		if((d.style & WS_CHILD) && w->parentWidget())
		{
			if(auto *p = dynamic_cast<Win32Widget *>(w->parentWidget()))
			{
				if(::SendMessage(p->hwnd(), WM_SETCURSOR, wp, lp))
					return TRUE;
			}
		}
		if(LOWORD(lp) == HTCLIENT && reinterpret_cast<HWND>(wp) == hwnd)
		{
			HCURSOR cursor = d.windowClass ? d.windowClass->wc.hCursor : nullptr;
			::SetCursor(cursor ? cursor : SystemCursor(Qt::ArrowCursor));
		}
		return FALSE;
	case WM_SETREDRAW:
		d.redraw = wp != FALSE;
		w->setUpdatesEnabled(d.redraw);
		return 0;
	case WM_SETICON:
		{
			const auto *icon = reinterpret_cast<const IconData *>(lp);
			if(w->isWindow() && icon && !icon->image.isNull())
				w->setWindowIcon(QIcon(QPixmap::fromImage(icon->image)));
		}
		return 0;
	case WM_GETDLGCODE:
		return 0;
	case WM_SHOWWINDOW:
		return 0;
	default:
		return 0;
	}
}

// ---- hierarchy --------------------------------------------------------------------

HWND GetParent(HWND hwnd)
{
	Win32Widget *w = FromHandle(hwnd);
	if(w == nullptr || w->parentWidget() == nullptr)
		return nullptr;
	auto *p = dynamic_cast<Win32Widget *>(w->parentWidget());
	return p ? p->hwnd() : nullptr;
}

HWND SetParent(HWND child, HWND parent)
{
	Win32Widget *w = FromHandle(child);
	if(w == nullptr)
		return nullptr;
	const HWND old = GetParent(child);
	Win32Widget *p = FromHandle(parent);
	const bool visible = w->isVisible();
	// The window keeps its position in client coordinates.
	const QPoint clientPos = w->pos() - (w->isWindow() ? QPoint() : ClientOrigin(w->parentWidget()));
	w->setParent(p, p ? w->windowFlags() & ~Qt::Window : w->windowFlags() | Qt::Window);
	w->move(clientPos + (p ? ClientOrigin(p) : QPoint()));
	if(visible)
		w->show();
	return old;
}

BOOL IsChild(HWND parent, HWND hwnd)
{
	Win32Widget *p = FromHandle(parent);
	Win32Widget *w = FromHandle(hwnd);
	// Only WS_CHILD windows are children; owned windows are not.
	if(p == nullptr || w == nullptr || w == p || w->isWindow())
		return FALSE;
	for(QWidget *x = w->parentWidget(); x != nullptr; x = x->parentWidget())
	{
		if(x == p)
			return TRUE;
		if(x->isWindow())
			break;
	}
	return FALSE;
}

HWND GetAncestor(HWND hwnd, UINT flags)
{
	Win32Widget *w = FromHandle(hwnd);
	if(w == nullptr)
		return nullptr;
	if(flags == GA_PARENT)
		return GetParent(hwnd);
	QWidget *top = w->window();
	if(flags == GA_ROOTOWNER)
	{
		while(top->parentWidget())
			top = top->parentWidget()->window();
	}
	auto *t = dynamic_cast<Win32Widget *>(top);
	return t ? t->hwnd() : hwnd;
}

namespace
{
// Children in Z order, topmost first. As on Windows, new children go to the
// bottom, so this is also creation order (and the dialog tab order) unless
// SetWindowPos reorders them. Qt lists children bottom first.
std::vector<Win32Widget *> ChildWindows(QWidget *parent)
{
	std::vector<Win32Widget *> result;
	const QObjectList &children = parent->children();
	for(auto it = children.rbegin(); it != children.rend(); ++it)
	{
		if(auto *c = dynamic_cast<Win32Widget *>(*it); c && FromHandle(c->hwnd()) && !c->isWindow())
			result.push_back(c);
	}
	return result;
}
} // namespace

HWND GetWindow(HWND hwnd, UINT cmd)
{
	Win32Widget *w = FromHandle(hwnd);
	if(w == nullptr)
		return nullptr;
	if(cmd == GW_CHILD)
	{
		auto children = ChildWindows(w);
		return children.empty() ? nullptr : children.front()->hwnd();
	}
	if(cmd == GW_OWNER)
		return w->isWindow() ? GetParent(hwnd) : nullptr;
	QWidget *parent = w->parentWidget();
	if(parent == nullptr || w->isWindow())
		return nullptr;
	auto siblings = ChildWindows(parent);
	auto it = std::find(siblings.begin(), siblings.end(), w);
	switch(cmd)
	{
	case GW_HWNDFIRST: return siblings.front()->hwnd();
	case GW_HWNDLAST: return siblings.back()->hwnd();
	case GW_HWNDNEXT: return (it != siblings.end() && it + 1 != siblings.end()) ? (*(it + 1))->hwnd() : nullptr;
	case GW_HWNDPREV: return (it != siblings.end() && it != siblings.begin()) ? (*(it - 1))->hwnd() : nullptr;
	default: return nullptr;
	}
}

HWND GetTopWindow(HWND hwnd)
{
	return hwnd ? GetWindow(hwnd, GW_CHILD) : nullptr;
}

BOOL EnumChildWindows(HWND parent, WNDENUMPROC proc, LPARAM lParam)
{
	Win32Widget *p = FromHandle(parent);
	if(p == nullptr || proc == nullptr)
		return FALSE;
	// All descendants, depth first, as on Windows.
	std::vector<HWND> all;
	std::function<void(QWidget *)> collect = [&](QWidget *x) {
		for(Win32Widget *c : ChildWindows(x))
		{
			all.push_back(c->hwnd());
			collect(c);
		}
	};
	collect(p);
	for(HWND h : all)
	{
		if(FromHandle(h) && !proc(h, lParam))
			break;
	}
	return TRUE;
}

BOOL EnumWindows(WNDENUMPROC proc, LPARAM lParam)
{
	std::vector<HWND> tops;
	for(Win32Widget *w : Windows())
	{
		if(w->isWindow())
			tops.push_back(w->hwnd());
	}
	for(HWND h : tops)
	{
		if(FromHandle(h) && !proc(h, lParam))
			break;
	}
	return TRUE;
}

HWND GetDesktopWindow()
{
	return nullptr;
}

DWORD GetWindowThreadProcessId(HWND, LPDWORD processID)
{
	if(processID)
		*processID = ::GetCurrentProcessId();
	return ::GetCurrentThreadId();
}

int GetClassName(HWND hwnd, LPTSTR className, int max)
{
	WindowData *d = Data(hwnd);
	if(d == nullptr || d->windowClass == nullptr || className == nullptr || max <= 0)
		return 0;
	const std::wstring &name = d->windowClass->name;
	const int n = std::min<int>(static_cast<int>(name.size()), max - 1);
	std::copy_n(name.c_str(), n, className);
	className[n] = 0;
	return n;
}

// ---- dialog items -------------------------------------------------------------------

HWND GetDlgItem(HWND dlg, int id)
{
	Win32Widget *p = FromHandle(dlg);
	if(p == nullptr)
		return nullptr;
	for(Win32Widget *c : ChildWindows(p))
	{
		if(static_cast<int>(c->data.id) == id)
			return c->hwnd();
	}
	return nullptr;
}

int GetDlgCtrlID(HWND hwnd)
{
	WindowData *d = Data(hwnd);
	return d ? static_cast<int>(d->id) : 0;
}

UINT GetDlgItemText(HWND dlg, int id, LPTSTR text, int max)
{
	return static_cast<UINT>(::GetWindowText(GetDlgItem(dlg, id), text, max));
}

BOOL SetDlgItemText(HWND dlg, int id, LPCTSTR text)
{
	return ::SetWindowText(GetDlgItem(dlg, id), text);
}

LRESULT SendDlgItemMessage(HWND dlg, int id, UINT msg, WPARAM wp, LPARAM lp)
{
	return ::SendMessage(GetDlgItem(dlg, id), msg, wp, lp);
}

UINT IsDlgButtonChecked(HWND dlg, int id)
{
	return static_cast<UINT>(::SendMessage(GetDlgItem(dlg, id), BM_GETCHECK, 0, 0));
}

BOOL CheckDlgButton(HWND dlg, int id, UINT check)
{
	::SendMessage(GetDlgItem(dlg, id), BM_SETCHECK, check, 0);
	return TRUE;
}

// ---- text ---------------------------------------------------------------------------

int GetWindowText(HWND hwnd, LPTSTR text, int max)
{
	return static_cast<int>(::SendMessage(hwnd, WM_GETTEXT, static_cast<WPARAM>(max), reinterpret_cast<LPARAM>(text)));
}

int GetWindowTextLength(HWND hwnd)
{
	return static_cast<int>(::SendMessage(hwnd, WM_GETTEXTLENGTH, 0, 0));
}

int GetWindowTextA(HWND hwnd, LPSTR text, int max)
{
	if(text == nullptr || max <= 0)
		return 0;
	std::wstring w(static_cast<std::size_t>(GetWindowTextLength(hwnd)) + 1, L'\0');
	GetWindowText(hwnd, w.data(), static_cast<int>(w.size()));
	const QByteArray local = QString::fromWCharArray(w.c_str()).toLocal8Bit();
	const int n = std::min<int>(static_cast<int>(local.size()), max - 1);
	std::copy_n(local.constData(), n, text);
	text[n] = 0;
	return n;
}

int GetWindowTextLengthA(HWND hwnd)
{
	std::wstring w(static_cast<std::size_t>(GetWindowTextLength(hwnd)) + 1, L'\0');
	GetWindowText(hwnd, w.data(), static_cast<int>(w.size()));
	return static_cast<int>(QString::fromWCharArray(w.c_str()).toLocal8Bit().size());
}

BOOL SetWindowText(HWND hwnd, LPCTSTR text)
{
	if(!IsWindow(hwnd))
		return FALSE;
	return static_cast<BOOL>(::SendMessage(hwnd, WM_SETTEXT, 0, reinterpret_cast<LPARAM>(text)));
}

// ---- state ----------------------------------------------------------------------------

namespace linopenmpt::qtwin
{

void KeepOwnedWindowsEnabled(QWidget *w)
{
	// Qt disables a widget's child windows with it; Windows does not disable
	// the windows a window owns (a modal dialog disables its owner).
	for(QWidget *c : w->findChildren<QWidget *>())
	{
		if(!c->isWindow() || c->testAttribute(Qt::WA_ForceDisabled))
			continue;
		if(auto *cw = dynamic_cast<Win32Widget *>(c); cw != nullptr && (cw->data.style & WS_DISABLED))
			continue;
		c->setEnabled(true);
	}
}

} // namespace linopenmpt::qtwin

BOOL EnableWindow(HWND hwnd, BOOL enable)
{
	Win32Widget *w = FromHandle(hwnd);
	if(w == nullptr)
		return FALSE;
	const BOOL wasDisabled = !w->isEnabled();
	if(static_cast<bool>(enable) == w->isEnabled())
		return wasDisabled;
	if(enable)
		w->data.style &= ~WS_DISABLED;
	else
		w->data.style |= WS_DISABLED;
	w->setEnabled(enable != FALSE);
	if(!enable)
		KeepOwnedWindowsEnabled(w);
	::SendMessage(hwnd, WM_ENABLE, enable, 0);
	return wasDisabled;
}

BOOL IsWindowEnabled(HWND hwnd)
{
	Win32Widget *w = FromHandle(hwnd);
	return w && w->isEnabled();
}

BOOL ShowWindow(HWND hwnd, int cmd)
{
	Win32Widget *w = FromHandle(hwnd);
	if(w == nullptr)
		return FALSE;
	const BOOL wasVisible = !w->isHidden();
	if(!w->isWindow())
	{
		if((w->data.exStyle & WS_EX_MDICHILD) && MdiShowWindow(hwnd, cmd))
			return wasVisible;
		// Child windows maximize to their parent's client area.
		const RECT client = [&]() { RECT r{}; ::GetClientRect(::GetParent(hwnd), &r); return r; }();
		switch(cmd)
		{
		case SW_SHOWMINIMIZED:
		case SW_MINIMIZE:
		case SW_SHOWMINNOACTIVE:
			{
				const int cx = GetSystemMetrics(SM_CXMINIMIZED), cy = GetSystemMetrics(SM_CYMINIMIZED);
				SetChildShowState(w, SIZE_MINIMIZED, QRect(0, client.bottom - cy, cx, cy));
			}
			break;
		case SW_SHOWMAXIMIZED:
			SetChildShowState(w, SIZE_MAXIMIZED, QRect(0, 0, client.right, client.bottom));
			break;
		case SW_RESTORE:
			SetChildShowState(w, SIZE_RESTORED, QRect());
			break;
		default:
			break;
		}
		if(cmd == SW_HIDE)
			w->hide();
		else if(w->isHidden())
			w->show();
		return wasVisible;
	}
	switch(cmd)
	{
	case SW_HIDE: w->hide(); break;
	case SW_SHOWMINIMIZED:
	case SW_MINIMIZE:
	case SW_SHOWMINNOACTIVE: w->showMinimized(); break;
	case SW_SHOWMAXIMIZED: w->showMaximized(); break;
	case SW_RESTORE: w->showNormal(); break;
	case SW_SHOWNOACTIVATE:
	case SW_SHOWNA:
		w->setAttribute(Qt::WA_ShowWithoutActivating);
		w->show();
		w->setAttribute(Qt::WA_ShowWithoutActivating, false);
		break;
	default: w->show(); break;
	}
	return wasVisible;
}

BOOL IsWindowVisible(HWND hwnd)
{
	Win32Widget *w = FromHandle(hwnd);
	return w && w->isVisible();
}

BOOL IsIconic(HWND hwnd)
{
	Win32Widget *w = FromHandle(hwnd);
	return w && (w->isWindow() ? w->isMinimized() : w->data.childShow == SIZE_MINIMIZED);
}

BOOL IsZoomed(HWND hwnd)
{
	Win32Widget *w = FromHandle(hwnd);
	return w && (w->isWindow() ? w->isMaximized() : w->data.childShow == SIZE_MAXIMIZED);
}

LONG_PTR GetWindowLongPtr(HWND hwnd, int index)
{
	WindowData *d = Data(hwnd);
	if(d == nullptr)
		return 0;
	switch(index)
	{
	case GWLP_WNDPROC: return reinterpret_cast<LONG_PTR>(d->proc);
	case GWLP_USERDATA: return d->userData;
	case GWLP_ID: return static_cast<LONG_PTR>(d->id);
	case GWL_STYLE: return static_cast<LONG_PTR>(d->style | (!d->widget->isHidden() ? WS_VISIBLE : 0));
	case GWL_EXSTYLE: return static_cast<LONG_PTR>(d->exStyle);
	default:
		if(index >= 0 && static_cast<std::size_t>(index) / sizeof(LONG_PTR) < d->extra.size())
			return d->extra[static_cast<std::size_t>(index) / sizeof(LONG_PTR)];
		return 0;
	}
}

LONG_PTR SetWindowLongPtr(HWND hwnd, int index, LONG_PTR value)
{
	WindowData *d = Data(hwnd);
	if(d == nullptr)
		return 0;
	const LONG_PTR old = GetWindowLongPtr(hwnd, index);
	switch(index)
	{
	case GWLP_WNDPROC: d->proc = reinterpret_cast<WNDPROC>(value); break;
	case GWLP_USERDATA: d->userData = value; break;
	case GWLP_ID: d->id = static_cast<UINT_PTR>(value); break;
	case GWL_STYLE:
	{
		const DWORD oldStyle = d->style;
		d->style = static_cast<DWORD>(value) & ~WS_VISIBLE;
		ScrollStylesChanged(d->widget, oldStyle);
		d->widget->setEnabled(!(value & WS_DISABLED));
		if((value & WS_VISIBLE) != 0 && d->widget->isHidden())
			d->widget->show();
		else if((value & WS_VISIBLE) == 0 && !d->widget->isHidden())
			d->widget->hide();
		break;
	}
	case GWL_EXSTYLE: d->exStyle = static_cast<DWORD>(value); break;
	default:
		if(index >= 0 && static_cast<std::size_t>(index) / sizeof(LONG_PTR) < d->extra.size())
			d->extra[static_cast<std::size_t>(index) / sizeof(LONG_PTR)] = value;
		break;
	}
	return old;
}

LONG GetWindowLong(HWND hwnd, int index)
{
	return static_cast<LONG>(GetWindowLongPtr(hwnd, index));
}

LONG SetWindowLong(HWND hwnd, int index, LONG value)
{
	return static_cast<LONG>(SetWindowLongPtr(hwnd, index, value));
}

// ---- geometry -------------------------------------------------------------------------

BOOL GetClientRect(HWND hwnd, LPRECT rect)
{
	Win32Widget *w = FromHandle(hwnd);
	if(w == nullptr || rect == nullptr)
		return FALSE;
	const QRect r = ClientRect(w);
	*rect = RECT{0, 0, r.width(), r.height()};
	return TRUE;
}

BOOL GetWindowRect(HWND hwnd, LPRECT rect)
{
	Win32Widget *w = FromHandle(hwnd);
	if(w == nullptr || rect == nullptr)
		return FALSE;
	QRect r;
	if(w->isWindow())
		r = w->frameGeometry();
	else
		r = QRect(w->mapToGlobal(QPoint(0, 0)), w->size());
	*rect = RECT{r.left(), r.top(), r.left() + r.width(), r.top() + r.height()};
	return TRUE;
}

BOOL ScreenToClient(HWND hwnd, LPPOINT point)
{
	Win32Widget *w = FromHandle(hwnd);
	if(w == nullptr || point == nullptr)
		return FALSE;
	const QPoint p = w->mapFromGlobal(QPoint(point->x, point->y)) - ClientOrigin(w);
	*point = POINT{p.x(), p.y()};
	return TRUE;
}

BOOL ClientToScreen(HWND hwnd, LPPOINT point)
{
	Win32Widget *w = FromHandle(hwnd);
	if(w == nullptr || point == nullptr)
		return FALSE;
	const QPoint p = w->mapToGlobal(QPoint(point->x, point->y) + ClientOrigin(w));
	*point = POINT{p.x(), p.y()};
	return TRUE;
}

int MapWindowPoints(HWND from, HWND to, LPPOINT points, UINT count)
{
	if(points == nullptr)
		return 0;
	POINT origin{0, 0};
	if(from)
		ClientToScreen(from, &origin);
	POINT target{0, 0};
	if(to)
		ClientToScreen(to, &target);
	const LONG dx = origin.x - target.x, dy = origin.y - target.y;
	for(UINT i = 0; i < count; i++)
	{
		points[i].x += dx;
		points[i].y += dy;
	}
	return MAKELONG(static_cast<WORD>(static_cast<short>(dx)), static_cast<WORD>(static_cast<short>(dy)));
}

BOOL MoveWindow(HWND hwnd, int x, int y, int w, int h, BOOL repaint)
{
	Win32Widget *wd = FromHandle(hwnd);
	if(wd == nullptr)
		return FALSE;
	{
		// The window can adjust its new position and size (combo boxes keep
		// their height, for one).
		WINDOWPOS pos{hwnd, nullptr, x, y, w, h, static_cast<UINT>(SWP_NOZORDER | SWP_NOACTIVATE | (repaint ? 0 : SWP_NOREDRAW))};
		::SendMessage(hwnd, WM_WINDOWPOSCHANGING, 0, reinterpret_cast<LPARAM>(&pos));
		if(FromHandle(hwnd) == nullptr)
			return FALSE;
		x = pos.x;
		y = pos.y;
		w = pos.cx;
		h = pos.cy;
	}
	if(wd->isWindow())
	{
		// Window coordinates include the frame on Windows.
		const QRect frame = wd->frameGeometry(), inner = wd->geometry();
		const int fw = frame.width() - inner.width(), fh = frame.height() - inner.height();
		wd->move(x, y);
		wd->resize(std::max(w - fw, 0), std::max(h - fh, 0));
	} else
	{
		// Child windows are placed in the parent's client coordinates.
		const QPoint origin = ClientOrigin(wd->parentWidget());
		wd->setGeometry(x + origin.x(), y + origin.y(), std::max(w, 0), std::max(h, 0));
	}
	// Qt holds back move and resize events for hidden widgets until they
	// show; Windows sends WM_MOVE and WM_SIZE at once (dialogs lay out their
	// controls before they first show).
	if(FromHandle(hwnd) == wd && wd->testAttribute(Qt::WA_PendingMoveEvent))
	{
		wd->setAttribute(Qt::WA_PendingMoveEvent, false);
		QMoveEvent e(wd->pos(), wd->pos());
		QApplication::sendEvent(wd, &e);
	}
	if(FromHandle(hwnd) == wd && wd->testAttribute(Qt::WA_PendingResizeEvent))
	{
		wd->setAttribute(Qt::WA_PendingResizeEvent, false);
		QResizeEvent e(wd->size(), QSize());
		QApplication::sendEvent(wd, &e);
	}
	if(repaint && FromHandle(hwnd) == wd)
		wd->update();
	return TRUE;
}

BOOL SetWindowPos(HWND hwnd, HWND after, int x, int y, int cx, int cy, UINT flags)
{
	Win32Widget *w = FromHandle(hwnd);
	if(w == nullptr)
		return FALSE;
	if(!(flags & SWP_NOMOVE) || !(flags & SWP_NOSIZE))
	{
		RECT rc;
		GetWindowRect(hwnd, &rc);
		POINT pos{rc.left, rc.top};
		if(!w->isWindow())
		{
			const QPoint p = w->pos() - ClientOrigin(w->parentWidget());
			pos = POINT{p.x(), p.y()};
		}
		if(!(flags & SWP_NOMOVE))
			pos = POINT{x, y};
		int width = rc.right - rc.left, height = rc.bottom - rc.top;
		if(!(flags & SWP_NOSIZE))
		{
			width = cx;
			height = cy;
		}
		MoveWindow(hwnd, pos.x, pos.y, width, height, !(flags & SWP_NOREDRAW));
	}
	if(flags & SWP_FRAMECHANGED)
	{
		// The non-client area is recalculated (WM_NCCALCSIZE) and redrawn.
		w->data.ncDirty = true;
		if(UpdateNonClient(w))
		{
			const QSize client = ClientRect(w).size();
			w->data.clientSize = client;
			::InvalidateRect(hwnd, nullptr, TRUE);
			::SendMessage(hwnd, WM_SIZE, SIZE_RESTORED, MAKELPARAM(client.width(), client.height()));
		}
		w->update();
	}
	if(!(flags & SWP_NOZORDER))
	{
		if(after == HWND_TOP || after == HWND_TOPMOST)
			w->raise();
		else if(after == HWND_BOTTOM)
			w->lower();
		else if(Win32Widget *a = FromHandle(after))
			w->stackUnder(a);
	}
	if(flags & SWP_SHOWWINDOW)
		ShowWindow(hwnd, (flags & SWP_NOACTIVATE) ? SW_SHOWNA : SW_SHOW);
	if(flags & SWP_HIDEWINDOW)
		ShowWindow(hwnd, SW_HIDE);
	if(!(flags & SWP_NOACTIVATE) && w->isWindow() && w->isVisible())
		w->activateWindow();
	return TRUE;
}

namespace
{
struct DeferredPos
{
	HWND hwnd, after;
	int x, y, cx, cy;
	UINT flags;
};
} // namespace

HDWP BeginDeferWindowPos(int count)
{
	auto *list = new std::vector<DeferredPos>();
	list->reserve(static_cast<std::size_t>(std::max(count, 0)));
	return reinterpret_cast<HDWP>(list);
}

HDWP DeferWindowPos(HDWP info, HWND hwnd, HWND after, int x, int y, int cx, int cy, UINT flags)
{
	if(info == nullptr)
		return nullptr;
	reinterpret_cast<std::vector<DeferredPos> *>(info)->push_back({hwnd, after, x, y, cx, cy, flags});
	return info;
}

BOOL EndDeferWindowPos(HDWP info)
{
	if(info == nullptr)
		return FALSE;
	auto *list = reinterpret_cast<std::vector<DeferredPos> *>(info);
	for(const auto &p : *list)
		SetWindowPos(p.hwnd, p.after, p.x, p.y, p.cx, p.cy, p.flags);
	delete list;
	return TRUE;
}

BOOL GetWindowPlacement(HWND hwnd, WINDOWPLACEMENT *placement)
{
	Win32Widget *w = FromHandle(hwnd);
	if(w == nullptr || placement == nullptr)
		return FALSE;
	placement->flags = 0;
	placement->showCmd = ::IsIconic(hwnd) ? SW_SHOWMINIMIZED : ::IsZoomed(hwnd) ? SW_SHOWMAXIMIZED : SW_SHOWNORMAL;
	const QRect r = w->isWindow() ? w->normalGeometry() : w->data.childShow != SIZE_RESTORED ? w->data.restoreRect : ChildWindowRect(w);
	placement->rcNormalPosition = RECT{r.left(), r.top(), r.left() + r.width(), r.top() + r.height()};
	placement->ptMinPosition = POINT{-1, -1};
	placement->ptMaxPosition = POINT{-1, -1};
	return TRUE;
}

BOOL SetWindowPlacement(HWND hwnd, const WINDOWPLACEMENT *placement)
{
	Win32Widget *w = FromHandle(hwnd);
	if(w == nullptr || placement == nullptr)
		return FALSE;
	const RECT &r = placement->rcNormalPosition;
	const QPoint origin = w->isWindow() ? QPoint() : ClientOrigin(w->parentWidget());
	w->setGeometry(r.left + origin.x(), r.top + origin.y(), r.right - r.left, r.bottom - r.top);
	ShowWindow(hwnd, static_cast<int>(placement->showCmd));
	return TRUE;
}

HWND WindowFromPoint(POINT point)
{
	for(QWidget *w = QApplication::widgetAt(point.x, point.y); w != nullptr; w = w->parentWidget())
	{
		if(auto *ww = dynamic_cast<Win32Widget *>(w))
			return ww->hwnd();
	}
	return nullptr;
}

HWND ChildWindowFromPoint(HWND parent, POINT point)
{
	Win32Widget *p = FromHandle(parent);
	if(p == nullptr)
		return nullptr;
	// point is in the parent's client coordinates.
	const QPoint pt = QPoint(point.x, point.y) + ClientOrigin(p);
	if(!p->rect().contains(pt))
		return nullptr;
	for(Win32Widget *c : ChildWindows(p))
	{
		if(c->geometry().contains(pt))
			return c->hwnd();
	}
	return parent;
}

HWND FindWindow(LPCTSTR className, LPCTSTR windowName)
{
	for(Win32Widget *w : Windows())
	{
		if(!w->isWindow())
			continue;
		if(className && (w->data.windowClass == nullptr || Lower(w->data.windowClass->name) != Lower(className)))
			continue;
		if(windowName && w->data.text != windowName)
			continue;
		return w->hwnd();
	}
	return nullptr;
}

// ---- painting ------------------------------------------------------------------------

namespace
{
QRect ToQRect(const RECT &r)
{
	return QRect(r.left, r.top, r.right - r.left, r.bottom - r.top);
}
} // namespace

BOOL InvalidateRect(HWND hwnd, const RECT *rect, BOOL erase)
{
	Win32Widget *w = FromHandle(hwnd);
	if(w == nullptr)
		return FALSE;
	// The update region is in client coordinates.
	const QRect client(QPoint(0, 0), ClientRect(w).size());
	const QRect r = (rect ? ToQRect(*rect) : client) & client;
	if(r.isEmpty())
		return TRUE;
	w->invalid += r;
	if(erase)
		w->erasePending = true;
	w->update(r.translated(ClientOrigin(w)));
	return TRUE;
}

BOOL ValidateRect(HWND hwnd, const RECT *rect)
{
	Win32Widget *w = FromHandle(hwnd);
	if(w == nullptr)
		return FALSE;
	if(rect)
		w->invalid -= ToQRect(*rect);
	else
		w->invalid = QRegion();
	return TRUE;
}

BOOL GetUpdateRect(HWND hwnd, LPRECT rect, BOOL)
{
	Win32Widget *w = FromHandle(hwnd);
	if(w == nullptr || w->invalid.isEmpty())
	{
		if(rect)
			*rect = RECT{};
		return FALSE;
	}
	if(rect)
	{
		const QRect r = w->invalid.boundingRect();
		*rect = RECT{r.left(), r.top(), r.left() + r.width(), r.top() + r.height()};
	}
	return TRUE;
}

BOOL UpdateWindow(HWND hwnd)
{
	Win32Widget *w = FromHandle(hwnd);
	if(w == nullptr)
		return FALSE;
	// WM_PAINT goes to the window procedure now, as on Windows; the screen
	// catches up with the backing image on Qt's next paint.
	if(!w->invalid.isEmpty() && w->isVisible())
	{
		const QRect r = w->invalid.boundingRect();
		EnsureSurface(w);
		::SendMessage(hwnd, WM_PAINT, 0, 0);
		w->invalid = QRegion();
		w->erasePending = false;
		w->update(r.translated(ClientOrigin(w)));
	}
	return TRUE;
}

BOOL RedrawWindow(HWND hwnd, const RECT *update, HRGN, UINT flags)
{
	Win32Widget *w = FromHandle(hwnd);
	if(w == nullptr)
		return FALSE;
	if(flags & (RDW_INVALIDATE | RDW_INTERNALPAINT | RDW_ERASE))
	{
		InvalidateRect(hwnd, update, flags & RDW_ERASE);
		if(flags & RDW_ALLCHILDREN)
		{
			for(Win32Widget *c : ChildWindows(w))
				RedrawWindow(c->hwnd(), nullptr, nullptr, flags);
		}
	}
	if(flags & (RDW_UPDATENOW | RDW_ERASENOW))
	{
		UpdateWindow(hwnd);
		if(flags & RDW_ALLCHILDREN)
		{
			for(Win32Widget *c : ChildWindows(w))
				UpdateWindow(c->hwnd());
		}
	}
	return TRUE;
}

int ScrollWindowEx(HWND hwnd, int dx, int dy, const RECT *scroll, const RECT *clip, HRGN, LPRECT updateRect, UINT flags)
{
	Win32Widget *w = FromHandle(hwnd);
	if(w == nullptr)
		return 0; // ERROR
	const QPoint origin = ClientOrigin(w);
	if(flags & SW_SCROLLCHILDREN)
	{
		for(Win32Widget *c : ChildWindows(w))
		{
			if(scroll == nullptr || ToQRect(*scroll).intersects(c->geometry().translated(-origin)))
				c->move(c->x() + dx, c->y() + dy);
		}
	}
	const QRect client(QPoint(0, 0), ClientRect(w).size());
	QRect area = scroll ? ToQRect(*scroll) : client;
	if(clip)
		area &= ToQRect(*clip);
	area &= client;
	EnsureSurface(w);
	ScrollSurface(w, dx, dy, area.translated(origin));
	// What scrolled in (and pending invalid areas, which move along).
	const QRegion moved = QRegion(area.translated(dx, dy)) & area;
	QRegion uncovered = QRegion(area) - moved;
	QRegion pending = (w->invalid & area).translated(dx, dy) & area;
	w->invalid = (w->invalid - area) + pending;
	if(flags & SW_INVALIDATE)
	{
		w->invalid += uncovered;
		if(flags & SW_ERASE)
			w->erasePending = true;
	}
	w->update(area.translated(origin));
	if(updateRect)
	{
		const QRect r = uncovered.boundingRect();
		*updateRect = RECT{r.left(), r.top(), r.left() + r.width(), r.top() + r.height()};
	}
	return uncovered.isEmpty() ? NULLREGION : (uncovered.rectCount() == 1 ? SIMPLEREGION : COMPLEXREGION);
}

// ---- focus, activation, capture --------------------------------------------------------

HWND GetFocus()
{
	if(QApplication::activePopupWidget() != nullptr && WindowOf(QApplication::activePopupWidget()) != nullptr)
		return g_hasFocus && FromHandle(g_focus) ? g_focus : nullptr;
	return WindowOf(QApplication::focusWidget());
}

HWND SetFocus(HWND hwnd)
{
	const HWND old = GetFocus();
	if(Win32Widget *w = FromHandle(hwnd))
	{
		if(w->focusPolicy() == Qt::NoFocus)
			w->setFocusPolicy(Qt::ClickFocus);
		// The focus can only be in the active window: SetFocus activates
		// the window's top-level window, as on Windows.
		QWidget *top = w->window();
		if(top->isVisible() && !top->isActiveWindow() && QApplication::activePopupWidget() == nullptr)
			top->activateWindow();
		w->setFocus(Qt::OtherFocusReason);
	} else if(hwnd == nullptr)
	{
		if(QWidget *f = QApplication::focusWidget())
			f->clearFocus();
	}
	return old;
}

HWND GetActiveWindow()
{
	auto *w = dynamic_cast<Win32Widget *>(QApplication::activeWindow());
	return w ? w->hwnd() : nullptr;
}

HWND SetActiveWindow(HWND hwnd)
{
	const HWND old = GetActiveWindow();
	if(Win32Widget *w = FromHandle(hwnd))
	{
		w->window()->activateWindow();
		w->window()->raise();
	}
	return old;
}

HWND GetForegroundWindow()
{
	return GetActiveWindow();
}

BOOL SetForegroundWindow(HWND hwnd)
{
	SetActiveWindow(hwnd);
	return IsWindow(hwnd);
}

BOOL AllowSetForegroundWindow(DWORD)
{
	return TRUE;
}

HWND SetCapture(HWND hwnd)
{
	const HWND old = g_capture;
	if(Win32Widget *w = FromHandle(hwnd))
	{
		g_capture = hwnd;
		w->grabMouse();
	}
	return old;
}

HWND GetCapture()
{
	return FromHandle(g_capture) ? g_capture : nullptr;
}

BOOL ReleaseCapture()
{
	if(Win32Widget *w = FromHandle(g_capture))
	{
		w->releaseMouse();
		g_capture = nullptr;
		::SendMessage(w->hwnd(), WM_CAPTURECHANGED, 0, 0);
	}
	g_capture = nullptr;
	return TRUE;
}

BOOL TrackMouseEvent(LPTRACKMOUSEEVENT event)
{
	if(event == nullptr)
		return FALSE;
	WindowData *d = Data(event->hwndTrack);
	if(d == nullptr)
		return FALSE;
	if(event->dwFlags & TME_CANCEL)
		d->trackLeave = false;
	else if(event->dwFlags & TME_LEAVE)
		d->trackLeave = true;
	return TRUE;
}

BOOL GetCursorPos(LPPOINT point)
{
	if(point == nullptr)
		return FALSE;
	*point = CursorPos();
	return TRUE;
}

BOOL SetCursorPos(int x, int y)
{
	QCursor::setPos(x, y);
	return TRUE;
}

// ---- timers ------------------------------------------------------------------------------

UINT_PTR SetTimer(HWND hwnd, UINT_PTR id, UINT elapse, void *proc)
{
	WindowData *d = Data(hwnd);
	if(d == nullptr)
		return 0;  // Thread timers are not used by OpenMPT.
	auto &timer = d->timers[id];
	if(!timer)
		timer = std::make_unique<QTimer>();
	timer->disconnect();
	timer->setInterval(static_cast<int>(std::max<UINT>(elapse, 10)));
	// WM_TIMER is a low-priority posted message on Windows: never queue a
	// second one while one is pending.
	auto pending = std::make_shared<bool>(false);
	QObject::connect(timer.get(), &QTimer::timeout, [hwnd, id, proc, pending]() {
		if(*pending || !IsWindow(hwnd))
			return;
		*pending = true;
		MSG msg = MakeMsg(hwnd, WM_TIMER, id, reinterpret_cast<LPARAM>(proc));
		PumpInputMessage(msg);
		*pending = false;
	});
	timer->start();
	return id;
}

BOOL KillTimer(HWND hwnd, UINT_PTR id)
{
	WindowData *d = Data(hwnd);
	if(d == nullptr)
		return FALSE;
	return d->timers.erase(id) ? TRUE : FALSE;
}

BOOL FlashWindow(HWND hwnd, BOOL)
{
	if(Win32Widget *w = FromHandle(hwnd))
		QApplication::alert(w->window());
	return TRUE;
}

// ---- hooks --------------------------------------------------------------------------

HHOOK SetWindowsHookEx(int idHook, HOOKPROC proc, HINSTANCE, DWORD)
{
	// Only thread-local CBT hooks are delivered; others are accepted and
	// never called, as there are no system-wide hooks on Linux.
	if(proc == nullptr)
		return nullptr;
	auto *hook = new Hook{idHook, proc};
	Hooks().insert(Hooks().begin(), hook);
	return reinterpret_cast<HHOOK>(hook);
}

BOOL UnhookWindowsHookEx(HHOOK handle)
{
	auto &hooks = Hooks();
	auto it = std::find(hooks.begin(), hooks.end(), reinterpret_cast<Hook *>(handle));
	if(it == hooks.end())
		return FALSE;
	delete *it;
	hooks.erase(it);
	return TRUE;
}

LRESULT CallNextHookEx(HHOOK handle, int code, WPARAM wp, LPARAM lp)
{
	auto &hooks = Hooks();
	auto it = std::find(hooks.begin(), hooks.end(), reinterpret_cast<Hook *>(handle));
	if(it == hooks.end())
		return 0;
	return CallHookChain((*it)->id, static_cast<std::size_t>(it - hooks.begin()) + 1, code, wp, lp);
}

// ---- message loops (linOpenMPT extensions) ------------------------------------------

namespace
{
using IdleProc = BOOL (*)(LONG);
std::vector<IdleProc> g_idleStack;
struct ModalLoop
{
	QEventLoop *loop;
	const void *key;
};
std::vector<ModalLoop> g_modalLoops;
bool g_inIdle = false;

// Idle processing runs whenever Qt's event loop is about to wait, which is
// when a Win32 loop would find its queue empty.
void RunIdle()
{
	if(g_inIdle || g_idleStack.empty() || g_idleStack.back() == nullptr)
		return;
	g_inIdle = true;
	const IdleProc idle = g_idleStack.back();
	for(LONG count = 0; count < 16 && idle(count); count++)
	{
	}
	g_inIdle = false;
}

void ConnectIdle()
{
	static bool connected = false;
	if(connected)
		return;
	if(QAbstractEventDispatcher *dispatcher = QAbstractEventDispatcher::instance())
	{
		QObject::connect(dispatcher, &QAbstractEventDispatcher::aboutToBlock, dispatcher, &RunIdle);
		connected = true;
	}
}
} // namespace

void LinOpenMPTSetPreTranslateHook(BOOL (*hook)(MSG *msg))
{
	SetPreTranslateHook(hook);
}

int LinOpenMPTRunMessageLoop(BOOL (*idle)(LONG count))
{
	if(g_quitPosted)
		return g_quitCode;
	ConnectIdle();
	g_idleStack.push_back(idle);
	const int result = QApplication::exec();
	g_idleStack.pop_back();
	return result;
}

int LinOpenMPTRunModalLoopFor(const void *key, BOOL (*idle)(LONG count))
{
	ConnectIdle();
	QEventLoop loop;
	g_modalLoops.push_back({&loop, key});
	g_idleStack.push_back(idle);
	const int result = loop.exec(QEventLoop::DialogExec);
	g_idleStack.pop_back();
	g_modalLoops.erase(std::find_if(g_modalLoops.begin(), g_modalLoops.end(), [&loop](const ModalLoop &m) { return m.loop == &loop; }));
	return result;
}

BOOL LinOpenMPTEndModalLoopFor(const void *key, int result)
{
	// An outer loop ends once the loops inside it have, as on Windows.
	for(auto it = g_modalLoops.rbegin(); it != g_modalLoops.rend(); ++it)
	{
		if(it->key == key)
		{
			it->loop->exit(result);
			return TRUE;
		}
	}
	return FALSE;
}

namespace linopenmpt::qtwin
{

// Loops that are not ours (QMessageBox, QFileDialog) run no idle processing,
// as Windows' own modal loops run none of the application's.
ForeignModalLoop::ForeignModalLoop()
{
	ConnectIdle();
	g_idleStack.push_back(nullptr);
}

ForeignModalLoop::~ForeignModalLoop()
{
	g_idleStack.pop_back();
}

} // namespace linopenmpt::qtwin

int LinOpenMPTRunModalLoop(BOOL (*idle)(LONG count))
{
	return LinOpenMPTRunModalLoopFor(nullptr, idle);
}

void LinOpenMPTEndModalLoop(int result)
{
	if(!g_modalLoops.empty())
		g_modalLoops.back().loop->exit(result);
}
