// linOpenMPT MFC layer: CWnd, written against the Win32 API as MFC is.
//
// Every window an MFC object creates is subclassed with AfxWndProc during
// creation (from a CBT hook, as in MFC), so its messages go to the object's
// WindowProc, then through the message map (OnWndMsg) or on to the original
// window procedure (DefWindowProc). Handle maps tie HWNDs to their objects.
#include "MfcInternal.h"

#include <algorithm>
#include <cstdio>
#include <cwchar>
#include <unordered_map>
#include <vector>

namespace linopenmpt::mfc
{

ThreadState &State()
{
	static ThreadState *state = new ThreadState; // never destroyed: used by static CWnds
	return *state;
}

namespace
{

std::unordered_map<HWND, CWnd *> &PermanentMap()
{
	static auto *map = new std::unordered_map<HWND, CWnd *>;
	return *map;
}

std::unordered_map<HWND, std::unique_ptr<CWnd>> &TemporaryMap()
{
	static auto *map = new std::unordered_map<HWND, std::unique_ptr<CWnd>>;
	return *map;
}

// Attaches the CWnd being created to its new window and subclasses it.
LRESULT CALLBACK CbtFilterHook(int code, WPARAM wp, LPARAM lp)
{
	ThreadState &state = State();
	if(code != HCBT_CREATEWND || state.wndInit == nullptr)
		return CallNextHookEx(state.cbtHook, code, wp, lp);
	CWnd *wnd = state.wndInit;
	state.wndInit = nullptr;
	const HWND hwnd = reinterpret_cast<HWND>(wp);
	wnd->Attach(hwnd);
	wnd->PreSubclassWindow();
	const auto old = reinterpret_cast<WNDPROC>(::SetWindowLongPtr(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&AfxWndProc)));
	if(old != &AfxWndProc)
		*wnd->GetSuperWndProcAddr() = old;
	return CallNextHookEx(state.cbtHook, code, wp, lp);
}

void HookWindowCreate(CWnd *wnd)
{
	ThreadState &state = State();
	if(state.cbtHook == nullptr)
		state.cbtHook = ::SetWindowsHookEx(WH_CBT, &CbtFilterHook, nullptr, ::GetCurrentThreadId());
	state.wndInit = wnd;
}

bool UnhookWindowCreate()
{
	ThreadState &state = State();
	if(state.cbtHook != nullptr)
	{
		::UnhookWindowsHookEx(state.cbtHook);
		state.cbtHook = nullptr;
	}
	if(state.wndInit != nullptr)
	{
		state.wndInit = nullptr;
		return false; // the window was never created
	}
	return true;
}

CPoint PointFromLParam(LPARAM lp)
{
	return CPoint(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
}

// The handler stored in a message map entry, as its real type.
template <typename F>
F Handler(const AFX_MSGMAP_ENTRY *e)
{
	return reinterpret_cast<F>(e->pfn);
}

} // namespace

void DeleteTempWindowMap()
{
	auto &temps = TemporaryMap();
	for(auto &entry : temps)
		entry.second->m_hWnd = nullptr; // temporary objects never own their window
	temps.clear();
}

void DetachFromMaps(HWND hwnd)
{
	PermanentMap().erase(hwnd);
	auto &temps = TemporaryMap();
	auto it = temps.find(hwnd);
	if(it != temps.end())
	{
		it->second->m_hWnd = nullptr;
		temps.erase(it);
	}
}

} // namespace linopenmpt::mfc

using namespace linopenmpt::mfc;

// ---- window procedure --------------------------------------------------------------

LRESULT AfxCallWndProc(CWnd *wnd, HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
	ThreadState &state = State();
	const MSG saved = state.lastSent;
	state.lastSent = MSG{hwnd, msg, wp, lp, 0, {0, 0}};
	LRESULT result = 0;
	try
	{
		result = wnd->WindowProc(msg, wp, lp);
	} catch(CException *e)
	{
		// As in MFC: the application reports it, the message fails.
		if(CWinThread *thread = AfxGetThread())
			result = thread->ProcessWndProcException(e, &state.lastSent);
		e->Delete();
	}
	state.lastSent = saved;
	return result;
}

LRESULT CALLBACK AfxWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
	CWnd *wnd = CWnd::FromHandlePermanent(hwnd);
	if(wnd == nullptr)
		return ::DefWindowProc(hwnd, msg, wp, lp);
	return AfxCallWndProc(wnd, hwnd, msg, wp, lp);
}

