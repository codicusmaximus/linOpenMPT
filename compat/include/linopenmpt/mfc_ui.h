// linOpenMPT compatibility layer: declarations of MFC UI classes.
//
// Upstream's shared headers (Mptrack.h, Mainfrm.h, Moddoc.h, ...) mention MFC
// window classes in member declarations and signatures. These skeletons let
// those headers compile unchanged. They have no behaviour. Virtual functions
// have do-nothing defaults (as MFC's base implementations mostly do), so the
// classes' vtables exist; non-virtual methods are declared but not defined,
// so any direct UI call that reaches a Linux binary shows up as a link error,
// which is exactly the list of things the Qt UI has to provide. Simple value types (CRect, CPoint, CSize) and the document base
// class are implemented, because non-UI logic uses them directly.

#pragma once

#include "afx.h"
#include "mfc_msgmap.h"
#include "../afxres.h"

// ---- window messages and constants used in shared headers ----------------
#define DT_TOP 0x0000
#define DT_LEFT 0x0000
#define DT_CENTER 0x0001
#define DT_RIGHT 0x0002
#define DT_VCENTER 0x0004
#define DT_BOTTOM 0x0008
#define DT_SINGLELINE 0x0020
#define DT_NOPREFIX 0x0800
#define HOTKEYF_SHIFT 0x01
#define HOTKEYF_CONTROL 0x02
#define HOTKEYF_ALT 0x04
#define HOTKEYF_EXT 0x08
#define MB_OK 0x0000
#define MB_OKCANCEL 0x0001
#define MB_YESNOCANCEL 0x0003
#define MB_YESNO 0x0004
#define MB_ICONERROR 0x0010
#define MB_ICONQUESTION 0x0020
#define MB_ICONWARNING 0x0030
#define MB_ICONINFORMATION 0x0040
#define MB_ABORTRETRYIGNORE 0x0002
#define MB_RETRYCANCEL 0x0005
#define MB_CANCELTRYCONTINUE 0x0006
#define MB_ICONHAND MB_ICONERROR
#define MB_ICONSTOP MB_ICONERROR
#define MB_ICONEXCLAMATION MB_ICONWARNING
#define MB_ICONASTERISK MB_ICONINFORMATION
#define MB_DEFBUTTON1 0x0000
#define MB_DEFBUTTON2 0x0100
#define MB_DEFBUTTON3 0x0200
#define MB_APPLMODAL 0x0000
#define MB_SYSTEMMODAL 0x1000
#define MB_TASKMODAL 0x2000
#define MB_SETFOREGROUND 0x00010000
#define MB_TOPMOST 0x00040000
#define BST_UNCHECKED 0x0000
#define BST_CHECKED 0x0001
#define BST_INDETERMINATE 0x0002
#define BST_PUSHED 0x0004
#define BST_FOCUS 0x0008
#define IDOK 1
#define IDCANCEL 2
#define IDABORT 3
#define IDRETRY 4
#define IDIGNORE 5
#define IDYES 6
#define IDNO 7
#define IDCLOSE 8
#define IDHELP 9
#define IDTRYAGAIN 10
#define IDCONTINUE 11
int MessageBox(HWND owner, LPCTSTR text, LPCTSTR caption, UINT type);
BOOL MessageBeep(UINT type);

struct CRuntimeClass;

// MFC's opaque list iterator.
struct __POSITION { };
using POSITION = __POSITION *;

struct CCreateContext
{
	CRuntimeClass *m_pNewViewClass = nullptr;
	class CDocument *m_pCurrentDoc = nullptr;
	class CDocTemplate *m_pNewDocTemplate = nullptr;
	class CView *m_pLastView = nullptr;
	class CFrameWnd *m_pCurrentFrame = nullptr;
};

struct CPrintInfo;

// ---- window styles, SetWindowPos flags, misc constants ---------------------
#define WS_OVERLAPPED 0x00000000L
#define WS_POPUP 0x80000000L
#define WS_CHILD 0x40000000L
#define WS_MINIMIZE 0x20000000L
#define WS_VISIBLE 0x10000000L
#define WS_DISABLED 0x08000000L
#define WS_CLIPSIBLINGS 0x04000000L
#define WS_CLIPCHILDREN 0x02000000L
#define WS_MAXIMIZE 0x01000000L
#define WS_CAPTION 0x00C00000L
#define WS_BORDER 0x00800000L
#define WS_DLGFRAME 0x00400000L
#define WS_VSCROLL 0x00200000L
#define WS_HSCROLL 0x00100000L
#define WS_SYSMENU 0x00080000L
#define WS_THICKFRAME 0x00040000L
#define WS_GROUP 0x00020000L
#define WS_TABSTOP 0x00010000L
#define WS_MINIMIZEBOX 0x00020000L
#define WS_MAXIMIZEBOX 0x00010000L
#define WS_OVERLAPPEDWINDOW (WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX)
#define WS_EX_TOOLWINDOW 0x00000080L
#define WS_EX_CLIENTEDGE 0x00000200L
#define WS_EX_STATICEDGE 0x00020000L
#define ES_LEFT 0x0000L
#define ES_CENTER 0x0001L
#define ES_RIGHT 0x0002L
#define ES_MULTILINE 0x0004L
#define ES_AUTOVSCROLL 0x0040L
#define ES_AUTOHSCROLL 0x0080L
#define ES_READONLY 0x0800L
#define ES_WANTRETURN 0x1000L
#define ES_NUMBER 0x2000L
#define SWP_NOSIZE 0x0001
#define SWP_NOMOVE 0x0002
#define SWP_NOZORDER 0x0004
#define SWP_NOREDRAW 0x0008
#define SWP_NOACTIVATE 0x0010
#define SWP_FRAMECHANGED 0x0020
#define SWP_SHOWWINDOW 0x0040
#define SWP_HIDEWINDOW 0x0080
#define SWP_NOOWNERZORDER 0x0200
#define GWL_STYLE (-16)
#define GWL_EXSTYLE (-20)
#define GWLP_WNDPROC (-4)
#define GWLP_ID (-12)
#define GWLP_USERDATA (-21)
#define LB_ERR (-1)
#define CB_ERR (-1)
#define LB_ERRSPACE (-2)
#define CB_ERRSPACE (-2)
LONG_PTR GetWindowLongPtr(HWND hwnd, int index);
LONG_PTR SetWindowLongPtr(HWND hwnd, int index, LONG_PTR value);
LONG GetWindowLong(HWND hwnd, int index);
LONG SetWindowLong(HWND hwnd, int index, LONG value);

