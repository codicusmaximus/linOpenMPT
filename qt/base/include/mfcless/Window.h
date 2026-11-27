/*
 * Window.h
 * --------
 * Purpose: The window handlers of upstream's classes (OnLButtonDown,
 *          OnKeyDown, OnPaint, OnTimer, ...) on top of a Qt widget class, so
 *          that a twin keeps upstream's handlers as they are.
 * Notes  : mfcless::Window<QWidget> (or another widget class) turns the Qt
 *          events into calls of the handlers, with upstream's arguments. A
 *          handler that calls its base class's handler gets the Qt base
 *          class's behaviour (Default()), as an MFC handler that calls
 *          CWnd's gets DefWindowProc's.
 *          Key and mouse button messages pass through PreTranslateMessage of
 *          the receiver and its parents first, innermost first, as MFC's
 *          message loop does (InstallMessageFilter), and a window's commands and
 *          program messages reach its message map (mfcless::MessageMap).
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */

#pragma once

#include "mfcless/Geometry.h"
#include "mfcless/Keyboard.h"
#include "mfcless/MessageMap.h"

#include <QtCore/QBasicTimer>
#include <QtCore/QMimeData>
#include <QtCore/QStringList>
#include <QtCore/QTimerEvent>
#include <QtCore/QUrl>
#include <QtGui/QCloseEvent>
#include <QtGui/QContextMenuEvent>
#include <QtGui/QFocusEvent>
#include <QtGui/QKeyEvent>
#include <QtGui/QMouseEvent>
#include <QtGui/QPaintEvent>
#include <QtGui/QWheelEvent>
#include <QtWidgets/QAbstractScrollArea>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDialog>
#include <QtWidgets/QToolTip>
#include <QtWidgets/QWidget>

#include <map>
#include <type_traits>

namespace mfcless
{

// Sizes of OnSize.
namespace size
{
	inline constexpr uint32_t Restored = 0, Minimized = 1, Maximized = 2;
}

// What OnToolHitTest reports about the tool (area) under the mouse.
struct ToolInfo
{
	QWidget *hwnd = nullptr;
	uintptr_t uId = 0;
	Rect rect;       // the area, in client coordinates
	QString text;    // empty: ask GetToolTipText(uId)
	uint32_t uFlags = 0;
};

// What all windows have, whatever their Qt class.
struct DrawItemInfo;

class WindowBase : public MessageMap
{
public:
	// The tool (area with its own tooltip) at point, or -1.
	virtual intptr_t OnToolHitTest(Point /*point*/, ToolInfo * /*ti*/) const { return -1; }
	// The tooltip of a tool that OnToolHitTest gave no text.
	virtual QString GetToolTipText(uintptr_t /*id*/) { return {}; }
	// Called after DestroyWindow, once the window is gone (from the event
	// loop); windows that own themselves delete themselves here.
	virtual void PostNcDestroy() {}

	// Called for key and mouse messages to this window or its children
	// before they are delivered (for mouse messages, lParam holds the
	// position in the receiver's coordinates). True if the message was handled (and is not delivered).
	virtual bool PreTranslateMessage(Message * /*msg*/) { return false; }

	// The name a screen reader announces for the window's current state.
	virtual QString GetAccessibleName() { return {}; }
	// Windows that give screen readers their own names call this, as with
	// MFC; Qt asks every widget, so it has nothing to do.
	void EnableActiveAccessibility() {}
	// A zoom gesture (pinch) around center; delta in Windows' units.
	virtual bool OnGestureZoom(Point /*center*/, long /*delta*/) { return false; }

	// An owner-drawn child control (with this resource ID) asks to be drawn.
	virtual void OnDrawItem(int /*id*/, DrawItemInfo * /*info*/) {}

	// The widget.
	virtual QWidget *GetWidget() = 0;
};

// Installs the application-wide filter that calls PreTranslateMessage and
// keeps the key state (mfcless::KeyState). Once, at start.
void InstallMessageFilter();

template <typename Base>
class Window : public Base, public WindowBase
{
	static constexpr bool IsScrollArea = std::is_base_of_v<QAbstractScrollArea, Base>;
	// Qt classes that do nothing of their own with the mouse and keys, as
	// windows that upstream draws itself.
	static constexpr bool IsPlain = std::is_same_v<Base, QWidget> || std::is_same_v<Base, QAbstractScrollArea> || std::is_same_v<Base, QDialog>;

public:
	using Base::Base;

	QWidget *GetWidget() override { return this; }