LPCTSTR AfxRegisterWndClass(UINT classStyle, HCURSOR cursor, HBRUSH background, HICON icon)
{
	// One class per combination, named as MFC names them.
	static auto *names = new std::vector<std::wstring>;
	wchar_t name[96];
	std::swprintf(name, 96, L"Afx:%x:%x:%p:%p:%p", 0x400000u, classStyle, static_cast<void *>(cursor), static_cast<void *>(background), static_cast<void *>(icon));
	for(const auto &n : *names)
	{
		if(n == name)
			return n.c_str();
	}
	names->push_back(name);
	WNDCLASS wc{};
	wc.style = classStyle;
	wc.lpfnWndProc = &::DefWindowProc;
	wc.hInstance = AfxGetInstanceHandle();
	wc.hCursor = cursor;
	wc.hbrBackground = background;
	wc.hIcon = icon;
	wc.lpszClassName = names->back().c_str();
	::RegisterClass(&wc);
	return names->back().c_str();
}

// ---- handle maps ---------------------------------------------------------------------

CWnd *CWnd::FromHandlePermanent(HWND hwnd)
{
	if(hwnd == nullptr)
		return nullptr;
	auto &map = PermanentMap();
	auto it = map.find(hwnd);
	return it != map.end() ? it->second : nullptr;
}

CWnd *CWnd::FromHandle(HWND hwnd)
{
	if(hwnd == nullptr)
		return nullptr;
	if(CWnd *wnd = FromHandlePermanent(hwnd))
		return wnd;
	// A temporary object, deleted at idle time as in MFC.
	auto &temps = TemporaryMap();
	auto &wnd = temps[hwnd];
	if(!wnd)
	{
		wnd = std::make_unique<CWnd>();
		wnd->m_hWnd = hwnd;
	}
	return wnd.get();
}

BOOL CWnd::Attach(HWND hwnd)
{
	if(hwnd == nullptr)
		return FALSE;
	DetachFromMaps(hwnd);
	PermanentMap()[hwnd] = this;
	m_hWnd = hwnd;
	return TRUE;
}

HWND CWnd::Detach()
{
	const HWND hwnd = m_hWnd;
	if(hwnd != nullptr)
	{
		auto &map = PermanentMap();
		auto it = map.find(hwnd);
		if(it != map.end() && it->second == this)
			map.erase(it);
	}
	m_hWnd = nullptr;
	return hwnd;
}

CWnd::~CWnd()
{
	if(m_hWnd != nullptr && FromHandlePermanent(m_hWnd) == this)
	{
		// Destroying the window while its object goes away; MFC does the
		// same (with a warning). OnNcDestroy() detaches.
		::DestroyWindow(m_hWnd);
		if(m_hWnd != nullptr)
			Detach();
	}
}

BOOL CWnd::SubclassWindow(HWND hwnd)
{
	if(!Attach(hwnd))
		return FALSE;
	PreSubclassWindow();
	const auto old = reinterpret_cast<WNDPROC>(::SetWindowLongPtr(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&AfxWndProc)));
	if(old != &AfxWndProc)
		*GetSuperWndProcAddr() = old;
	return TRUE;
}

BOOL CWnd::SubclassDlgItem(UINT id, CWnd *parent)
{
	if(parent == nullptr)
		return FALSE;
	const HWND control = ::GetDlgItem(parent->m_hWnd, static_cast<int>(id));
	return control != nullptr && SubclassWindow(control);
}

HWND CWnd::UnsubclassWindow()
{
	if(m_hWnd == nullptr)
		return nullptr;
	if(WNDPROC super = *GetSuperWndProcAddr())
	{
		::SetWindowLongPtr(m_hWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(super));
		*GetSuperWndProcAddr() = nullptr;
	}
	return Detach();
}

// ---- creation and destruction ---------------------------------------------------------

BOOL CWnd::PreCreateWindow(CREATESTRUCT &cs)
{
	if(cs.lpszClass == nullptr)
		cs.lpszClass = AfxRegisterWndClass(CS_DBLCLKS, ::LoadCursor(nullptr, IDC_ARROW), nullptr, nullptr);
	return TRUE;
}

BOOL CWnd::CreateEx(DWORD exStyle, LPCTSTR className, LPCTSTR windowName, DWORD style, const RECT &rect, CWnd *parent, UINT id, LPVOID param)
{
	return CreateEx(exStyle, className, windowName, style, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top,
		parent ? parent->GetSafeHwnd() : nullptr, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(id)), param);
}