// Owner-drawn controls.
#define ODT_MENU 1
#define ODT_LISTBOX 2
#define ODT_COMBOBOX 3
#define ODT_BUTTON 4
#define ODS_SELECTED 0x0001
#define ODS_GRAYED 0x0002
#define ODS_DISABLED 0x0004
#define ODS_CHECKED 0x0008
#define ODS_FOCUS 0x0010
struct DRAWITEMSTRUCT
{
	UINT CtlType; UINT CtlID; UINT itemID; UINT itemAction; UINT itemState;
	HWND hwndItem; HDC hDC; RECT rcItem; ULONG_PTR itemData;
};
using LPDRAWITEMSTRUCT = DRAWITEMSTRUCT *;
struct MEASUREITEMSTRUCT
{
	UINT CtlType; UINT CtlID; UINT itemID; UINT itemWidth; UINT itemHeight; ULONG_PTR itemData;
};
using LPMEASUREITEMSTRUCT = MEASUREITEMSTRUCT *;

struct NMHDR { HWND hwndFrom; UINT_PTR idFrom; UINT code; };
using LPNMHDR = NMHDR *;
struct MINMAXINFO { POINT ptReserved; POINT ptMaxSize; POINT ptMaxPosition; POINT ptMinTrackSize; POINT ptMaxTrackSize; };
struct TOOLINFO { UINT cbSize; UINT uFlags; HWND hwnd; UINT_PTR uId; RECT rect; HINSTANCE hinst; LPTSTR lpszText; LPARAM lParam; void *lpReserved; };
using TTTOOLINFO = TOOLINFO;
struct _EXCEPTION_RECORD;
struct _CONTEXT;
struct _EXCEPTION_POINTERS { _EXCEPTION_RECORD *ExceptionRecord; _CONTEXT *ContextRecord; };
using EXCEPTION_POINTERS = _EXCEPTION_POINTERS;

// ---- value types ---------------------------------------------------------
class CSize : public SIZE
{
public:
	CSize() noexcept { cx = 0; cy = 0; }
	CSize(int x, int y) noexcept { cx = x; cy = y; }
	CSize(SIZE s) noexcept { cx = s.cx; cy = s.cy; }
	bool operator==(const SIZE &o) const noexcept { return cx == o.cx && cy == o.cy; }
	bool operator!=(const SIZE &o) const noexcept { return !(*this == o); }
	CSize operator+(SIZE o) const noexcept { return CSize(cx + o.cx, cy + o.cy); }
	CSize operator-(SIZE o) const noexcept { return CSize(cx - o.cx, cy - o.cy); }
};

class CPoint : public POINT
{
public:
	CPoint() noexcept { x = 0; y = 0; }
	CPoint(int x_, int y_) noexcept { x = x_; y = y_; }
	CPoint(POINT p) noexcept { x = p.x; y = p.y; }
	void Offset(int dx, int dy) noexcept { x += dx; y += dy; }
	void SetPoint(int x_, int y_) noexcept { x = x_; y = y_; }
	bool operator==(const POINT &o) const noexcept { return x == o.x && y == o.y; }
	bool operator!=(const POINT &o) const noexcept { return !(*this == o); }
	CPoint operator+(SIZE s) const noexcept { return CPoint(x + s.cx, y + s.cy); }
	CPoint operator-(SIZE s) const noexcept { return CPoint(x - s.cx, y - s.cy); }
	CSize operator-(POINT p) const noexcept { return CSize(x - p.x, y - p.y); }
	CPoint &operator+=(SIZE s) noexcept { x += s.cx; y += s.cy; return *this; }
	CPoint &operator-=(SIZE s) noexcept { x -= s.cx; y -= s.cy; return *this; }
};

class CRect : public RECT
{
public:
	CRect() noexcept { left = top = right = bottom = 0; }
	CRect(int l, int t, int r, int b) noexcept { left = l; top = t; right = r; bottom = b; }
	CRect(const RECT &r) noexcept { left = r.left; top = r.top; right = r.right; bottom = r.bottom; }
	CRect(const RECT *r) noexcept { left = r->left; top = r->top; right = r->right; bottom = r->bottom; }
	CRect(POINT p, SIZE s) noexcept { left = p.x; top = p.y; right = p.x + s.cx; bottom = p.y + s.cy; }
	CRect(POINT tl, POINT br) noexcept { left = tl.x; top = tl.y; right = br.x; bottom = br.y; }
	int Width() const noexcept { return right - left; }
	int Height() const noexcept { return bottom - top; }
	CSize Size() const noexcept { return CSize(Width(), Height()); }
	CPoint TopLeft() const noexcept { return CPoint(left, top); }
	CPoint BottomRight() const noexcept { return CPoint(right, bottom); }
	CPoint CenterPoint() const noexcept { return CPoint((left + right) / 2, (top + bottom) / 2); }
	bool IsRectEmpty() const noexcept { return right <= left || bottom <= top; }
	bool IsRectNull() const noexcept { return left == 0 && top == 0 && right == 0 && bottom == 0; }
	bool PtInRect(POINT p) const noexcept { return p.x >= left && p.x < right && p.y >= top && p.y < bottom; }
	void SetRect(int l, int t, int r, int b) noexcept { left = l; top = t; right = r; bottom = b; }
	void SetRectEmpty() noexcept { left = top = right = bottom = 0; }
	void OffsetRect(int dx, int dy) noexcept { left += dx; right += dx; top += dy; bottom += dy; }
	void OffsetRect(POINT p) noexcept { OffsetRect(p.x, p.y); }
	void OffsetRect(SIZE s) noexcept { OffsetRect(s.cx, s.cy); }
	void MoveToXY(int x, int y) noexcept { OffsetRect(x - left, y - top); }
	void MoveToX(int x) noexcept { OffsetRect(x - left, 0); }
	void MoveToY(int y) noexcept { OffsetRect(0, y - top); }
	void InflateRect(int dx, int dy) noexcept { left -= dx; top -= dy; right += dx; bottom += dy; }
	void InflateRect(int l, int t, int r, int b) noexcept { left -= l; top -= t; right += r; bottom += b; }
	void DeflateRect(int dx, int dy) noexcept { InflateRect(-dx, -dy); }
	void DeflateRect(int l, int t, int r, int b) noexcept { InflateRect(-l, -t, -r, -b); }
	void NormalizeRect() noexcept
	{
		if(left > right) std::swap(left, right);
		if(top > bottom) std::swap(top, bottom);
	}
	bool IntersectRect(const RECT *a, const RECT *b) noexcept
	{
		left = std::max(a->left, b->left); top = std::max(a->top, b->top);
		right = std::min(a->right, b->right); bottom = std::min(a->bottom, b->bottom);
		if(IsRectEmpty()) { SetRectEmpty(); return false; }
		return true;
	}
	bool UnionRect(const RECT *a, const RECT *b) noexcept
	{
		left = std::min(a->left, b->left); top = std::min(a->top, b->top);
		right = std::max(a->right, b->right); bottom = std::max(a->bottom, b->bottom);
		return !IsRectEmpty();
	}
	operator RECT *() noexcept { return this; }
	operator const RECT *() const noexcept { return this; }
	bool operator==(const RECT &o) const noexcept { return left == o.left && top == o.top && right == o.right && bottom == o.bottom; }
	bool operator!=(const RECT &o) const noexcept { return !(*this == o); }
};