	// The widget that upstream's client area is: a scroll area's viewport.
	QWidget *ClientWidget()
	{
		if constexpr(IsScrollArea)
			return this->viewport();
		else
			return this;
	}
	Rect GetClientRect()
	{
		return Rect(0, 0, ClientWidget()->width(), ClientWidget()->height());
	}

	// Timers by ID, as SetTimer / KillTimer.
	void SetTimer(uintptr_t id, int milliseconds)
	{
		m_timers[id].start(milliseconds, this);
	}
	void KillTimer(uintptr_t id)
	{
		if(auto it = m_timers.find(id); it != m_timers.end())
		{
			it->second.stop();
			m_timers.erase(it);
		}
	}

	// Mouse capture.
	void SetCapture() { ClientWidget()->grabMouse(); }
	void ReleaseCapture() { if(QWidget::mouseGrabber() == ClientWidget()) ClientWidget()->releaseMouse(); }
	bool HasCapture() const { return QWidget::mouseGrabber() == const_cast<Window *>(this)->ClientWidget(); }

	void DragAcceptFiles(bool accept = true) { this->setAcceptDrops(accept); }

	// Window functions as upstream calls them.
	void ShowWindow(bool show) { this->setVisible(show); }
	bool IsWindowVisible() const { return this->isVisible(); }
	void EnableWindow(bool enable = true) { this->setEnabled(enable); }
	bool IsWindowEnabled() const { return this->isEnabled(); }
	void Invalidate(bool /*erase*/ = true) { ClientWidget()->update(); }
	// Stops and restarts repainting during bulk updates.
	void SetRedraw(bool redraw = true)
	{
		this->setUpdatesEnabled(redraw);
		if(redraw)
			ClientWidget()->update();
	}
	void InvalidateRect(const Rect *r, bool /*erase*/ = true)
	{
		if(r)
			ClientWidget()->update(QRect(*r));
		else
			ClientWidget()->update();
	}
	void InvalidateRect(const Rect &r, bool erase = true) { InvalidateRect(&r, erase); }
	// Qt paints from the event loop; this paints what is due now.
	void UpdateWindow() { ClientWidget()->repaint(); }
	void GetClientRect(Rect *r) { *r = GetClientRect(); }
	// In screen coordinates.
	void GetWindowRect(Rect *r) const
	{
		const QWidget *w = this;
		*r = Rect(QRect(w->mapToGlobal(QPoint(0, 0)), w->size()));
	}
	void ClientToScreen(Point *p) { *p = ClientWidget()->mapToGlobal(QPoint(*p)); }
	void ClientToScreen(Rect *r) { r->OffsetRect(Point(ClientWidget()->mapToGlobal(QPoint(0, 0)))); }
	void ScreenToClient(Point *p) { *p = ClientWidget()->mapFromGlobal(QPoint(*p)); }
	void ScreenToClient(Rect *r) { const QPoint o = ClientWidget()->mapFromGlobal(QPoint(0, 0)); r->OffsetRect(o.x(), o.y()); }
	void MoveWindow(const Rect &r, bool /*repaint*/ = true) { this->setGeometry(QRect(r)); }
	void MoveWindow(int x, int y, int width, int height, bool /*repaint*/ = true) { this->setGeometry(x, y, width, height); }
	void SetWindowText(const QString &text) { this->setWindowTitle(text); }
	QString GetWindowText() const { return this->windowTitle(); }
	// Hides the window; PostNcDestroy follows from the event loop.
	bool DestroyWindow()
	{
		this->hide();
		QMetaObject::invokeMethod(this, [this] { PostNcDestroy(); }, Qt::QueuedConnection);
		return true;
	}