BOOL CWnd::CreateEx(DWORD exStyle, LPCTSTR className, LPCTSTR windowName, DWORD style, int x, int y, int width, int height, HWND parent, HMENU menuOrId, LPVOID param)
{
	CREATESTRUCT cs{};
	cs.dwExStyle = exStyle;
	cs.lpszClass = className;
	cs.lpszName = windowName;
	cs.style = static_cast<LONG>(style);
	cs.x = x;
	cs.y = y;
	cs.cx = width;
	cs.cy = height;
	cs.hwndParent = parent;
	cs.hMenu = menuOrId;
	cs.hInstance = AfxGetInstanceHandle();
	cs.lpCreateParams = param;
	if(!PreCreateWindow(cs))
	{
		PostNcDestroy();
		return FALSE;
	}
	HookWindowCreate(this);
	const HWND hwnd = ::CreateWindowEx(cs.dwExStyle, cs.lpszClass, cs.lpszName, static_cast<DWORD>(cs.style), cs.x, cs.y, cs.cx, cs.cy,
		cs.hwndParent, cs.hMenu, cs.hInstance, cs.lpCreateParams);
	if(!UnhookWindowCreate())
		PostNcDestroy(); // the CBT hook never ran: clean up as MFC does
	return hwnd != nullptr;
}

BOOL CWnd::Create(LPCTSTR className, LPCTSTR windowName, DWORD style, const RECT &rect, CWnd *parent, UINT id, CCreateContext *context)
{
	return CreateEx(0, className, windowName, style | WS_CHILD, rect, parent, id, context);
}

BOOL CWnd::DestroyWindow()
{
	if(m_hWnd == nullptr)
		return FALSE;
	return ::DestroyWindow(m_hWnd);
}

void CWnd::OnNcDestroy()
{
	// The main window going away ends the application, as in MFC.
	if(CWinThread *thread = AfxGetThread())
	{
		if(thread->m_pMainWnd == this)
		{
			::PostQuitMessage(0);
			thread->m_pMainWnd = nullptr;
		}
	}
	const auto proc = reinterpret_cast<WNDPROC>(::GetWindowLongPtr(m_hWnd, GWLP_WNDPROC));
	Default();
	if(m_hWnd != nullptr && reinterpret_cast<WNDPROC>(::GetWindowLongPtr(m_hWnd, GWLP_WNDPROC)) == proc)
	{
		if(WNDPROC super = *GetSuperWndProcAddr())
			::SetWindowLongPtr(m_hWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(super));
	}
	Detach();
	PostNcDestroy();
}

// ---- message processing ------------------------------------------------------------------

const MSG *CWnd::GetCurrentMessage()
{
	return &State().lastSent;
}

LRESULT CWnd::Default()
{
	const MSG &msg = State().lastSent;
	return DefWindowProc(msg.message, msg.wParam, msg.lParam);
}

LRESULT CWnd::DefWindowProc(UINT msg, WPARAM wp, LPARAM lp)
{
	if(m_pfnSuper != nullptr)
		return ::CallWindowProc(m_pfnSuper, m_hWnd, msg, wp, lp);
	return ::DefWindowProc(m_hWnd, msg, wp, lp);
}

LRESULT CWnd::WindowProc(UINT msg, WPARAM wp, LPARAM lp)
{
	LRESULT result = 0;
	if(!OnWndMsg(msg, wp, lp, &result))
		result = DefWindowProc(msg, wp, lp);
	return result;
}

BOOL CWnd::PreTranslateMessage(MSG *)
{
	return FALSE;
}

BOOL CWnd::WalkPreTranslateTree(HWND stop, MSG *msg)
{
	for(HWND hwnd = msg->hwnd; hwnd != nullptr; hwnd = ::GetParent(hwnd))
	{
		if(CWnd *wnd = FromHandlePermanent(hwnd))
		{
			if(wnd->PreTranslateMessage(msg))
				return TRUE;
		}
		if(hwnd == stop)
			break;
	}
	return FALSE;
}

BOOL CWnd::OnCommand(WPARAM wp, LPARAM lp)
{
	const UINT id = LOWORD(wp);
	const auto control = reinterpret_cast<HWND>(lp);
	int code = HIWORD(wp);
	if(control == nullptr)
	{
		// Menu commands and accelerators.
		if(id == 0)
			return FALSE;
		code = CN_COMMAND;
	} else
	{
		// Control notifications: the control may handle them itself.
		if(ReflectLastMsg(control))
			return TRUE;
		if(id == 0)
			return FALSE;
	}
	return OnCmdMsg(id, code, nullptr, nullptr);
}