// ---- runtime class / message map macros ----------------------------------
struct CRuntimeClass { const char *m_lpszClassName; };
#define RUNTIME_CLASS(c) (static_cast<CRuntimeClass *>(nullptr))
#define DECLARE_DYNAMIC(c)
#define DECLARE_DYNCREATE(c)
#define DECLARE_SERIAL(c)
#define IMPLEMENT_DYNAMIC(c, b)
#define IMPLEMENT_DYNCREATE(c, b)
#define IMPLEMENT_SERIAL(c, b, s)

// ---- command routing -----------------------------------------------------
class CCmdTarget : public CObject
{
public:
	virtual ~CCmdTarget() = default;
	virtual BOOL OnCmdMsg(UINT id, int code, void *extra, void *handlerInfo);
protected:
	static const AFX_MSGMAP *GetThisMessageMap();
	virtual const AFX_MSGMAP *GetMessageMap() const { return GetThisMessageMap(); }
public:
	void BeginWaitCursor();
	void EndWaitCursor();
	void RestoreWaitCursor();
};

class CCmdUI
{
public:
	UINT m_nID = 0;
	virtual void Enable(BOOL on = TRUE) { }
	virtual void SetCheck(int check = 1) { }
	virtual void SetRadio(BOOL on = TRUE) { }
	virtual void SetText(LPCTSTR text) { }
	virtual ~CCmdUI() = default;
};

class CDataExchange
{
public:
	BOOL m_bSaveAndValidate = FALSE;
};

// ---- GDI -----------------------------------------------------------------
class CGdiObject : public CObject
{
public:
	HGDIOBJ m_hObject = nullptr;
	BOOL DeleteObject();
	operator HGDIOBJ() const { return m_hObject; }
};
class CPen : public CGdiObject { };
class CBrush : public CGdiObject { };
class CFont : public CGdiObject { };
class CBitmap : public CGdiObject { };
class CRgn : public CGdiObject { };
class CPalette : public CGdiObject { };
class CDC : public CObject
{
public:
	HDC m_hDC = nullptr;
	operator HDC() const { return m_hDC; }
};
class CPaintDC : public CDC { };
class CClientDC : public CDC { };
class CWindowDC : public CDC { };
class CImageList : public CObject
{
public:
	HIMAGELIST m_hImageList = nullptr;
};

// ---- windows -------------------------------------------------------------
class CMenu : public CObject
{
public:
	HMENU m_hMenu = nullptr;
	operator HMENU() const { return m_hMenu; }
	BOOL CreatePopupMenu();
	BOOL DestroyMenu();
	BOOL AppendMenu(UINT flags, UINT_PTR id = 0, LPCTSTR text = nullptr);
	UINT CheckMenuItem(UINT id, UINT check);
	UINT EnableMenuItem(UINT id, UINT enable);
	CMenu *GetSubMenu(int pos) const;
	static CMenu *FromHandle(HMENU h);
};

