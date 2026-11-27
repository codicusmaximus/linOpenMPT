/*
 * ForeignWindows.cpp
 * ------------------
 * Purpose: Other programs' X11 windows inside ours (plugin editors).
 * Notes  : The same as linOpenMPT's MFC build does it (mfc-qt's embed.cpp).
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */

#include "mfcless/ForeignWindows.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QPointer>
#include <QtCore/QTimer>
#include <QtGui/QGuiApplication>
#include <QtGui/QWindow>
#include <QtWidgets/QWidget>

#include <algorithm>
#include <vector>

#ifdef MFCLESS_WITH_XCB
#include <cstdlib>
#include <xcb/xcb.h>
#endif

namespace mfcless
{

bool CanEmbedForeignWindows()
{
	return QGuiApplication::platformName() == QLatin1String("xcb");
}


uintptr_t NativeWindowId(QWidget *w)
{
	if(w == nullptr || !CanEmbedForeignWindows())
		return 0;
	// The plugin draws into its own window and Qt must not paint over it.
	w->setAttribute(Qt::WA_NativeWindow);
	w->setAttribute(Qt::WA_DontCreateNativeAncestors);
	// Pictures of the window must come from the screen (main.cpp).
	w->window()->setProperty("mfclessForeignWindows", true);
	return static_cast<uintptr_t>(w->winId());
}

namespace
{

// A Windows plugin's editor (a Wine window) inside one of ours. Wine
// places its windows itself and translates mouse positions with where it
// believes they are on the screen; inside our window that is wrong, and
// Wine ignores what the X server says about windows it does not let a
// window manager handle. So whenever our window gets somewhere, the bridge
// moves its window to that screen position (Wine believes it, and moves the
// X window there inside ours) and we put the X window back at our top
// left, which Wine does not notice.
class ForeignWindowTracker : public QObject
{
public:
	ForeignWindowTracker(QWidget *container, QWindow *child, void (*moved)(void *, int, int), void *context)
		: QObject(container), m_container(container), m_child(child), m_moved(moved), m_context(context)
	{
		container->window()->installEventFilter(this);
		container->installEventFilter(this);
	}

	QWindow *Child() const { return m_child; }

	void Update()
	{
		if(m_container == nullptr || m_child == nullptr)
			return;
		const QPoint screen = ScreenPosition();
		if(m_moved)
			m_moved(m_context, screen.x(), screen.y());
		// Wine moves its window when it gets to it; back to our top left,
		// now and once Wine is done.
		PutBack();
		QTimer::singleShot(50, this, [this]() { PutBack(); });
		QTimer::singleShot(300, this, [this]() { PutBack(); });
	}

	bool eventFilter(QObject *watched, QEvent *event) override
	{
		switch(event->type())
		{
		case QEvent::Move:
		case QEvent::Show:
			// Once the window system has the new position.
			QTimer::singleShot(0, this, [this]() { Update(); });
			break;
		default:
			break;
		}
		return QObject::eventFilter(watched, event);
	}

private:
	// Where our window is on the screen, in the X server's pixels (Wine
	// knows nothing of Qt's scaling).
	QPoint ScreenPosition() const
	{
#ifdef MFCLESS_WITH_XCB
		auto *x11 = qGuiApp->nativeInterface<QNativeInterface::QX11Application>();
		if(xcb_connection_t *connection = x11 != nullptr ? x11->connection() : nullptr)
		{
			const xcb_window_t window = static_cast<xcb_window_t>(m_container->winId());
			xcb_screen_t *screen = xcb_setup_roots_iterator(xcb_get_setup(connection)).data;
			if(xcb_translate_coordinates_reply_t *reply = xcb_translate_coordinates_reply(connection, xcb_translate_coordinates(connection, window, screen->root, 0, 0), nullptr))
			{
				const QPoint position(reply->dst_x, reply->dst_y);
				std::free(reply);
				return position;
			}
		}
#endif
		return m_container->mapToGlobal(QPoint(0, 0)) * m_container->devicePixelRatioF();
	}

	void PutBack()
	{
		if(m_child == nullptr)
			return;
#ifdef MFCLESS_WITH_XCB
		// Straight to the X server: Qt believes the window is still where
		// it put it, and would do nothing.
		auto *x11 = qGuiApp->nativeInterface<QNativeInterface::QX11Application>();
		if(xcb_connection_t *connection = x11 != nullptr ? x11->connection() : nullptr)
		{
			const uint32_t position[] = {0, 0};
			xcb_configure_window(connection, static_cast<xcb_window_t>(m_child->winId()), XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, position);
			xcb_flush(connection);
			return;
		}
#endif
		m_child->setPosition(1, 1);
		m_child->setPosition(0, 0);
	}

	QPointer<QWidget> m_container;
	QPointer<QWindow> m_child;
	void (*m_moved)(void *, int, int);
	void *m_context;
};

std::vector<QPointer<ForeignWindowTracker>> &Trackers()
{
	static std::vector<QPointer<ForeignWindowTracker>> trackers;
	return trackers;
}

} // namespace


bool EmbedX11Window(uintptr_t parent, uintptr_t child, void (*moved)(void *context, int x, int y), void *context)
{
	if(parent == 0 || child == 0 || !CanEmbedForeignWindows())
		return false;
	// Foreign windows: Qt reparents the X11 windows themselves.
	QWindow *parentWindow = QWindow::fromWinId(static_cast<WId>(parent));
	QWindow *childWindow = QWindow::fromWinId(static_cast<WId>(child));
	if(parentWindow == nullptr || childWindow == nullptr)
		return false;
	childWindow->setParent(parentWindow);
	childWindow->setPosition(0, 0);
	QWidget *container = QWidget::find(static_cast<WId>(parent));
	if(container == nullptr)
		return true;
	// The Qt wrappers go with the container.
	QObject::connect(container, &QObject::destroyed, parentWindow, &QObject::deleteLater);
	auto *tracker = new ForeignWindowTracker(container, childWindow, moved, context);
	Trackers().erase(std::remove(Trackers().begin(), Trackers().end(), nullptr), Trackers().end());
	Trackers().push_back(tracker);
	tracker->Update();
	return true;
}


void UnembedX11Window(uintptr_t child)
{
	for(auto &tracker : Trackers())
	{
		if(tracker != nullptr && tracker->Child() != nullptr && tracker->Child()->winId() == static_cast<WId>(child))
			delete tracker.data();
	}
	Trackers().erase(std::remove(Trackers().begin(), Trackers().end(), nullptr), Trackers().end());
}


void PostToMainThread(void (*fn)(void *context), void *context)
{
	QCoreApplication *app = QCoreApplication::instance();
	if(app == nullptr || fn == nullptr)
		return;
	QMetaObject::invokeMethod(app, [fn, context]() { fn(context); }, Qt::QueuedConnection);
}

} // namespace mfcless