BOOL CWnd::OnNotify(WPARAM, LPARAM lp, LRESULT *result)
{
	auto *nmhdr = reinterpret_cast<NMHDR *>(lp);
	if(nmhdr == nullptr)
		return FALSE;
	const HWND control = nmhdr->hwndFrom;
	if(control != nullptr && ReflectLastMsg(control, result))
		return TRUE;
	const UINT id = control ? static_cast<UINT>(::GetDlgCtrlID(control)) : static_cast<UINT>(nmhdr->idFrom);
	AFX_NOTIFY notify{result, nmhdr};
	return OnCmdMsg(id, static_cast<int>(MAKELONG(nmhdr->code, WM_NOTIFY)), &notify, nullptr);
}

BOOL CWnd::ReflectLastMsg(HWND child, LRESULT *result)
{
	CWnd *wnd = FromHandlePermanent(child);
	if(wnd == nullptr)
		return FALSE;
	return wnd->SendChildNotifyLastMsg(result);
}

BOOL CWnd::SendChildNotifyLastMsg(LRESULT *result)
{
	const MSG &msg = State().lastSent;
	return OnChildNotify(msg.message, msg.wParam, msg.lParam, result);
}

BOOL CWnd::OnChildNotify(UINT msg, WPARAM wp, LPARAM lp, LRESULT *result)
{
	return ReflectChildNotify(msg, wp, lp, result);
}

BOOL CWnd::ReflectChildNotify(UINT msg, WPARAM wp, LPARAM lp, LRESULT *result)
{
	switch(msg)
	{
	case WM_HSCROLL:
	case WM_VSCROLL:
	case WM_PARENTNOTIFY:
	case WM_DRAWITEM:
	case WM_MEASUREITEM:
	case WM_DELETEITEM:
	case WM_VKEYTOITEM:
	case WM_CHARTOITEM:
	case WM_COMPAREITEM:
		return OnWndMsg(WM_REFLECT_BASE + msg, wp, lp, result);
	case WM_COMMAND:
		if(OnCmdMsg(0, static_cast<int>(MAKELONG(HIWORD(wp), WM_REFLECT_BASE + WM_COMMAND)), nullptr, nullptr))
		{
			if(result)
				*result = 1;
			return TRUE;
		}
		return FALSE;
	case WM_NOTIFY:
		{
			auto *nmhdr = reinterpret_cast<NMHDR *>(lp);
			AFX_NOTIFY notify{result, nmhdr};
			return OnCmdMsg(0, static_cast<int>(MAKELONG(nmhdr->code, WM_REFLECT_BASE + WM_NOTIFY)), &notify, nullptr);
		}
	default:
		if(msg >= WM_CTLCOLORMSGBOX && msg <= WM_CTLCOLORSTATIC)
		{
			// ON_WM_CTLCOLOR_REFLECT: HBRUSH CtlColor(CDC *, UINT)
			const AFX_MSGMAP_ENTRY *e = AfxFindMessageEntry(GetMessageMap(), WM_REFLECT_BASE + WM_CTLCOLOR, 0, 0);
			if(e == nullptr || e->nSig != AfxSig_wm)
				return FALSE;
			const HBRUSH brush = (this->*Handler<HBRUSH (CWnd::*)(CDC *, UINT)>(e))(CDC::FromHandle(reinterpret_cast<HDC>(wp)), msg - WM_CTLCOLORMSGBOX);
			if(brush == nullptr)
				return FALSE;
			if(result)
				*result = reinterpret_cast<LRESULT>(brush);
			return TRUE;
		}
		return FALSE;
	}
}