class CWnd : public CCmdTarget
{
public:
	HWND m_hWnd = nullptr;
	virtual ~CWnd() = default;
	HWND GetSafeHwnd() const { return this ? m_hWnd : nullptr; }
	operator HWND() const { return m_hWnd; }
	static CWnd *FromHandle(HWND h);
	BOOL IsWindowVisible() const;
	BOOL IsWindowEnabled() const;
	BOOL EnableWindow(BOOL enable = TRUE);
	BOOL ShowWindow(int cmd);
	void Invalidate(BOOL erase = TRUE);
	void InvalidateRect(LPCRECT rect, BOOL erase = TRUE);
	void UpdateWindow();
	void GetClientRect(LPRECT rect) const;
	void GetWindowRect(LPRECT rect) const;
	void SetWindowText(LPCTSTR text);
	int GetWindowText(LPTSTR buf, int max) const;
	void GetWindowText(CString &text) const;
	int GetWindowTextLength() const;
	CWnd *GetDlgItem(int id) const;
	CWnd *GetParent() const;
	CWnd *SetFocus();
	static CWnd *GetFocus();
	BOOL PostMessage(UINT msg, WPARAM wp = 0, LPARAM lp = 0);
	LRESULT SendMessage(UINT msg, WPARAM wp = 0, LPARAM lp = 0);
	BOOL SendNotifyMessage(UINT msg, WPARAM wp, LPARAM lp);
	void SendMessageToDescendants(UINT msg, WPARAM wp = 0, LPARAM lp = 0, BOOL deep = TRUE, BOOL onlyPerm = FALSE);
	BOOL GetWindowPlacement(WINDOWPLACEMENT *placement) const;
	BOOL SetWindowPlacement(const WINDOWPLACEMENT *placement);
	void SetRedraw(BOOL redraw = TRUE);
	void MoveWindow(int x, int y, int w, int h, BOOL repaint = TRUE);
	void MoveWindow(LPCRECT rect, BOOL repaint = TRUE);
	BOOL SetWindowPos(const CWnd *after, int x, int y, int cx, int cy, UINT flags);
	void ScreenToClient(LPPOINT p) const;
	void ClientToScreen(LPPOINT p) const;
	void ScreenToClient(LPRECT r) const;
	void ClientToScreen(LPRECT r) const;
	CDC *GetDC();
	int ReleaseDC(CDC *dc);
	void SetFont(CFont *font, BOOL redraw = TRUE);
	CFont *GetFont() const;
	int MessageBox(LPCTSTR text, LPCTSTR caption = nullptr, UINT type = MB_OK);
	UINT_PTR SetTimer(UINT_PTR id, UINT elapse, void *proc = nullptr);
	BOOL KillTimer(UINT_PTR id);
	CMenu *GetMenu() const;
	int GetDlgCtrlID() const;
	void CheckDlgButton(int id, UINT check);
	void CheckRadioButton(int firstID, int lastID, int checkID);
	int GetCheckedRadioButton(int firstID, int lastID);
	int GetDlgItemText(int id, LPTSTR buf, int max) const;
	int GetDlgItemText(int id, CString &text) const;
	static CWnd *GetActiveWindow();
	static CWnd *GetForegroundWindow();
	static CWnd *GetDesktopWindow();
	HICON SetIcon(HICON icon, BOOL bigIcon);
	HICON GetIcon(BOOL bigIcon) const;
	UINT IsDlgButtonChecked(int id) const;
	void SetDlgItemText(int id, LPCTSTR text);
	UINT GetDlgItemInt(int id, BOOL *ok = nullptr, BOOL isSigned = TRUE) const;
	void SetDlgItemInt(int id, UINT value, BOOL isSigned = TRUE);
	virtual BOOL PreTranslateMessage(MSG *msg) { return 0; }
	virtual BOOL OnCommand(WPARAM wp, LPARAM lp) { return 0; }
	virtual BOOL OnNotify(WPARAM wp, LPARAM lp, LRESULT *result) { return 0; }
	virtual LRESULT WindowProc(UINT msg, WPARAM wp, LPARAM lp) { return 0; }
	virtual void DoDataExchange(CDataExchange *dx) { }
	virtual BOOL PreCreateWindow(CREATESTRUCT &cs) { return 0; }
	virtual BOOL DestroyWindow() { return 0; }
	virtual void PostNcDestroy() { }
	virtual INT_PTR OnToolHitTest(CPoint point, TOOLINFO *ti) const { return 0; }
	virtual BOOL OnGesturePan(CPoint from, CPoint to) { return FALSE; }
	virtual BOOL Create(LPCTSTR className, LPCTSTR windowName, DWORD style, const RECT &rect, CWnd *parent, UINT id, CCreateContext *context = nullptr) { return FALSE; }
	BOOL CreateEx(DWORD exStyle, LPCTSTR className, LPCTSTR windowName, DWORD style, const RECT &rect, CWnd *parent, UINT id, LPVOID param = nullptr);
	BOOL SubclassDlgItem(UINT id, CWnd *parent);
	BOOL SubclassWindow(HWND hwnd);
	HWND UnsubclassWindow();
	DWORD GetStyle() const;
	DWORD GetExStyle() const;
	BOOL ModifyStyle(DWORD remove, DWORD add, UINT flags = 0);
	BOOL ModifyStyleEx(DWORD remove, DWORD add, UINT flags = 0);
	BOOL OpenClipboard();
	void DragAcceptFiles(BOOL accept = TRUE);
	CWnd *SetCapture();
	static CWnd *GetCapture();
	BOOL IsIconic() const;
	BOOL IsZoomed() const;
	BOOL IsChild(const CWnd *wnd) const;
	CWnd *GetTopLevelParent() const;
	class CFrameWnd *GetParentFrame() const;
	CWnd *GetWindow(UINT cmd) const;
	CWnd *GetNextDlgTabItem(CWnd *ctrl, BOOL previous = FALSE) const;
	CWnd *SetActiveWindow();
	BOOL SetForegroundWindow();
	void BringWindowToTop();
	void CenterWindow(CWnd *alternateOwner = nullptr);
	int SetScrollPos(int bar, int pos, BOOL redraw = TRUE);
	int GetScrollPos(int bar) const;
	void SetScrollRange(int bar, int minPos, int maxPos, BOOL redraw = TRUE);
	BOOL SetScrollInfo(int bar, struct SCROLLINFO *info, BOOL redraw = TRUE);
	BOOL GetScrollInfo(int bar, struct SCROLLINFO *info, UINT mask = 0xFFFF);
	void ShowScrollBar(UINT bar, BOOL show = TRUE);
	void ScrollWindow(int dx, int dy, LPCRECT rect = nullptr, LPCRECT clip = nullptr);
	BOOL RedrawWindow(LPCRECT update = nullptr, CRgn *updateRgn = nullptr, UINT flags = 0);
	void ValidateRect(LPCRECT rect);
	CDC *GetWindowDC();
	CMenu *GetSystemMenu(BOOL revert) const;
	BOOL SetMenu(CMenu *menu);
	void DrawMenuBar();
	LRESULT Default();
	// Default message handlers, as in MFC. Upstream calls these as
	// CWnd::OnXxx (or Base::OnXxx) to get default processing.
	afx_msg int OnCreate(LPCREATESTRUCT cs);
	afx_msg void OnDestroy();
	afx_msg void OnClose();
	afx_msg void OnSize(UINT type, int cx, int cy);
	afx_msg void OnMove(int x, int y);
	afx_msg void OnPaint();
	afx_msg BOOL OnEraseBkgnd(CDC *dc);
	afx_msg void OnSetFocus(CWnd *oldWnd);
	afx_msg void OnKillFocus(CWnd *newWnd);
	afx_msg void OnTimer(UINT_PTR id);
	afx_msg void OnHScroll(UINT code, UINT pos, class CScrollBar *scrollBar);
	afx_msg void OnVScroll(UINT code, UINT pos, class CScrollBar *scrollBar);
	afx_msg void OnMouseMove(UINT flags, CPoint point);
	afx_msg void OnLButtonDown(UINT flags, CPoint point);
	afx_msg void OnLButtonUp(UINT flags, CPoint point);
	afx_msg void OnLButtonDblClk(UINT flags, CPoint point);
	afx_msg void OnRButtonDown(UINT flags, CPoint point);
	afx_msg void OnRButtonUp(UINT flags, CPoint point);
	afx_msg void OnRButtonDblClk(UINT flags, CPoint point);
	afx_msg void OnMButtonDown(UINT flags, CPoint point);
	afx_msg void OnMButtonUp(UINT flags, CPoint point);
	afx_msg void OnMButtonDblClk(UINT flags, CPoint point);
	afx_msg void OnXButtonDown(UINT flags, UINT button, CPoint point);
	afx_msg void OnXButtonUp(UINT flags, UINT button, CPoint point);
	afx_msg BOOL OnMouseWheel(UINT flags, short delta, CPoint point);
	afx_msg void OnMouseHWheel(UINT flags, short delta, CPoint point);
	afx_msg void OnMouseHover(UINT flags, CPoint point);
	afx_msg void OnMouseLeave();
	afx_msg int OnMouseActivate(CWnd *topParent, UINT hitTest, UINT message);
	afx_msg void OnChar(UINT ch, UINT repCnt, UINT flags);
	afx_msg void OnKeyDown(UINT ch, UINT repCnt, UINT flags);
	afx_msg void OnKeyUp(UINT ch, UINT repCnt, UINT flags);
	afx_msg void OnSysKeyDown(UINT ch, UINT repCnt, UINT flags);
	afx_msg UINT OnGetDlgCode();
	afx_msg void OnShowWindow(BOOL show, UINT status);
	afx_msg void OnContextMenu(CWnd *wnd, CPoint pos);
	afx_msg void OnDropFiles(HDROP drop);
	afx_msg void OnGetMinMaxInfo(MINMAXINFO *mmi);
	afx_msg BOOL OnSetCursor(CWnd *wnd, UINT hitTest, UINT message);
	afx_msg void OnActivate(UINT state, CWnd *other, BOOL minimized);
	afx_msg void OnActivateApp(BOOL active, DWORD threadID);
	afx_msg void OnSysCommand(UINT id, LPARAM lParam);
	afx_msg void OnNcCalcSize(BOOL calcValidRects, NCCALCSIZE_PARAMS *params);
	afx_msg LRESULT OnNcHitTest(CPoint point);
	afx_msg void OnNcPaint();
	afx_msg BOOL OnNcActivate(BOOL active);
	afx_msg void OnNcMouseMove(UINT hitTest, CPoint point);
	afx_msg void OnNcLButtonDown(UINT hitTest, CPoint point);
	afx_msg void OnNcLButtonUp(UINT hitTest, CPoint point);
	afx_msg void OnNcLButtonDblClk(UINT hitTest, CPoint point);
	afx_msg void OnInitMenu(CMenu *menu);
	afx_msg void OnInitMenuPopup(CMenu *popup, UINT index, BOOL sysMenu);
	afx_msg void OnMenuSelect(UINT item, UINT flags, HMENU sysMenu);
	afx_msg void OnDrawItem(int id, LPDRAWITEMSTRUCT dis);
	afx_msg void OnMeasureItem(int id, LPMEASUREITEMSTRUCT mis);
	afx_msg BOOL OnDeviceChange(UINT eventType, DWORD_PTR data);
	afx_msg BOOL OnQueryEndSession();
	afx_msg void OnEndSession(BOOL ending);
	afx_msg HBRUSH OnCtlColor(CDC *dc, CWnd *wnd, UINT ctlColor);
	// Active Accessibility hooks (no-ops on Linux; Qt provides accessibility).
	void EnableActiveAccessibility() { }
	virtual HRESULT get_accName(VARIANT child, BSTR *name) { return 0; }
	virtual HRESULT get_accValue(VARIANT child, BSTR *value) { return 0; }
	virtual HRESULT get_accDescription(VARIANT child, BSTR *description) { return 0; }
	virtual HRESULT get_accRole(VARIANT child, VARIANT *role) { return 0; }
};

