// Foreign windows inside Win32 windows: plugin editors. A Linux VST2 plugin
// opens its editor as a child of an X11 window it is given (effEditOpen).
#include "QtWin.h"

#include <QGuiApplication>
#include <QWidget>

BOOL LinOpenMPTCanEmbedForeignWindows()
{
	// Only X11 (also XWayland) has window ids that another toolkit in the
	// process can parent its windows to.
	return QGuiApplication::platformName() == QLatin1String("xcb") ? TRUE : FALSE;
}

uintptr_t LinOpenMPTNativeWindowId(HWND hwnd)
{
	linopenmpt::qtwin::Win32Widget *w = linopenmpt::qtwin::FromHandle(hwnd);
	if(w == nullptr || !LinOpenMPTCanEmbedForeignWindows())
		return 0;
	// The plugin draws into its own window and Qt must not paint over it.
	w->setAttribute(Qt::WA_NativeWindow);
	w->setAttribute(Qt::WA_DontCreateNativeAncestors);
	// Pictures of the window must come from the screen (app/main.cpp).
	w->window()->setProperty("linopenmptForeignWindows", true);
	return static_cast<uintptr_t>(w->winId());
}