	// Handlers. Each one's default is the Qt base class's handling of the
	// event that led to it.
	virtual void OnLButtonDown(uint32_t, Point) { Default(); }
	virtual void OnLButtonUp(uint32_t, Point) { Default(); }
	virtual void OnLButtonDblClk(uint32_t, Point) { Default(); }
	virtual void OnRButtonDown(uint32_t, Point) { Default(); }
	virtual void OnRButtonUp(uint32_t, Point point)
	{
		// The context menu follows the release of the right button.
		Default();
		if(!m_contextSent)
		{
			m_contextSent = true;
			OnContextMenu(this, ClientWidget()->mapToGlobal(point));
		}
	}
	virtual void OnRButtonDblClk(uint32_t, Point) { Default(); }
	virtual void OnMButtonDown(uint32_t, Point) { Default(); }
	virtual void OnMButtonUp(uint32_t, Point) { Default(); }
	virtual void OnMButtonDblClk(uint32_t, Point) { Default(); }
	// button: 1 for the first X button (back), 2 for the second.
	virtual void OnXButtonDown(uint32_t, uint32_t, Point) { Default(); }
	virtual void OnXButtonUp(uint32_t, uint32_t, Point) { Default(); }
	virtual void OnXButtonDblClk(uint32_t, uint32_t, Point) { Default(); }
	virtual void OnMouseMove(uint32_t, Point) { Default(); }
	virtual void OnMouseLeave() { Default(); }
	// delta: WheelDelta per notch; point in screen coordinates.
	virtual bool OnMouseWheel(uint32_t, short, Point) { Default(); return false; }
	virtual void OnMouseHWheel(uint32_t, short, Point) { Default(); }
	// point in screen coordinates; (-1, -1) from the keyboard.
	virtual void OnContextMenu(QWidget *, Point) { DefaultContextMenu(); }

	virtual void OnKeyDown(uint32_t, uint32_t, uint32_t) { Default(); }
	virtual void OnKeyUp(uint32_t, uint32_t, uint32_t) { Default(); }
	virtual void OnSysKeyDown(uint32_t, uint32_t, uint32_t) { Default(); }
	virtual void OnSysKeyUp(uint32_t, uint32_t, uint32_t) { Default(); }
	virtual void OnChar(uint32_t, uint32_t, uint32_t) { Default(); }
	virtual void OnSysChar(uint32_t, uint32_t, uint32_t) { Default(); }

	virtual void OnSetFocus(QWidget *) { Default(); }
	virtual void OnKillFocus(QWidget *) { Default(); }
	virtual void OnPaint() { Default(); }
	virtual void OnSize(uint32_t, int, int) { Default(); }
	virtual void OnMove(int, int) { Default(); }
	virtual void OnShowWindow(bool, uint32_t) { Default(); }
	virtual void OnTimer(uintptr_t) { Default(); }
	virtual void OnDropFiles(const QStringList &) {}
	// The window is asked to close; it closes if this calls the base class's.
	virtual void OnClose() { Default(); }

protected:
	// The Qt base class's handling of the event being handled.
	void Default()
	{
		QEvent *e = m_event;
		if(!e || m_defaultDone)
			return;
		m_defaultDone = true;
		switch(e->type())
		{
		case QEvent::MouseButtonPress: Base::mousePressEvent(static_cast<QMouseEvent *>(e)); break;
		case QEvent::MouseButtonRelease: Base::mouseReleaseEvent(static_cast<QMouseEvent *>(e)); break;
		case QEvent::MouseButtonDblClick:
			// QWidget's handling of a double click is a second press; Windows
			// sends none.
			if constexpr(!IsPlain)
				Base::mouseDoubleClickEvent(static_cast<QMouseEvent *>(e));
			break;
		case QEvent::MouseMove: Base::mouseMoveEvent(static_cast<QMouseEvent *>(e)); break;
		case QEvent::Leave: Base::leaveEvent(e); break;
		case QEvent::Wheel: Base::wheelEvent(static_cast<QWheelEvent *>(e)); break;
		case QEvent::KeyPress: Base::keyPressEvent(static_cast<QKeyEvent *>(e)); break;
		case QEvent::KeyRelease: Base::keyReleaseEvent(static_cast<QKeyEvent *>(e)); break;
		case QEvent::FocusIn: Base::focusInEvent(static_cast<QFocusEvent *>(e)); break;
		case QEvent::FocusOut: Base::focusOutEvent(static_cast<QFocusEvent *>(e)); break;
		case QEvent::Paint: Base::paintEvent(static_cast<QPaintEvent *>(e)); break;
		case QEvent::Resize: Base::resizeEvent(static_cast<QResizeEvent *>(e)); break;
		case QEvent::Move: Base::moveEvent(static_cast<QMoveEvent *>(e)); break;
		case QEvent::Show: Base::showEvent(static_cast<QShowEvent *>(e)); break;
		case QEvent::Hide: Base::hideEvent(static_cast<QHideEvent *>(e)); break;
		case QEvent::Timer: Base::timerEvent(static_cast<QTimerEvent *>(e)); break;
		case QEvent::Close: e->accept(); Base::closeEvent(static_cast<QCloseEvent *>(e)); break;
		default: break;
		}
	}

	// The event being handled, for handlers that need more than upstream's
	// arguments.
	QEvent *CurrentEvent() const { return m_event; }

