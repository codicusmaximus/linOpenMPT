// linOpenMPT platform layer: Win32 windowing and GDI on Qt.
//
// Every HWND is a QWidget (Win32Widget). Qt delivers input, paint, focus and
// geometry events; Win32Widget turns them into the window messages upstream
// handles (WM_LBUTTONDOWN, WM_KEYDOWN, WM_PAINT, ...) and sends them to the
// window procedure, which for MFC windows routes them into message maps.
//
// Input messages go through the same pre-translation step as in a Win32
// message loop, so MFC's PreTranslateMessage (keyboard shortcuts, dialog
// navigation) sees them first.
//
// This header is internal to platform/qtwin. Include Qt before compat's
// Win32 headers, which define a few function-like macros.
#pragma once

#include <QtCore/QHash>
#include <QtCore/QPointer>
#include <QtCore/QTimer>
#include <QtGui/QCursor>
#include <QtGui/QFont>
#include <QtGui/QImage>
#include <QtGui/QKeyEvent>
#include <QtGui/QPainter>
#include <QtGui/QPen>
#include <QtGui/QBrush>
#include <QtGui/QRegion>
#include <QtWidgets/QWidget>

#include <afxwin.h>

#include <deque>
#include <map>
#include <memory>
#include <string>
#include <vector>

class QScrollBar;

namespace linopenmpt::qtwin
{

struct WindowClass
{
	std::wstring name;
	WNDCLASS wc{};
};

class Win32Widget;

// A window scroll bar (WS_HSCROLL / WS_VSCROLL), as SetScrollInfo sees it.
struct ScrollState
{
	int min = 0;
	int max = 100;
	UINT page = 0;
	int pos = 0;
	int trackPos = 0;
	bool tracking = false;
	bool shown = false;
	bool enabled = true;
	QScrollBar *bar = nullptr; // owned by the widget
};

// Per-window state that Win32 keeps for every HWND.
struct WindowData
{
	Win32Widget *widget = nullptr;
	const WindowClass *windowClass = nullptr;
	WNDPROC proc = nullptr;
	DWORD style = 0;
	DWORD exStyle = 0;
	UINT_PTR id = 0;
	LONG_PTR userData = 0;
	std::vector<LONG_PTR> extra;
	std::wstring text;
	HFONT font = nullptr;
	HMENU menu = nullptr;
	bool trackLeave = false;
	bool destroying = false;
	std::map<UINT_PTR, std::unique_ptr<QTimer>> timers;
	ScrollState scroll[2]; // SB_HORZ, SB_VERT
	bool redraw = true; // WM_SETREDRAW
};

class Win32Widget : public QWidget
{
public:
	Win32Widget(QWidget *parent, Qt::WindowFlags flags);
	~Win32Widget() override;

	HWND hwnd() const { return reinterpret_cast<HWND>(const_cast<Win32Widget *>(this)); }
	WindowData data;

	// Window pixels persist between paints, as on Windows: WM_PAINT and
	// window DCs draw into this backing image, and paintEvent() copies it
	// to the screen. So drawing outside WM_PAINT (GetDC), ScrollWindowEx and
	// XOR tracking rectangles behave as upstream expects, and an expose only
	// costs a blit.
	QImage surface;
	// The Win32 update region: what WM_PAINT still has to draw.
	QRegion invalid;
	bool erasePending = false;
	// One painter per surface, shared by all of the window's DCs.
	std::unique_ptr<QPainter> surfacePainter;
	int surfacePainterRefs = 0;

protected:
	bool event(QEvent *e) override;
	void paintEvent(QPaintEvent *e) override;
	void mousePressEvent(QMouseEvent *e) override;
	void mouseReleaseEvent(QMouseEvent *e) override;
	void mouseDoubleClickEvent(QMouseEvent *e) override;
	void mouseMoveEvent(QMouseEvent *e) override;
	void wheelEvent(QWheelEvent *e) override;
	void keyPressEvent(QKeyEvent *e) override;
	void keyReleaseEvent(QKeyEvent *e) override;
	void focusInEvent(QFocusEvent *e) override;
	void focusOutEvent(QFocusEvent *e) override;
	void resizeEvent(QResizeEvent *e) override;
	void moveEvent(QMoveEvent *e) override;
	void closeEvent(QCloseEvent *e) override;
	void leaveEvent(QEvent *e) override;
	void contextMenuEvent(QContextMenuEvent *e) override;
	void showEvent(QShowEvent *e) override;
	void hideEvent(QHideEvent *e) override;
};

// HWND <-> widget. Returns nullptr for handles that are not (or no longer)
// windows.
Win32Widget *FromHandle(HWND hwnd);
WindowData *Data(HWND hwnd);

// Messages from Qt input: pre-translated, then dispatched, like a message
// loop iteration.
void PumpInputMessage(const MSG &msg);

// Set by the MFC layer: returns TRUE when the message was handled before
// dispatch (CWinThread::PreTranslateMessage).
using PreTranslateHook = BOOL (*)(MSG *msg);
void SetPreTranslateHook(PreTranslateHook hook);

// Keyboard (keymap.cpp): Win32 virtual-key and scan codes for Qt key
// events, and the key state GetKeyState() reports.
struct KeyInfo
{
	UINT vk = 0;
	UINT scanCode = 0;
	bool extended = false;
};
KeyInfo KeyFromQt(const QKeyEvent *e);
void SetKeyState(UINT vk, bool down, UINT scanCode = 0, bool extended = false);
// Forgets held keys, e.g. when the application loses focus.
void ResetKeyState();
WPARAM MouseKeyFlags(Qt::MouseButtons buttons, Qt::KeyboardModifiers modifiers);

QString ToQString(LPCWSTR text, int count = -1);
std::wstring ToWString(const QString &text);
QColor ToQColor(COLORREF color);
COLORREF ToColorRef(const QColor &color);

// GDI (gdi32.cpp). The backing image follows the widget's size and device
// pixel ratio; returns true when it was (re)allocated.
bool EnsureSurface(Win32Widget *w);
// Moves pixels of the backing image (ScrollWindowEx).
void ScrollSurface(Win32Widget *w, int dx, int dy, const QRect &area);
// Called before a window goes away: invalidates its DCs.
void ReleaseWindowDCs(Win32Widget *w);

// Window scroll bars and the client area they leave (system.cpp).
QRect ClientRect(const Win32Widget *w);
void InitScrollBars(Win32Widget *w);
void LayoutScrollBars(Win32Widget *w);
void ScrollStylesChanged(Win32Widget *w, DWORD oldStyle);

// Icons and cursors (HICON, HCURSOR) point to one of these.
struct IconData
{
	QImage image;
	QPoint hotspot;
	bool isCursor = false;
	int sharedCursorShape = -1; // Qt::CursorShape for system cursors
};
HCURSOR SystemCursor(Qt::CursorShape shape);
QCursor ToQCursor(HCURSOR cursor);

} // namespace linopenmpt::qtwin
