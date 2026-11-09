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
class QMenuBar;

namespace linopenmpt::qtwin
{

struct WindowClass
{
	std::wstring name;
	WNDCLASS wc{};
};

class Win32Widget;

// What a built-in control (button, edit, list box, ...) keeps per window.
struct ControlState
{
	virtual ~ControlState() = default;
};

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
	// The non-client area (from WM_NCCALCSIZE) around the client area, and
	// whether it needs WM_NCPAINT.
	QMargins nonClient;
	bool ncDirty = true;
	QSize clientSize; // as last reported in WM_SIZE
	QMenuBar *menuBar = nullptr; // top-level windows with a menu (SetMenu)
	bool transparent = false; // see SetTransparent()
	std::unique_ptr<ControlState> control; // built-in controls
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

	// WM_SETFOCUS and WM_KILLFOCUS, also for a Qt widget inside the window
	// that holds the focus for it (edit controls).
	void HandleFocusIn();
	void HandleFocusOut();
	// Key events of a Qt widget inside the window, delivered as the
	// window's own WM_KEYDOWN / WM_CHAR / WM_KEYUP.
	void DeliverKey(QKeyEvent *e) { if(e->type() == QEvent::KeyPress) keyPressEvent(e); else keyReleaseEvent(e); }

protected:
	bool event(QEvent *e) override;
	// Tab is a key like any other; dialogs move the focus (IsDialogMessage).
	bool focusNextPrevChild(bool) override { return false; }
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
// The window a widget belongs to: itself, or the window around it.
HWND WindowOf(QWidget *widget);

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
// Brings Shift, Ctrl and Alt in line with a Qt event's modifiers.
void SyncModifierState(Qt::KeyboardModifiers modifiers);
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

// The non-client area (nonclient.cpp). Client coordinates of a window are
// widget coordinates minus ClientOrigin(); child windows are placed in their
// parent's client coordinates.
QRect ClientRect(const Win32Widget *w); // in widget coordinates
QPoint ClientOrigin(const QWidget *widget); // (0, 0) for non-Win32 widgets
// Recomputes the client area with WM_NCCALCSIZE; true if it changed.
bool UpdateNonClient(Win32Widget *w);
QMargins FrameMargins(DWORD style, DWORD exStyle, bool child);
int MenuBarHeight(const Win32Widget *w);
void DefNcCalcSize(Win32Widget *w, RECT *rect);
void DefNcPaint(Win32Widget *w);
LRESULT DefNcHitTest(Win32Widget *w, QPoint screen);

// Window scroll bars (system.cpp, laid out by nonclient.cpp).
void InitScrollBars(Win32Widget *w);
void LayoutScrollBars(Win32Widget *w);
void ScrollStylesChanged(Win32Widget *w, DWORD oldStyle);
QScrollBar *EnsureScrollBar(Win32Widget *w, int bar);
void SyncScrollBar(Win32Widget *w, int bar);

// Menus (menu.cpp): the menu bar of a top-level window.
void LayoutMenuBar(Win32Widget *w);
void AttachMenuBar(Win32Widget *w, HMENU menu);
// Opens the menu bar item whose mnemonic is key (Alt+key); false if none.
bool OpenMenuByKey(Win32Widget *w, WCHAR key);

// Built-in controls (controls.cpp, edit.cpp, listbox.cpp, combobox.cpp):
// registers the system window classes ("Button", "Edit", ...).
void RegisterSystemClasses();
// DefWindowProc's WM_CTLCOLOR* answer: system colours.
LRESULT DefCtlColor(HDC dc, UINT msg);
// The state of a built-in control, if hwnd is one of type T.
template <typename T>
T *Control(HWND hwnd)
{
	WindowData *d = Data(hwnd);
	return d != nullptr ? dynamic_cast<T *>(d->control.get()) : nullptr;
}
// True while an edit hands a key event back to its Qt widget (edit.cpp).
extern bool g_reinjecting;
// What a combo box's list tells it besides LBN_* (listbox.cpp, combobox.cpp).
constexpr UINT ComboChosen = 0xFFFF; // an item was clicked
constexpr UINT ComboCancel = 0xFFFE; // closed by clicking elsewhere
// WM_COMMAND notifications to a control's parent.
void NotifyParent(HWND ctl, UINT code);
// The brush (and DC colours) a control's parent chooses with WM_CTLCOLOR*.
HBRUSH ControlBrush(HWND ctl, HDC dc, UINT msg);

// Re-enables the owned windows Qt disabled along with w.
void KeepOwnedWindowsEnabled(QWidget *w);

// While one of these lives, a Qt modal loop runs: no idle processing.
struct ForeignModalLoop
{
	ForeignModalLoop();
	~ForeignModalLoop();
	ForeignModalLoop(const ForeignModalLoop &) = delete;
	ForeignModalLoop &operator=(const ForeignModalLoop &) = delete;
};

// Dialogs (dialog.cpp): the dialog manager's part of a message loop (for
// DialogBox), and the default button following the focus.
bool HandleDialogMessage(const MSG &msg);
void DialogFocusChanged(HWND focus);

// Icons and cursors (HICON, HCURSOR) point to one of these.
struct IconData
{
	QImage image;
	QPoint hotspot;
	bool isCursor = false;
	int sharedCursorShape = -1; // Qt::CursorShape for system cursors
	bool shared = false;        // system icons and cursors are never destroyed
};
HCURSOR SystemCursor(Qt::CursorShape shape);
// A device-independent bitmap as a top-down image (copied).
QImage DecodeDib(const void *bits, const BITMAPINFO *bmi, int lines = -1);
// The painter that draws for a DC, set up with its clip and origin (for
// QStyle drawing); nullptr if the DC cannot be drawn on. Also the DC's
// font and text colour, and an HFONT's font.
QPainter *DCPainter(HDC dc);
QFont DCFont(HDC dc);
COLORREF DCTextColor(HDC dc);
QFont FontOf(HFONT font);
// Makes a window's surface transparent where it draws nothing, so its
// parent shows through (group boxes, which Windows does not erase inside).
void SetTransparent(Win32Widget *w);
// A copy of a bitmap's pixels.
QImage BitmapImage(HBITMAP bitmap);
// An image of an image list (imagelist.cpp); null if there is none.
QImage ImageListImage(HIMAGELIST list, int index);
// Icons and cursors from a module's resources (resource.cpp).
HICON LoadResourceIcon(HINSTANCE module, LPCTSTR name, bool cursor, int cx, int cy, bool shared);
QCursor ToQCursor(HCURSOR cursor);

} // namespace linopenmpt::qtwin