	bool event(QEvent *e) override
	{
		if(HandleModMessage(e))
			return true;
		if(e->type() == QEvent::ToolTip && ToolTip(static_cast<QHelpEvent *>(e)))
			return true;
		return Base::event(e);
	}

	void mousePressEvent(QMouseEvent *e) override { Mouse(e, false); }
	void mouseReleaseEvent(QMouseEvent *e) override { Mouse(e, false); }
	void mouseDoubleClickEvent(QMouseEvent *e) override { Mouse(e, true); }
	void mouseMoveEvent(QMouseEvent *e) override
	{
		Handling h(this, e);
		OnMouseMove(MouseKeyFlags(e->buttons(), e->modifiers()), e->position().toPoint());
	}
	void leaveEvent(QEvent *e) override
	{
		Handling h(this, e);
		OnMouseLeave();
	}
	void wheelEvent(QWheelEvent *e) override
	{
		Handling h(this, e);
		const uint32_t flags = MouseKeyFlags(e->buttons(), e->modifiers());
		const Point global = e->globalPosition().toPoint();
		// Shift turns a wheel's turns into horizontal ones in some toolkits;
		// Windows reports the wheel as it is.
		QPoint delta = e->angleDelta();
		if(e->inverted())
			delta = -delta;
		if(delta.y() != 0)
			OnMouseWheel(flags, static_cast<short>(delta.y()), global);
		if(delta.x() != 0)
			OnMouseHWheel(flags, static_cast<short>(-delta.x()), global);
		if(!m_defaultDone)
			e->accept();
	}
	void contextMenuEvent(QContextMenuEvent *e) override
	{
		// From the mouse, the context menu comes with the right button's
		// release (OnRButtonUp); from the keyboard, now.
		e->accept();
		if(e->reason() == QContextMenuEvent::Keyboard)
		{
			Handling h(this, e);
			OnContextMenu(this, Point(-1, -1));
		}
	}
	void keyPressEvent(QKeyEvent *e) override { Key(e); }
	void keyReleaseEvent(QKeyEvent *e) override { Key(e); }
	void focusInEvent(QFocusEvent *e) override
	{
		Handling h(this, e);
		OnSetFocus(nullptr);
	}
	void focusOutEvent(QFocusEvent *e) override
	{
		Handling h(this, e);
		OnKillFocus(QApplication::focusWidget());
	}
	void paintEvent(QPaintEvent *e) override
	{
		Handling h(this, e);
		OnPaint();
	}
	void resizeEvent(QResizeEvent *e) override
	{
		Handling h(this, e);
		const QSize client = ClientWidget()->size();
		const QWidget *top = this->window();
		const uint32_t type = (top == this && this->isMinimized()) ? size::Minimized : (top == this && this->isMaximized()) ? size::Maximized : size::Restored;
		OnSize(type, IsScrollArea ? client.width() : e->size().width(), IsScrollArea ? client.height() : e->size().height());
	}
	void moveEvent(QMoveEvent *e) override
	{
		Handling h(this, e);
		OnMove(e->pos().x(), e->pos().y());
	}
	void showEvent(QShowEvent *e) override
	{
		Handling h(this, e);
		OnShowWindow(true, 0);
	}
	void hideEvent(QHideEvent *e) override
	{
		Handling h(this, e);
		OnShowWindow(false, 0);
	}
	void timerEvent(QTimerEvent *e) override
	{
		for(auto &[id, timer] : m_timers)
		{
			if(timer.timerId() == e->timerId())
			{
				Handling h(this, e);
				m_defaultDone = true;  // a timer of ours: nothing for the base class
				OnTimer(id);
				return;
			}
		}
		Base::timerEvent(e);
	}
	void closeEvent(QCloseEvent *e) override
	{
		Handling h(this, e);
		e->ignore();
		OnClose();
	}
	void dragEnterEvent(QDragEnterEvent *e) override
	{
		if(this->acceptDrops() && e->mimeData()->hasUrls())
			e->acceptProposedAction();
		else
			Base::dragEnterEvent(e);
	}
	void dropEvent(QDropEvent *e) override
	{
		if(!this->acceptDrops() || !e->mimeData()->hasUrls())
		{
			Base::dropEvent(e);
			return;
		}
		QStringList files;
		for(const QUrl &url : e->mimeData()->urls())
		{
			if(url.isLocalFile())
				files.append(url.toLocalFile());
		}
		e->acceptProposedAction();
		if(!files.isEmpty())
			OnDropFiles(files);
	}

private:
	// Sets the event being handled, for Default(), for one handler call
	// (handlers can be nested: a handler may send another event).
	struct Handling
	{
		Window *w;
		QEvent *prevEvent;
		bool prevDone;
		Handling(Window *window, QEvent *e) : w(window), prevEvent(window->m_event), prevDone(window->m_defaultDone)
		{
			w->m_event = e;
			w->m_defaultDone = false;
		}
		~Handling()
		{
			w->m_event = prevEvent;
			w->m_defaultDone = prevDone;
		}
	};