class CMFCDynamicLayout
{
public:
	struct MoveSettings { int m_nXRatio = 0; int m_nYRatio = 0; };
	struct SizeSettings { int m_nXRatio = 0; int m_nYRatio = 0; };
	BOOL AddItem(HWND hwnd, MoveSettings move, SizeSettings size);
};

class CStatic : public CWnd { public: void SetIcon(HICON); };
class CButton : public CWnd
{
public:
	BOOL Create(LPCTSTR caption, DWORD style, const RECT &rect, CWnd *parent, UINT id);
	int GetCheck() const;
	void SetCheck(int check);
	UINT GetState() const;
	void SetState(BOOL highlight);
	UINT GetButtonStyle() const;
	void SetButtonStyle(UINT style, BOOL redraw = TRUE);
	HICON SetIcon(HICON icon);
	HBITMAP SetBitmap(HBITMAP bitmap);
	virtual void DrawItem(LPDRAWITEMSTRUCT dis) { }
};
class CEdit : public CWnd
{
public:
	void SetSel(int start, int end, BOOL noScroll = FALSE);
	void GetSel(int &start, int &end) const;
	void ReplaceSel(LPCTSTR text, BOOL canUndo = FALSE);
	void SetLimitText(UINT max);
	UINT GetLimitText() const;
	void LimitText(int max = 0);
	BOOL SetReadOnly(BOOL ro = TRUE);
	BOOL Create(DWORD style, const RECT &rect, CWnd *parent, UINT id);
	int GetLineCount() const;
	int LineIndex(int line = -1) const;
	int LineFromChar(int index = -1) const;
	int LineLength(int line = -1) const;
	int GetLine(int index, LPTSTR buffer, int maxLength) const;
	void LineScroll(int lines, int chars = 0);
	int GetFirstVisibleLine() const;
	BOOL Undo();
	BOOL CanUndo() const;
	void Clear();
	void Copy();
	void Cut();
	void Paste();
	void SetMargins(UINT left, UINT right);
	void SetPasswordChar(TCHAR ch);
	BOOL GetModify() const;
	void SetModify(BOOL modified = TRUE);
};
class CComboBox : public CWnd
{
public:
	int InitStorage(int items, UINT bytes);
	void Clear();
	void Copy();
	void Cut();
	void Paste();
	BOOL LimitText(int max);
	DWORD GetEditSel() const;
	BOOL SetEditSel(int start, int end);
	int GetLBTextLen(int index) const;
	int GetLBText(int index, LPTSTR text) const;
	int FindString(int start, LPCTSTR text) const;
	int SelectString(int start, LPCTSTR text);
	void *GetItemDataPtr(int index) const;
	int SetItemDataPtr(int index, void *data);
	void ShowDropDown(BOOL show = TRUE);
	BOOL GetDroppedState() const;
	int SetDroppedWidth(UINT width);
	int GetDroppedWidth() const;
	int GetTopIndex() const;
	int SetTopIndex(int index);
	int SetItemHeight(int index, UINT height);
	int GetItemHeight(int index) const;
	int GetCount() const;
	int GetCurSel() const;
	int SetCurSel(int sel);
	int AddString(LPCTSTR text);
	int InsertString(int index, LPCTSTR text);
	int DeleteString(UINT index);
	void ResetContent();
	DWORD_PTR GetItemData(int index) const;
	int SetItemData(int index, DWORD_PTR data);
	void GetLBText(int index, CString &text) const;
	int FindStringExact(int start, LPCTSTR text) const;
};
class CComboBoxEx : public CComboBox { };
class CListBox : public CWnd
{
public:
	int GetCount() const;
	int GetCurSel() const;
	int SetCurSel(int sel);
	int AddString(LPCTSTR text);
	int InsertString(int index, LPCTSTR text);
	int DeleteString(UINT index);
	void ResetContent();
	DWORD_PTR GetItemData(int index) const;
	int SetItemData(int index, DWORD_PTR data);
	int GetText(int index, LPTSTR buf) const;
	void GetText(int index, CString &text) const;
	int GetSel(int index) const;
	int SetSel(int index, BOOL select = TRUE);
	int GetSelCount() const;
	int GetTopIndex() const;
	int SetTopIndex(int index);
	void *GetItemDataPtr(int index) const;
	int SetItemDataPtr(int index, void *data);
	int GetItemRect(int index, LPRECT rect) const;
	int GetItemHeight(int index) const;
	int SetItemHeight(int index, UINT height);
	int FindString(int start, LPCTSTR text) const;
	int FindStringExact(int start, LPCTSTR text) const;
	int GetTextLen(int index) const;
	int GetCaretIndex() const;
	int SetCaretIndex(int index, BOOL scroll = TRUE);
	int GetSelItems(int maxItems, LPINT items) const;
	int SelItemRange(BOOL select, int first, int last);
	UINT ItemFromPoint(CPoint pt, BOOL &outside) const;
};
class CCheckListBox : public CListBox
{
public:
	void SetCheckStyle(UINT style);
	void SetCheck(int index, int check);
	int GetCheck(int index);
};
class CScrollBar : public CWnd
{
public:
	int GetScrollPos() const;
	int SetScrollPos(int pos, BOOL redraw = TRUE);
	void SetScrollRange(int minPos, int maxPos, BOOL redraw = TRUE);
};
class CSliderCtrl : public CWnd
{
public:
	int GetPos() const;
	void SetPos(int pos);
	void SetRange(int minPos, int maxPos, BOOL redraw = FALSE);
	int GetRangeMin() const;
	int GetRangeMax() const;
	void SetTicFreq(int freq);
	void SetPageSize(int size);
	void SetLineSize(int size);
};
class CSpinButtonCtrl : public CWnd
{
public:
	int GetPos32(BOOL *error = nullptr) const;
	int SetPos32(int pos);
	int GetPos() const;
	int SetPos(int pos);
	void SetRange(short lower, short upper);
	void SetRange32(int lower, int upper);
	void GetRange32(int &lower, int &upper) const;
	CWnd *SetBuddy(CWnd *buddy);
	CWnd *GetBuddy() const;
	UINT SetBase(int base);
	BOOL SetAccel(int count, struct UDACCEL *accel);
};
class CProgressCtrl : public CWnd { public: void SetRange32(int, int); int SetPos(int); };
// List view column formats and styles.
#define LVCFMT_LEFT 0x0000
#define LVCFMT_RIGHT 0x0001
#define LVCFMT_CENTER 0x0002
#define LVSCW_AUTOSIZE (-1)
#define LVSCW_AUTOSIZE_USEHEADER (-2)
#define LVNI_ALL 0x0000
#define LVNI_FOCUSED 0x0001
#define LVNI_SELECTED 0x0002
#define LVIS_FOCUSED 0x0001
#define LVIS_SELECTED 0x0002
#define LVIS_STATEIMAGEMASK 0xF000
#define LVS_EX_GRIDLINES 0x00000001
#define LVS_EX_CHECKBOXES 0x00000004
#define LVS_EX_FULLROWSELECT 0x00000020
#define LVS_EX_DOUBLEBUFFER 0x00010000
#define LVIF_TEXT 0x0001
#define LVIF_IMAGE 0x0002
#define LVIF_PARAM 0x0004
#define LVIF_STATE 0x0008
#define LVIF_GROUPID 0x0100
struct LVITEM
{
	UINT mask; int iItem; int iSubItem; UINT state; UINT stateMask;
	LPTSTR pszText; int cchTextMax; int iImage; LPARAM lParam; int iIndent; int iGroupId;
	UINT cColumns; UINT *puColumns; int *piColFmt; int iGroup;
};
struct LVGROUP;
// Returns 1 if group view was enabled, 0 if unchanged, -1 on failure.
LRESULT ListView_EnableGroupView(HWND hwnd, BOOL enable);

