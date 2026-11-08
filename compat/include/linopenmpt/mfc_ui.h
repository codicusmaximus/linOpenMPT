// linOpenMPT compatibility layer: declarations of MFC UI classes.
//
// Upstream's shared headers (Mptrack.h, Mainfrm.h, Moddoc.h, ...) mention MFC
// window classes in member declarations and signatures. These skeletons let
// those headers compile unchanged. They have no behaviour: most methods are
// declared but not defined, so any UI call that reaches a Linux binary shows
// up as a link error, which is exactly the list of things the Qt UI has to
// provide. Simple value types (CRect, CPoint, CSize) and the document base
// class are implemented, because non-UI logic uses them directly.

#pragma once

#include "afx.h"

// ---- window messages and constants used in shared headers ----------------
#define WM_NULL 0x0000
#define WM_USER 0x0400
#define WM_APP 0x8000
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
#define IDOK 1
#define IDCANCEL 2
#define IDABORT 3
#define IDRETRY 4
#define IDIGNORE 5
#define IDYES 6
#define IDNO 7

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
#define DECLARE_MESSAGE_MAP()
#define IMPLEMENT_DYNAMIC(c, b)
#define IMPLEMENT_DYNCREATE(c, b)
#define IMPLEMENT_SERIAL(c, b, s)

// ---- command routing -----------------------------------------------------
class CCmdTarget : public CObject
{
};

class CCmdUI
{
public:
	UINT m_nID = 0;
	virtual void Enable(BOOL on = TRUE);
	virtual void SetCheck(int check = 1);
	virtual void SetRadio(BOOL on = TRUE);
	virtual void SetText(LPCTSTR text);
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
	UINT IsDlgButtonChecked(int id) const;
	void SetDlgItemText(int id, LPCTSTR text);
	UINT GetDlgItemInt(int id, BOOL *ok = nullptr, BOOL isSigned = TRUE) const;
	void SetDlgItemInt(int id, UINT value, BOOL isSigned = TRUE);
	virtual BOOL PreTranslateMessage(MSG *msg);
	virtual BOOL OnCommand(WPARAM wp, LPARAM lp);
	virtual BOOL OnNotify(WPARAM wp, LPARAM lp, LRESULT *result);
	virtual LRESULT WindowProc(UINT msg, WPARAM wp, LPARAM lp);
	virtual void DoDataExchange(CDataExchange *dx);
	virtual BOOL PreCreateWindow(CREATESTRUCT &cs);
	virtual BOOL DestroyWindow();
	virtual void PostNcDestroy();
	virtual INT_PTR OnToolHitTest(CPoint point, TOOLINFO *ti) const;
	// Active Accessibility hooks (no-ops on Linux; Qt provides accessibility).
	void EnableActiveAccessibility() { }
	virtual HRESULT get_accName(VARIANT child, BSTR *name);
	virtual HRESULT get_accValue(VARIANT child, BSTR *value);
	virtual HRESULT get_accDescription(VARIANT child, BSTR *description);
	virtual HRESULT get_accRole(VARIANT child, VARIANT *role);
};

class CMFCDynamicLayout
{
public:
	struct MoveSettings { int m_nXRatio = 0; int m_nYRatio = 0; };
	struct SizeSettings { int m_nXRatio = 0; int m_nYRatio = 0; };
	BOOL AddItem(HWND hwnd, MoveSettings move, SizeSettings size);
};

class CStatic : public CWnd { public: void SetIcon(HICON); };
class CButton : public CWnd { public: int GetCheck() const; void SetCheck(int); };
class CEdit : public CWnd
{
public:
	void SetSel(int start, int end, BOOL noScroll = FALSE);
	void GetSel(int &start, int &end) const;
	void ReplaceSel(LPCTSTR text, BOOL canUndo = FALSE);
	void SetLimitText(UINT max);
	BOOL SetReadOnly(BOOL ro = TRUE);
};
class CComboBox : public CWnd
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
	void GetLBText(int index, CString &text) const;
	int FindStringExact(int start, LPCTSTR text) const;
	void SetRedraw(BOOL redraw = TRUE);
};
class CListBox : public CWnd
{
public:
	int GetCount() const;
	int GetCurSel() const;
	int SetCurSel(int sel);
	int AddString(LPCTSTR text);
	void ResetContent();
	DWORD_PTR GetItemData(int index) const;
	int SetItemData(int index, DWORD_PTR data);
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
	void SetRange32(int lower, int upper);
	void SetBuddy(CWnd *buddy);
};
class CProgressCtrl : public CWnd { public: void SetRange32(int, int); int SetPos(int); };
class CListCtrl : public CWnd { };
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
	virtual CSize CalcFixedLayout(BOOL stretch, BOOL horz);
	virtual CSize CalcDynamicLayout(int length, DWORD mode);
	virtual void DelayShow(BOOL show);
};
class CToolBar : public CControlBar { };
class CStatusBar : public CControlBar { };
class CDialogBar : public CControlBar { };
class CReBar : public CControlBar { };
class CSplitterWnd : public CWnd { };