	bool ToolTip(QHelpEvent *e)
	{
		ToolInfo ti;
		ti.hwnd = this;
		const QPoint client = ClientWidget()->mapFromGlobal(e->globalPos());
		const intptr_t id = OnToolHitTest(Point(client), &ti);
		if(id < 0)
			return false;
		if(ti.text.isEmpty())
			ti.text = GetToolTipText(ti.uId ? ti.uId : static_cast<uintptr_t>(id));
		if(ti.text.isEmpty())
		{
			QToolTip::hideText();
			return true;
		}
		const QRect area = ti.rect.IsRectEmpty() ? QRect() : QRect(ti.rect);
		QToolTip::showText(e->globalPos(), ti.text, ClientWidget(), area);
		return true;
	}

	void DefaultContextMenu()
	{
		// Windows passes an unhandled context menu on to the parent window.
		if(m_event && m_event->type() == QEvent::ContextMenu)
			static_cast<QContextMenuEvent *>(m_event)->ignore();
	}

	void Mouse(QMouseEvent *e, bool dblClk)
	{
		Handling h(this, e);
		const bool down = (e->type() != QEvent::MouseButtonRelease);
		const uint32_t flags = MouseKeyFlags(e->buttons(), e->modifiers());
		const Point pt = e->position().toPoint();
		switch(e->button())
		{
		case Qt::LeftButton:
			if(dblClk) OnLButtonDblClk(flags, pt); else if(down) OnLButtonDown(flags, pt); else OnLButtonUp(flags, pt);
			break;
		case Qt::RightButton:
			if(down)
				m_contextSent = false;
			if(dblClk) OnRButtonDblClk(flags, pt); else if(down) OnRButtonDown(flags, pt); else OnRButtonUp(flags, pt);
			break;
		case Qt::MiddleButton:
			if(dblClk) OnMButtonDblClk(flags, pt); else if(down) OnMButtonDown(flags, pt); else OnMButtonUp(flags, pt);
			break;
		case Qt::BackButton:
		case Qt::ForwardButton:
			{
				const uint32_t button = (e->button() == Qt::BackButton) ? 1 : 2;
				if(dblClk) OnXButtonDblClk(flags, button, pt); else if(down) OnXButtonDown(flags, button, pt); else OnXButtonUp(flags, button, pt);
			}
			break;
		default:
			Default();
			break;
		}
	}

	void Key(QKeyEvent *e)
	{
		Handling h(this, e);
		const Message m = KeyMessage(e, this);
		if(m.message == 0)
		{
			// The release of a held key, which Windows does not send.
			e->accept();
			return;
		}
		const uint32_t key = static_cast<uint32_t>(m.wParam);
		const uint32_t repeat = static_cast<uint32_t>(m.lParam) & 0xFFFF;
		const uint32_t flags = (static_cast<uint32_t>(m.lParam) >> 16) & 0xFFFF;
		switch(m.message)
		{
		case msg::KeyDown: OnKeyDown(key, repeat, flags); break;
		case msg::SysKeyDown: OnSysKeyDown(key, repeat, flags); break;
		case msg::KeyUp: OnKeyUp(key, repeat, flags); break;
		case msg::SysKeyUp: OnSysKeyUp(key, repeat, flags); break;
		default: break;
		}
		if(e->type() == QEvent::KeyPress)
		{
			// The characters it types (Windows' WM_CHAR, from TranslateMessage).
			for(const QChar c : e->text())
			{
				if(m.message == msg::SysKeyDown)
					OnSysChar(c.unicode(), repeat, flags);
				else
					OnChar(c.unicode(), repeat, flags);
			}
		}
		if(m_defaultDone == false)
			e->accept();
	}

	QEvent *m_event = nullptr;
	bool m_defaultDone = false;
	bool m_contextSent = true;
	std::map<uintptr_t, QBasicTimer> m_timers;
};

} // namespace mfcless