class CListCtrl : public CWnd
{
public:
	int InsertColumn(int col, LPCTSTR heading, int format = LVCFMT_LEFT, int width = -1, int subItem = -1);
	BOOL DeleteColumn(int col);
	int GetColumnWidth(int col) const;
	BOOL SetColumnWidth(int col, int width);
	int GetItemCount() const;
	int InsertItem(int item, LPCTSTR text);
	int InsertItem(int item, LPCTSTR text, int image);
	int InsertItem(const LVITEM *item);
	int InsertItem(UINT mask, int item, LPCTSTR text, UINT state, UINT stateMask, int image, LPARAM lParam);
	BOOL SetItem(const LVITEM *item);
	BOOL GetItem(LVITEM *item) const;
	BOOL DeleteItem(int item);
	BOOL DeleteAllItems();
	BOOL SetItemText(int item, int subItem, LPCTSTR text);
	CString GetItemText(int item, int subItem) const;
	BOOL SetItemData(int item, DWORD_PTR data);
	DWORD_PTR GetItemData(int item) const;
	BOOL SetItemState(int item, UINT state, UINT mask);
	UINT GetItemState(int item, UINT mask) const;
	BOOL SetCheck(int item, BOOL check = TRUE);
	BOOL GetCheck(int item) const;
	UINT GetSelectedCount() const;
	int GetSelectionMark() const;
	int SetSelectionMark(int item);
	int GetNextItem(int item, int flags) const;
	POSITION GetFirstSelectedItemPosition() const;
	int GetNextSelectedItem(POSITION &pos) const;
	BOOL EnsureVisible(int item, BOOL partialOK);
	DWORD GetExtendedStyle() const;
	DWORD SetExtendedStyle(DWORD style);
	CImageList *SetImageList(CImageList *list, int type);
	int GetTopIndex() const;
	int GetCountPerPage() const;
	BOOL GetItemRect(int item, LPRECT rect, UINT code) const;
	int HitTest(CPoint pt, UINT *flags = nullptr) const;
	LRESULT EnableGroupView(BOOL enable);
	int InsertGroup(int index, LVGROUP *group);
	void RemoveAllGroups();
	BOOL SortItems(int (*compare)(LPARAM, LPARAM, LPARAM), DWORD_PTR data);
};
class CMFCListCtrl : public CListCtrl
{
public:
	virtual int OnCompareItems(LPARAM item1, LPARAM item2, int column) { return 0; }
	virtual COLORREF OnGetCellTextColor(int row, int column) { return 0; }
	virtual COLORREF OnGetCellBkColor(int row, int column) { return 0; }
	virtual void Sort(int column, BOOL ascending = TRUE, BOOL add = FALSE) { }
};
class CTreeCtrl : public CWnd { };
class CTabCtrl : public CWnd { };
class CToolTipCtrl : public CWnd
{
public:
	BOOL AddTool(CWnd *wnd, LPCTSTR text = nullptr, LPCRECT rect = nullptr, UINT_PTR id = 0);
	void Activate(BOOL activate);
	void RelayEvent(LPMSG msg);
};
class CToolBarCtrl : public CWnd { };
class CReBarCtrl : public CWnd { };
class CStatusBarCtrl : public CWnd { };
class CControlBar : public CWnd
{
public:
	DWORD GetBarStyle();
	void SetBarStyle(DWORD style);
	virtual CSize CalcFixedLayout(BOOL stretch, BOOL horz) { return {}; }
	virtual CSize CalcDynamicLayout(int length, DWORD mode) { return {}; }
	virtual void DelayShow(BOOL show) { }
};
class CToolBar : public CControlBar { };
class CStatusBar : public CControlBar { };
class CDialogBar : public CControlBar { };
class CReBar : public CControlBar { };
class CSplitterWnd : public CWnd
{
public:
	virtual BOOL CreateStatic(CWnd *parent, int rows, int cols, DWORD style = 0, UINT id = 0) { return FALSE; }
	virtual BOOL CreateView(int row, int col, CRuntimeClass *viewClass, SIZE initialSize, CCreateContext *context) { return FALSE; }
	virtual void DeleteView(int row, int col) { }
	virtual void RecalcLayout() { }
	CWnd *GetPane(int row, int col) const;
	void SetRowInfo(int row, int ideal, int min);
	void GetRowInfo(int row, int &cur, int &min) const;
	void SetColumnInfo(int col, int ideal, int min);
	void GetColumnInfo(int col, int &cur, int &min) const;
	int IdFromRowCol(int row, int col) const;
};