class CDialog : public CWnd
{
public:
	CDialog() = default;
	explicit CDialog(UINT id, CWnd *parent = nullptr);
	virtual INT_PTR DoModal();
	virtual BOOL OnInitDialog();
	virtual void OnOK();
	virtual void OnCancel();
	BOOL Create(UINT id, CWnd *parent = nullptr);
	void EndDialog(int result);
	CMFCDynamicLayout *GetDynamicLayout();
	virtual void ResizeDynamicLayout();
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
	virtual BOOL OnSetActive();
	virtual BOOL OnKillActive();
	virtual BOOL OnApply();
	void SetModified(BOOL changed = TRUE);
};
class CPropertySheet : public CWnd { };
class CFileDialog : public CDialog { };

class CFrameWnd : public CWnd
{
public:
	virtual void OnUpdateFrameTitle(BOOL addToTitle);
	virtual void RecalcLayout(BOOL notify = TRUE);
	void SetMessageText(LPCTSTR text);
	virtual class CDocument *GetActiveDocument();
	virtual class CView *GetActiveView() const;
	virtual CFrameWnd *GetActiveFrame();
};
class CMDIFrameWnd : public CFrameWnd
{
public:
	class CMDIChildWnd *MDIGetActive(BOOL *maximized = nullptr) const;
};
class CMDIChildWnd : public CFrameWnd { };
class CMDIFrameWndEx : public CMDIFrameWnd { };
class CMDIChildWndEx : public CMDIChildWnd { };

class CView : public CWnd
{
public:
	class CDocument *GetDocument() const;
	virtual void OnDraw(CDC *dc);
	virtual void OnInitialUpdate();
	virtual void OnUpdate(CView *sender, LPARAM hint, CObject *hintObj);
	virtual void OnActivateView(BOOL activate, CView *activateView, CView *deactiveView);
	CFrameWnd *GetParentFrame() const;
};
class CScrollView : public CView { };
class CFormView : public CScrollView { };

// ---- documents -----------------------------------------------------------
class CDocTemplate : public CCmdTarget { };
class CMultiDocTemplate : public CDocTemplate { };

class CDocument : public CCmdTarget
{
protected:
	CString m_strTitle;
	CString m_strPathName;
	BOOL m_bModified = FALSE;
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
	virtual void OnCloseDocument();
	virtual BOOL SaveModified();
	virtual BOOL DoSave(LPCTSTR path, BOOL replace = TRUE);
	virtual BOOL DoFileSave();
	virtual void DeleteContents() { }
	virtual BOOL CanCloseFrame(CFrameWnd *frame);
	virtual void PreCloseFrame(CFrameWnd *frame);
	virtual void OnChangedViewList();
	virtual void SetDocumentTitle() { }
	void UpdateAllViews(CView *sender, LPARAM hint = 0, CObject *hintObj = nullptr);
	CDocTemplate *GetDocTemplate() const;
	struct POSITION__;
	using POSITION = POSITION__ *;
	POSITION GetFirstViewPosition() const;
	CView *GetNextView(POSITION &pos) const;
};

class CDataRecoveryHandler : public CObject { };

// ---- application ---------------------------------------------------------
class CWinThread : public CCmdTarget
{
public:
	CWnd *m_pMainWnd = nullptr;
	virtual BOOL InitInstance();
	virtual int ExitInstance();
	virtual int Run();
	virtual BOOL OnIdle(LONG count);
	virtual BOOL PreTranslateMessage(MSG *msg);
	virtual LRESULT ProcessWndProcException(CException *e, const MSG *msg);
};

class CWinApp : public CWinThread
{
public:
	LPCTSTR m_pszAppName = nullptr;
	LPCTSTR m_pszExeName = nullptr;
	HINSTANCE m_hInstance = nullptr;
	virtual CDocument *OpenDocumentFile(LPCTSTR path);
	virtual CDocument *OpenDocumentFile(LPCTSTR path, BOOL addToMRU);
	virtual void AddToRecentFileList(LPCTSTR path);
	virtual CDataRecoveryHandler *GetDataRecoveryHandler();
	virtual BOOL SaveAllModified();
	virtual void WinHelp(DWORD_PTR data, UINT cmd = 0);
	void AddDocTemplate(CDocTemplate *tmpl);
};

CWinApp *AfxGetApp();
CWnd *AfxGetMainWnd();
int AfxMessageBox(LPCTSTR text, UINT type = MB_OK, UINT help = 0);
