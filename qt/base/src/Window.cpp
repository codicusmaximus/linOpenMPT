/*
 * Window.cpp
 * ----------
 * Purpose: The message filter: PreTranslateMessage for key messages.
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */

#include "mfcless/Window.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QPointer>
#include <QtWidgets/QLineEdit>

namespace mfcless
{

namespace
{

class MessageFilter : public QObject
{
public:
	using QObject::QObject;

	bool eventFilter(QObject *watched, QEvent *e) override
	{
		const QEvent::Type type = e->type();
		const bool key = (type == QEvent::KeyPress || type == QEvent::KeyRelease);
		const bool mouse = (type == QEvent::MouseButtonPress || type == QEvent::MouseButtonRelease || type == QEvent::MouseButtonDblClick || type == QEvent::MouseMove);
		if(!key && !mouse)
			return false;
		auto *widget = qobject_cast<QWidget *>(watched);
		if(!widget)
			return false;
		// Qt delivers an input event to each parent that ignores it; the
		// filter sees each of these deliveries, but the message loop sees the
		// message once.
		auto *ie = static_cast<QInputEvent *>(e);
		const Seen seen{ie, ie->timestamp(), type};
		if(seen == m_last && widget != m_lastReceiver)
			return false;
		m_last = seen;
		m_lastReceiver = widget;
		if(mouse)
			return PreTranslate(widget, MouseMessage(static_cast<QMouseEvent *>(e), widget));
		auto *ke = static_cast<QKeyEvent *>(e);

		// The key state first, so that PreTranslateMessage sees it.
		const KeyInfo info = KeyFromQt(ke);
		if(!(e->type() == QEvent::KeyRelease && ke->isAutoRepeat()))
		{
			if(info.vk)
				SetKeyState(info.vk, e->type() == QEvent::KeyPress, info.scanCode, info.extended);
			if(info.vk != vk::SHIFT && info.vk != vk::CONTROL && info.vk != vk::MENU)
				SyncModifierState(ke->modifiers());
		}

		return PreTranslate(widget, KeyMessage(ke, widget));
	}

private:
	static Message MouseMessage(QMouseEvent *e, QWidget *widget)
	{
		Message m;
		m.hwnd = widget;
		m.event = e;
		const bool dbl = (e->type() == QEvent::MouseButtonDblClick), up = (e->type() == QEvent::MouseButtonRelease);
		switch(e->type() == QEvent::MouseMove ? Qt::NoButton : e->button())
		{
		case Qt::NoButton: m.message = msg::MouseMove; break;
		case Qt::LeftButton: m.message = dbl ? msg::LButtonDblClk : up ? msg::LButtonUp : msg::LButtonDown; break;
		case Qt::RightButton: m.message = dbl ? msg::RButtonDblClk : up ? msg::RButtonUp : msg::RButtonDown; break;
		case Qt::MiddleButton: m.message = up ? msg::MButtonUp : msg::MButtonDown; break;
		default: m.message = up ? msg::XButtonUp : msg::XButtonDown; break;
		}
		m.wParam = MouseKeyFlags(e->buttons(), e->modifiers());
		const QPoint pt = e->position().toPoint();
		m.lParam = static_cast<intptr_t>(static_cast<uint32_t>(static_cast<uint16_t>(pt.x())) | (static_cast<uint32_t>(static_cast<uint16_t>(pt.y())) << 16));
		return m;
	}

	static bool PreTranslate(QWidget *widget, Message m)
	{
		if(m.message == 0)
			return false;
		// Innermost first, up to the top-level window.
		for(QWidget *w = widget; w; w = w->parentWidget())
		{
			if(auto *window = dynamic_cast<WindowBase *>(w))
			{
				QPointer<QWidget> guard = widget;
				if(window->PreTranslateMessage(&m))
					return true;
				if(!guard)
					return true;
			}
			if(w->isWindow())
				break;
		}
		return false;
	}

	struct Seen
	{
		const QInputEvent *event = nullptr;
		quint64 timestamp = 0;
		QEvent::Type type = QEvent::None;
		bool operator==(const Seen &o) const { return event == o.event && timestamp == o.timestamp && type == o.type; }
	};
	Seen m_last;
	const QWidget *m_lastReceiver = nullptr;
};

} // namespace

void InstallMessageFilter()
{
	static MessageFilter *filter = nullptr;
	if(filter || !QCoreApplication::instance())
		return;
	filter = new MessageFilter(QCoreApplication::instance());
	QCoreApplication::instance()->installEventFilter(filter);
}

} // namespace mfcless


namespace mfcless
{

void GotoDlgCtrl(QWidget *w)
{
	if(!w)
		return;
	w->setFocus(Qt::TabFocusReason);
	if(auto *edit = qobject_cast<QLineEdit *>(w))
		edit->selectAll();
}

} // namespace mfcless