class CDialog : public CWnd
{
public:
	CDialog() = default;
	explicit CDialog(UINT id, CWnd *parent = nullptr);
	virtual INT_PTR DoModal() { return 0; }
	virtual BOOL OnInitDialog() { return 0; }
	virtual void OnOK() { }
	virtual void OnCancel() { }
	BOOL Create(UINT id, CWnd *parent = nullptr);
	void EndDialog(int result);
	void GotoDlgCtrl(CWnd *ctrl);
	void NextDlgCtrl() const;
	void PrevDlgCtrl() const;
	void SetDefID(UINT id);
	DWORD GetDefID() const;
	void MapDialogRect(LPRECT rect) const;
	CMFCDynamicLayout *GetDynamicLayout();
	virtual void ResizeDynamicLayout() { }
};
class CDialogEx : public CDialog
{
public:
	using CDialog::CDialog;
};
class CPropertyPage : public CDialog
{
public:
	CPropertyPage() = default;
	explicit CPropertyPage(UINT id, UINT caption = 0);
	virtual BOOL OnSetActive() { return 0; }
	virtual BOOL OnKillActive() { return 0; }
	virtual BOOL OnApply() { return 0; }
	void SetModified(BOOL changed = TRUE);
};
class CPropertySheet : public CWnd
{
public:
	CPropertySheet() = default;
	explicit CPropertySheet(UINT captionID, CWnd *parent = nullptr, UINT selectPage = 0);
	explicit CPropertySheet(LPCTSTR caption, CWnd *parent = nullptr, UINT selectPage = 0);
	virtual INT_PTR DoModal() { return IDCANCEL; }
	virtual BOOL OnInitDialog() { return TRUE; }
	void AddPage(CPropertyPage *page);
	void RemovePage(CPropertyPage *page);
	void RemovePage(int page);
	int GetPageCount() const;
	CPropertyPage *GetPage(int page) const;
	CPropertyPage *GetActivePage() const;
	int GetActiveIndex() const;
	BOOL SetActivePage(int page);
	BOOL SetActivePage(CPropertyPage *page);
	void SetTitle(LPCTSTR text, UINT style = 0);
	void EndDialog(int result);
	struct { DWORD dwFlags = 0; } m_psh;
};
class CFileDialog : public CDialog { };

class CFrameWnd : public CWnd
{
public:
	virtual void OnUpdateFrameTitle(BOOL addToTitle) { }
	virtual void RecalcLayout(BOOL notify = TRUE) { }
	void SetMessageText(LPCTSTR text);
	virtual BOOL OnCreateClient(LPCREATESTRUCT cs, CCreateContext *context) { return TRUE; }
	virtual void ActivateFrame(int cmdShow = -1) { }
	virtual BOOL LoadFrame(UINT resourceID, DWORD style = 0, CWnd *parent = nullptr, CCreateContext *context = nullptr) { return FALSE; }
	virtual void OnUpdateFrameMenu(HMENU menuAlt) { }
	virtual class CDocument *GetActiveDocument() { return nullptr; }
	virtual class CView *GetActiveView() const { return nullptr; }
	virtual CFrameWnd *GetActiveFrame() { return nullptr; }
};
class CMDIFrameWnd : public CFrameWnd
{
public:
	HWND m_hWndMDIClient = nullptr;
	class CMDIChildWnd *MDIGetActive(BOOL *maximized = nullptr) const;
	void MDIActivate(CWnd *child);
	void MDINext();
	void MDIPrev();
	void MDICascade();
	void MDITile(int type);
	void MDIIconArrange();
};
class CMDIChildWnd : public CFrameWnd
{
public:
	void MDIActivate();
	void MDIDestroy();
	void MDIMaximize();
	void MDIRestore();
	CMDIFrameWnd *GetMDIFrame();
};
class CMDIFrameWndEx : public CMDIFrameWnd { };
class CMDIChildWndEx : public CMDIChildWnd { };

class CView : public CWnd
{
public:
	class CDocument *GetDocument() const;
	virtual void OnDraw(CDC *dc) { }
	virtual void OnInitialUpdate() { }
	virtual void OnUpdate(CView *sender, LPARAM hint, CObject *hintObj) { }
	virtual void OnActivateView(BOOL activate, CView *activateView, CView *deactiveView) { }
	virtual void OnPrepareDC(CDC *dc, CPrintInfo *info = nullptr) { }
};
#define MM_TEXT 1
class CScrollView : public CView
{
public:
	static const SIZE sizeDefault;
	virtual BOOL OnScroll(UINT scrollCode, UINT pos, BOOL doScroll = TRUE) { return FALSE; }
	virtual BOOL OnScrollBy(CSize sizeScroll, BOOL doScroll = TRUE) { return FALSE; }
	void SetScrollSizes(int mapMode, SIZE total, const SIZE &page = sizeDefault, const SIZE &line = sizeDefault);
	CPoint GetScrollPosition() const;
	void ScrollToPosition(POINT pt);
	CSize GetTotalSize() const;
	void GetDeviceScrollSizes(int &mapMode, SIZE &total, SIZE &page, SIZE &line) const;
	int GetScrollPos(int bar) const;
	int SetScrollPos(int bar, int pos, BOOL redraw = TRUE);
};
class CFormView : public CScrollView { };

// ---- documents -----------------------------------------------------------
class CDocTemplate : public CCmdTarget
{
public:
	enum DocStringIndex { windowTitle, docName, fileNewName, filterName, filterExt, regFileTypeId, regFileTypeName };
	CDocTemplate(UINT resourceID, CRuntimeClass *docClass, CRuntimeClass *frameClass, CRuntimeClass *viewClass)
		: m_nIDResource(resourceID), m_pDocClass(docClass), m_pFrameClass(frameClass), m_pViewClass(viewClass) { }
	virtual CDocument *OpenDocumentFile(LPCTSTR pathName, BOOL addToMRU, BOOL makeVisible) = 0;
	virtual CDocument *OpenDocumentFile(LPCTSTR pathName, BOOL makeVisible = TRUE) { return OpenDocumentFile(pathName, TRUE, makeVisible); }
	virtual void AddDocument(CDocument *doc) { }
	virtual void RemoveDocument(CDocument *doc) { }
	virtual POSITION GetFirstDocPosition() const = 0;
	virtual CDocument *GetNextDoc(POSITION &pos) const = 0;
	virtual CDocument *CreateNewDocument() { return nullptr; }
	virtual CFrameWnd *CreateNewFrame(CDocument *doc, CFrameWnd *other) { return nullptr; }
	virtual void InitialUpdateFrame(CFrameWnd *frame, CDocument *doc, BOOL makeVisible = TRUE) { }
	virtual BOOL SaveAllModified() { return TRUE; }
	virtual void CloseAllDocuments(BOOL endSession) { }
	virtual BOOL GetDocString(CString &string, DocStringIndex index) const { return FALSE; }
	virtual void SetDefaultTitle(CDocument *doc) { }
protected:
	UINT m_nIDResource;
	CRuntimeClass *m_pDocClass;
	CRuntimeClass *m_pFrameClass;
	CRuntimeClass *m_pViewClass;
};