BOOL CWnd::OnWndMsg(UINT msg, WPARAM wp, LPARAM lp, LRESULT *pResult)
{
	LRESULT result = 0;
	if(msg == WM_COMMAND)
	{
		if(!OnCommand(wp, lp))
			return FALSE;
		if(pResult)
			*pResult = 1;
		return TRUE;
	}
	if(msg == WM_NOTIFY)
	{
		auto *nmhdr = reinterpret_cast<NMHDR *>(lp);
		if(nmhdr == nullptr || !OnNotify(wp, lp, &result))
			return FALSE;
		if(pResult)
			*pResult = result;
		return TRUE;
	}

	// WM_CTLCOLORxxx all go to OnCtlColor(), with the control type.
	UINT lookup = msg;
	UINT ctlColorType = 0;
	if(msg >= WM_CTLCOLORMSGBOX && msg <= WM_CTLCOLORSTATIC)
	{
		lookup = WM_CTLCOLOR;
		ctlColorType = msg - WM_CTLCOLORMSGBOX;
	}
	const AFX_MSGMAP_ENTRY *e = AfxFindMessageEntry(GetMessageMap(), lookup, 0, 0);
	if(e == nullptr)
	{
		// Messages CWnd's own message map handles in MFC.
		switch(msg)
		{
		case WM_NCDESTROY:
			OnNcDestroy();
			return TRUE;
		case WM_DRAWITEM:
		case WM_MEASUREITEM:
		case WM_DELETEITEM:
		case WM_COMPAREITEM:
			{
				// Owner-drawn controls draw themselves (reflection).
				HWND control = nullptr;
				if(msg == WM_DRAWITEM && lp)
					control = reinterpret_cast<LPDRAWITEMSTRUCT>(lp)->hwndItem;
				else if(wp)
					control = ::GetDlgItem(m_hWnd, static_cast<int>(wp));
				if(control != nullptr && ReflectLastMsg(control, pResult))
				{
					if(pResult)
						*pResult = TRUE;
					return TRUE;
				}
				return FALSE;
			}
		case WM_HSCROLL:
		case WM_VSCROLL:
			if(lp != 0 && ReflectLastMsg(reinterpret_cast<HWND>(lp), pResult))
				return TRUE;
			return FALSE;
		default:
			if(msg >= WM_CTLCOLORMSGBOX && msg <= WM_CTLCOLORSTATIC && lp != 0)
				return ReflectLastMsg(reinterpret_cast<HWND>(lp), pResult);
			return FALSE;
		}
	}

	if(e->nSig == AfxSig_message)
	{
		result = (this->*Handler<LRESULT (CWnd::*)(WPARAM, LPARAM)>(e))(wp, lp);
	} else if(e->nSig == AfxSig_wm)
	{
		switch(lookup)
		{
		// void ()
		case WM_DESTROY:
		case WM_CLOSE:
		case WM_PAINT:
		case WM_NCPAINT:
		case WM_MOUSELEAVE:
		case WM_NCDESTROY:
			(this->*Handler<void (CWnd::*)()>(e))();
			break;
		case WM_CREATE:
			result = (this->*Handler<int (CWnd::*)(LPCREATESTRUCT)>(e))(reinterpret_cast<LPCREATESTRUCT>(lp));
			break;
		case WM_SIZE:
			(this->*Handler<void (CWnd::*)(UINT, int, int)>(e))(static_cast<UINT>(wp), static_cast<short>(LOWORD(lp)), static_cast<short>(HIWORD(lp)));
			break;
		case WM_MOVE:
			(this->*Handler<void (CWnd::*)(int, int)>(e))(static_cast<short>(LOWORD(lp)), static_cast<short>(HIWORD(lp)));
			break;
		case WM_ERASEBKGND:
			result = (this->*Handler<BOOL (CWnd::*)(CDC *)>(e))(CDC::FromHandle(reinterpret_cast<HDC>(wp)));
			break;
		case WM_SETFOCUS:
		case WM_KILLFOCUS:
			(this->*Handler<void (CWnd::*)(CWnd *)>(e))(FromHandle(reinterpret_cast<HWND>(wp)));
			break;
		case WM_CAPTURECHANGED:
			(this->*Handler<void (CWnd::*)(CWnd *)>(e))(FromHandle(reinterpret_cast<HWND>(lp)));
			break;
		case WM_TIMER:
			(this->*Handler<void (CWnd::*)(UINT_PTR)>(e))(static_cast<UINT_PTR>(wp));
			break;
		case WM_HSCROLL:
		case WM_VSCROLL:
			(this->*Handler<void (CWnd::*)(UINT, UINT, CScrollBar *)>(e))(LOWORD(wp), HIWORD(wp), static_cast<CScrollBar *>(FromHandle(reinterpret_cast<HWND>(lp))));
			break;
		case WM_MOUSEMOVE:
		case WM_LBUTTONDOWN:
		case WM_LBUTTONUP:
		case WM_LBUTTONDBLCLK:
		case WM_RBUTTONDOWN:
		case WM_RBUTTONUP:
		case WM_RBUTTONDBLCLK:
		case WM_MBUTTONDOWN:
		case WM_MBUTTONUP:
		case WM_MBUTTONDBLCLK:
		case WM_MOUSEHOVER:
			(this->*Handler<void (CWnd::*)(UINT, CPoint)>(e))(static_cast<UINT>(wp), PointFromLParam(lp));
			break;
		case WM_XBUTTONDOWN:
		case WM_XBUTTONUP:
		case WM_XBUTTONDBLCLK:
			(this->*Handler<void (CWnd::*)(UINT, UINT, CPoint)>(e))(GET_KEYSTATE_WPARAM(wp), GET_XBUTTON_WPARAM(wp), PointFromLParam(lp));
			result = TRUE;
			break;
		case WM_MOUSEWHEEL:
			result = (this->*Handler<BOOL (CWnd::*)(UINT, short, CPoint)>(e))(GET_KEYSTATE_WPARAM(wp), GET_WHEEL_DELTA_WPARAM(wp), PointFromLParam(lp));
			break;
		case WM_MOUSEHWHEEL:
			(this->*Handler<void (CWnd::*)(UINT, short, CPoint)>(e))(GET_KEYSTATE_WPARAM(wp), GET_WHEEL_DELTA_WPARAM(wp), PointFromLParam(lp));
			break;
		case WM_MOUSEACTIVATE:
			result = (this->*Handler<int (CWnd::*)(CWnd *, UINT, UINT)>(e))(FromHandle(reinterpret_cast<HWND>(wp)), LOWORD(lp), HIWORD(lp));
			break;
		case WM_CHAR:
		case WM_KEYDOWN:
		case WM_KEYUP:
		case WM_SYSKEYDOWN:
		case WM_SYSKEYUP:
		case WM_SYSCHAR:
		case WM_DEADCHAR:
			(this->*Handler<void (CWnd::*)(UINT, UINT, UINT)>(e))(static_cast<UINT>(wp), LOWORD(lp), HIWORD(lp));
			break;
		case WM_GETDLGCODE:
			result = (this->*Handler<UINT (CWnd::*)()>(e))();
			break;
		case WM_SHOWWINDOW:
			(this->*Handler<void (CWnd::*)(BOOL, UINT)>(e))(static_cast<BOOL>(wp), static_cast<UINT>(lp));
			break;
		case WM_CONTEXTMENU:
			(this->*Handler<void (CWnd::*)(CWnd *, CPoint)>(e))(FromHandle(reinterpret_cast<HWND>(wp)), PointFromLParam(lp));
			break;
		case WM_DROPFILES:
			(this->*Handler<void (CWnd::*)(HDROP)>(e))(reinterpret_cast<HDROP>(wp));
			break;
		case WM_GETMINMAXINFO:
			(this->*Handler<void (CWnd::*)(MINMAXINFO *)>(e))(reinterpret_cast<MINMAXINFO *>(lp));
			break;
		case WM_SETCURSOR:
			result = (this->*Handler<BOOL (CWnd::*)(CWnd *, UINT, UINT)>(e))(FromHandle(reinterpret_cast<HWND>(wp)), LOWORD(lp), HIWORD(lp));
			break;
		case WM_ACTIVATE:
			(this->*Handler<void (CWnd::*)(UINT, CWnd *, BOOL)>(e))(LOWORD(wp), FromHandle(reinterpret_cast<HWND>(lp)), HIWORD(wp));
			break;
		case WM_ACTIVATEAPP:
			(this->*Handler<void (CWnd::*)(BOOL, DWORD)>(e))(static_cast<BOOL>(wp), static_cast<DWORD>(lp));
			break;
		case WM_SYSCOMMAND:
			(this->*Handler<void (CWnd::*)(UINT, LPARAM)>(e))(static_cast<UINT>(wp), lp);
			break;
		case WM_NCCALCSIZE:
			(this->*Handler<void (CWnd::*)(BOOL, NCCALCSIZE_PARAMS *)>(e))(static_cast<BOOL>(wp), reinterpret_cast<NCCALCSIZE_PARAMS *>(lp));
			break;
		case WM_NCHITTEST:
			result = (this->*Handler<LRESULT (CWnd::*)(CPoint)>(e))(PointFromLParam(lp));
			break;
		case WM_NCACTIVATE:
			result = (this->*Handler<BOOL (CWnd::*)(BOOL)>(e))(static_cast<BOOL>(wp));
			break;
		case WM_NCMOUSEMOVE:
		case WM_NCLBUTTONDOWN:
		case WM_NCLBUTTONUP:
		case WM_NCLBUTTONDBLCLK:
		case WM_NCRBUTTONDOWN:
		case WM_NCRBUTTONUP:
		case WM_NCRBUTTONDBLCLK:
		case WM_NCMBUTTONDOWN:
		case WM_NCMBUTTONUP:
		case WM_NCMBUTTONDBLCLK:
			(this->*Handler<void (CWnd::*)(UINT, CPoint)>(e))(static_cast<UINT>(wp), PointFromLParam(lp));
			break;
		case WM_INITMENU:
			(this->*Handler<void (CWnd::*)(CMenu *)>(e))(CMenu::FromHandle(reinterpret_cast<HMENU>(wp)));
			break;
		case WM_INITMENUPOPUP:
			(this->*Handler<void (CWnd::*)(CMenu *, UINT, BOOL)>(e))(CMenu::FromHandle(reinterpret_cast<HMENU>(wp)), LOWORD(lp), HIWORD(lp));
			break;
		case WM_MENUSELECT:
			(this->*Handler<void (CWnd::*)(UINT, UINT, HMENU)>(e))(LOWORD(wp), HIWORD(wp), reinterpret_cast<HMENU>(lp));
			break;
		case WM_DRAWITEM:
			(this->*Handler<void (CWnd::*)(int, LPDRAWITEMSTRUCT)>(e))(static_cast<int>(wp), reinterpret_cast<LPDRAWITEMSTRUCT>(lp));
			result = TRUE;
			break;
		case WM_MEASUREITEM:
			(this->*Handler<void (CWnd::*)(int, LPMEASUREITEMSTRUCT)>(e))(static_cast<int>(wp), reinterpret_cast<LPMEASUREITEMSTRUCT>(lp));
			result = TRUE;
			break;
		case WM_DEVICECHANGE:
			result = (this->*Handler<BOOL (CWnd::*)(UINT, DWORD_PTR)>(e))(static_cast<UINT>(wp), static_cast<DWORD_PTR>(lp));
			break;
		case WM_QUERYENDSESSION:
			result = (this->*Handler<BOOL (CWnd::*)()>(e))();
			break;
		case WM_ENDSESSION:
		case WM_ENABLE:
			(this->*Handler<void (CWnd::*)(BOOL)>(e))(static_cast<BOOL>(wp));
			break;
		case WM_CTLCOLOR:
			result = reinterpret_cast<LRESULT>((this->*Handler<HBRUSH (CWnd::*)(CDC *, CWnd *, UINT)>(e))(
				CDC::FromHandle(reinterpret_cast<HDC>(wp)), FromHandle(reinterpret_cast<HWND>(lp)), ctlColorType));
			break;
		case WM_MDIACTIVATE:
			// wParam: the window losing activation, lParam: the one gaining it.
			(this->*Handler<void (CWnd::*)(BOOL, CWnd *, CWnd *)>(e))(reinterpret_cast<HWND>(lp) == m_hWnd,
				FromHandle(reinterpret_cast<HWND>(lp)), FromHandle(reinterpret_cast<HWND>(wp)));
			break;
		case WM_WINDOWPOSCHANGED:
		case WM_WINDOWPOSCHANGING:
			(this->*Handler<void (CWnd::*)(WINDOWPOS *)>(e))(reinterpret_cast<WINDOWPOS *>(lp));
			break;
		case WM_PARENTNOTIFY:
			(this->*Handler<void (CWnd::*)(UINT, LPARAM)>(e))(static_cast<UINT>(wp), lp);
			break;
		default:
			return FALSE;
		}
	} else
	{
		return FALSE;
	}
	if(pResult)
		*pResult = result;
	return TRUE;
}