class CMultiDocTemplate : public CDocTemplate
{
public:
	CMultiDocTemplate(UINT resourceID, CRuntimeClass *docClass, CRuntimeClass *frameClass, CRuntimeClass *viewClass)
		: CDocTemplate(resourceID, docClass, frameClass, viewClass) { }
	CDocument *OpenDocumentFile(LPCTSTR pathName, BOOL addToMRU, BOOL makeVisible) override { return nullptr; }
	using CDocTemplate::OpenDocumentFile;
	// Not used by upstream, which keeps its own document set in CModDocTemplate.
	POSITION GetFirstDocPosition() const override { return nullptr; }
	CDocument *GetNextDoc(POSITION &pos) const override { pos = nullptr; return nullptr; }
	HMENU m_hMenuShared = nullptr;
	HACCEL m_hAccelTable = nullptr;
};

class CDocManager : public CObject
{
public:
	virtual ~CDocManager() = default;
	virtual void AddDocTemplate(CDocTemplate *tmpl) { }
	virtual POSITION GetFirstDocTemplatePosition() const { return nullptr; }
	virtual CDocTemplate *GetNextDocTemplate(POSITION &pos) const { pos = nullptr; return nullptr; }
	virtual CDocument *OpenDocumentFile(LPCTSTR fileName) { return OpenDocumentFile(fileName, TRUE); }
	virtual CDocument *OpenDocumentFile(LPCTSTR fileName, BOOL addToMRU) { return nullptr; }
	virtual BOOL SaveAllModified() { return TRUE; }
	virtual void CloseAllDocuments(BOOL endSession) { }
	virtual int GetOpenDocumentCount() { return 0; }
	virtual BOOL OnDDECommand(LPTSTR command) { return FALSE; }
	virtual BOOL DoPromptFileName(CString &fileName, UINT titleID, DWORD flags, BOOL openFileDialog, CDocTemplate *tmpl) { return FALSE; }
};

class CDocument : public CCmdTarget
{
protected:
	CString m_strTitle;
	CString m_strPathName;
	// BOOL in MFC. Upstream updates it with InterlockedExchange through a
	// long *, and asserts sizeof(long) == sizeof(m_bModified). long is
	// 8 bytes on Linux, so the member is a long here.
	long m_bModified = FALSE;
	BOOL m_bAutoDelete = TRUE;
	BOOL m_bEmbedded = FALSE;
public:
	CDocument() = default;
	virtual ~CDocument() = default;
	const CString &GetTitle() const { return m_strTitle; }
	virtual void SetTitle(LPCTSTR title) { m_strTitle = title; }
	const CString &GetPathName() const { return m_strPathName; }
	virtual void SetPathName(LPCTSTR path, BOOL addToMRU = TRUE) { (void)addToMRU; m_strPathName = path; }
	virtual BOOL IsModified() { return m_bModified; }
	virtual void SetModifiedFlag(BOOL modified = TRUE) { m_bModified = modified; }
	virtual BOOL OnNewDocument() { m_bModified = FALSE; m_strPathName.Empty(); return TRUE; }
	virtual BOOL OnOpenDocument(LPCTSTR path) { (void)path; return TRUE; }
	virtual BOOL OnSaveDocument(LPCTSTR path) { (void)path; m_bModified = FALSE; return TRUE; }
	virtual void OnCloseDocument() { }
	virtual BOOL SaveModified() { return TRUE; }
	virtual BOOL DoSave(LPCTSTR path, BOOL replace = TRUE) { return 0; }
	virtual BOOL DoFileSave() { return 0; }
	virtual void DeleteContents() { }
	virtual BOOL CanCloseFrame(CFrameWnd *frame) { return TRUE; }
	virtual void PreCloseFrame(CFrameWnd *frame) { }
	virtual void OnChangedViewList() { }
	virtual void SetDocumentTitle() { }
	afx_msg void OnFileClose();
	afx_msg void OnFileSave();
	afx_msg void OnFileSaveAs();
	void UpdateAllViews(CView *sender, LPARAM hint = 0, CObject *hintObj = nullptr);
	CDocTemplate *GetDocTemplate() const;
	POSITION GetFirstViewPosition() const;
	CView *GetNextView(POSITION &pos) const;
};

class CDataRecoveryHandler : public CObject { };

// ---- application ---------------------------------------------------------
#define AFX_RESTART_MANAGER_SUPPORT_RESTART 0x01
#define AFX_RESTART_MANAGER_REOPEN_PREVIOUS_FILES 0x08

class CWinThread : public CCmdTarget
{
public:
	CWnd *m_pMainWnd = nullptr;
	virtual CWnd *GetMainWnd() { return m_pMainWnd; }
	virtual BOOL InitInstance() { return 0; }
	virtual int ExitInstance() { return 0; }
	virtual int Run() { return 0; }
	virtual BOOL OnIdle(LONG count) { return 0; }
	virtual BOOL PreTranslateMessage(MSG *msg) { return 0; }
	virtual LRESULT ProcessWndProcException(CException *e, const MSG *msg) { return 0; }
};

class CWinApp : public CWinThread
{
public:
	LPCTSTR m_pszAppName = nullptr;
	DWORD m_dwRestartManagerSupportFlags = 0;
	LPCTSTR m_pszExeName = nullptr;
	HINSTANCE m_hInstance = nullptr;
	virtual CDocument *OpenDocumentFile(LPCTSTR path) { return nullptr; }
	virtual CDocument *OpenDocumentFile(LPCTSTR path, BOOL addToMRU) { return nullptr; }
	virtual void AddToRecentFileList(LPCTSTR path) { }
	virtual CDataRecoveryHandler *GetDataRecoveryHandler() { return nullptr; }
	virtual BOOL SaveAllModified() { return 0; }
	virtual void WinHelp(DWORD_PTR data, UINT cmd = 0) { }
	void AddDocTemplate(CDocTemplate *tmpl);
};

CWinApp *AfxGetApp();
CWnd *AfxGetMainWnd();
int AfxMessageBox(LPCTSTR text, UINT type = MB_OK, UINT help = 0);

// ---- dialog data exchange ------------------------------------------------
void DDX_Control(CDataExchange *dx, int id, CWnd &control);
void DDX_Text(CDataExchange *dx, int id, CString &value);
void DDX_Text(CDataExchange *dx, int id, int &value);
void DDX_Text(CDataExchange *dx, int id, UINT &value);
void DDX_Text(CDataExchange *dx, int id, long &value);
void DDX_Text(CDataExchange *dx, int id, DWORD &value);
void DDX_Text(CDataExchange *dx, int id, float &value);
void DDX_Text(CDataExchange *dx, int id, double &value);
void DDX_Check(CDataExchange *dx, int id, int &value);
void DDX_Radio(CDataExchange *dx, int id, int &value);
void DDX_CBIndex(CDataExchange *dx, int id, int &index);
void DDX_CBString(CDataExchange *dx, int id, CString &value);
void DDX_LBIndex(CDataExchange *dx, int id, int &index);
void DDX_Slider(CDataExchange *dx, int id, int &value);
void DDV_MaxChars(CDataExchange *dx, const CString &value, int maxChars);
void DDV_MinMaxInt(CDataExchange *dx, int value, int minVal, int maxVal);
void DDV_MinMaxUInt(CDataExchange *dx, UINT value, UINT minVal, UINT maxVal);