// ---- default message handlers (MFC calls Default() for most) ----------------------------

int CWnd::OnCreate(LPCREATESTRUCT) { return static_cast<int>(Default()); }
void CWnd::OnDestroy() { Default(); }
void CWnd::OnClose() { Default(); }
void CWnd::OnSize(UINT, int, int) { Default(); }
void CWnd::OnMove(int, int) { Default(); }
void CWnd::OnPaint() { Default(); }
BOOL CWnd::OnEraseBkgnd(CDC *) { return static_cast<BOOL>(Default()); }
void CWnd::OnSetFocus(CWnd *) { Default(); }
void CWnd::OnKillFocus(CWnd *) { Default(); }
void CWnd::OnTimer(UINT_PTR) { Default(); }
void CWnd::OnHScroll(UINT, UINT, CScrollBar *scrollBar)
{
	if(scrollBar != nullptr && ReflectLastMsg(scrollBar->GetSafeHwnd()))
		return;
	Default();
}
void CWnd::OnVScroll(UINT, UINT, CScrollBar *scrollBar)
{
	if(scrollBar != nullptr && ReflectLastMsg(scrollBar->GetSafeHwnd()))
		return;
	Default();
}
void CWnd::OnMouseMove(UINT, CPoint) { Default(); }
void CWnd::OnLButtonDown(UINT, CPoint) { Default(); }
void CWnd::OnLButtonUp(UINT, CPoint) { Default(); }
void CWnd::OnLButtonDblClk(UINT, CPoint) { Default(); }
void CWnd::OnRButtonDown(UINT, CPoint) { Default(); }
void CWnd::OnRButtonUp(UINT, CPoint) { Default(); }
void CWnd::OnRButtonDblClk(UINT, CPoint) { Default(); }
void CWnd::OnMButtonDown(UINT, CPoint) { Default(); }
void CWnd::OnMButtonUp(UINT, CPoint) { Default(); }
void CWnd::OnMButtonDblClk(UINT, CPoint) { Default(); }
void CWnd::OnXButtonDown(UINT, UINT, CPoint) { Default(); }
void CWnd::OnXButtonUp(UINT, UINT, CPoint) { Default(); }
BOOL CWnd::OnMouseWheel(UINT, short, CPoint) { return static_cast<BOOL>(Default()); }
void CWnd::OnMouseHWheel(UINT, short, CPoint) { Default(); }
void CWnd::OnMouseHover(UINT, CPoint) { Default(); }
void CWnd::OnMouseLeave() { Default(); }
int CWnd::OnMouseActivate(CWnd *, UINT, UINT) { return static_cast<int>(Default()); }
void CWnd::OnChar(UINT, UINT, UINT) { Default(); }
void CWnd::OnKeyDown(UINT, UINT, UINT) { Default(); }
void CWnd::OnKeyUp(UINT, UINT, UINT) { Default(); }
void CWnd::OnSysKeyDown(UINT, UINT, UINT) { Default(); }
void CWnd::OnSysKeyUp(UINT, UINT, UINT) { Default(); }
void CWnd::OnSysChar(UINT, UINT, UINT) { Default(); }
UINT CWnd::OnGetDlgCode() { return static_cast<UINT>(Default()); }
void CWnd::OnShowWindow(BOOL, UINT) { Default(); }
void CWnd::OnContextMenu(CWnd *, CPoint) { Default(); }
void CWnd::OnDropFiles(HDROP) { Default(); }
void CWnd::OnGetMinMaxInfo(MINMAXINFO *) { Default(); }
BOOL CWnd::OnSetCursor(CWnd *, UINT, UINT) { return static_cast<BOOL>(Default()); }
void CWnd::OnActivate(UINT, CWnd *, BOOL) { Default(); }
void CWnd::OnActivateApp(BOOL, DWORD) { Default(); }
void CWnd::OnSysCommand(UINT, LPARAM) { Default(); }
void CWnd::OnNcCalcSize(BOOL, NCCALCSIZE_PARAMS *) { Default(); }
LRESULT CWnd::OnNcHitTest(CPoint) { return Default(); }
void CWnd::OnNcPaint() { Default(); }
BOOL CWnd::OnNcActivate(BOOL) { return static_cast<BOOL>(Default()); }
void CWnd::OnNcMouseMove(UINT, CPoint) { Default(); }
void CWnd::OnNcLButtonDown(UINT, CPoint) { Default(); }
void CWnd::OnNcLButtonUp(UINT, CPoint) { Default(); }
void CWnd::OnNcLButtonDblClk(UINT, CPoint) { Default(); }
void CWnd::OnInitMenu(CMenu *) { Default(); }
void CWnd::OnInitMenuPopup(CMenu *, UINT, BOOL) { Default(); }
void CWnd::OnMenuSelect(UINT, UINT, HMENU) { Default(); }
void CWnd::OnDrawItem(int, LPDRAWITEMSTRUCT dis)
{
	if(dis != nullptr && ReflectLastMsg(dis->hwndItem))
		return;
	Default();
}
void CWnd::OnMeasureItem(int, LPMEASUREITEMSTRUCT) { Default(); }
BOOL CWnd::OnDeviceChange(UINT, DWORD_PTR) { return static_cast<BOOL>(Default()); }
BOOL CWnd::OnQueryEndSession() { return TRUE; }
void CWnd::OnEndSession(BOOL) { Default(); }
HBRUSH CWnd::OnCtlColor(CDC *, CWnd *wnd, UINT)
{
	LRESULT result = 0;
	if(wnd != nullptr && ReflectLastMsg(wnd->m_hWnd, &result))
		return reinterpret_cast<HBRUSH>(result);
	return reinterpret_cast<HBRUSH>(Default());
}
void CWnd::OnCaptureChanged(CWnd *) { Default(); }
void CWnd::OnWindowPosChanged(WINDOWPOS *) { Default(); }
void CWnd::OnWindowPosChanging(WINDOWPOS *) { Default(); }
void CWnd::OnParentNotify(UINT, LPARAM) { Default(); }
void CWnd::OnEnable(BOOL) { Default(); }
