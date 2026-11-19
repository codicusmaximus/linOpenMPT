// mfc-qt: MFC message maps.
//
// Upstream routes every menu command, control notification and window
// message through MFC message maps (BEGIN_MESSAGE_MAP ... END_MESSAGE_MAP).
// These macros build the same static tables MFC builds, so upstream's maps
// compile unchanged and the Qt layer can dispatch Qt actions and events to
// the same handlers, through CCmdTarget::OnCmdMsg and CWnd::OnWndMsg.
//
// Each entry stores the handler as a generic member pointer plus a signature
// code saying how to call it. The helpers below take the handler's real type,
// so a handler with the wrong signature is a compile error, as in MFC.

#pragma once

#include "win32_messages.h"

#include <type_traits>

class CCmdTarget;
class CCmdUI;
class CWnd;
class CDC;
struct NMHDR;

using AFX_PMSG = void (CCmdTarget::*)();

// How to call an entry's handler. Window messages (sig_wm) are called by
// message number, because each WM_ has its own fixed handler signature.
enum AfxSig : UINT_PTR
{
	AfxSig_end = 0,
	AfxSig_cmd,          // void ()
	AfxSig_cmd_ex,       // BOOL (UINT id)
	AfxSig_cmd_range,    // void (UINT id)
	AfxSig_cmd_ui,       // void (CCmdUI *)
	AfxSig_message,      // LRESULT (WPARAM, LPARAM)
	AfxSig_notify,       // void (NMHDR *, LRESULT *)
	AfxSig_notify_ex,    // BOOL (UINT id, NMHDR *, LRESULT *)
	AfxSig_wm,           // fixed per message, see the ON_WM_ list below
};

struct AFX_MSGMAP_ENTRY
{
	UINT nMessage;
	UINT nCode;
	UINT nID;
	UINT nLastID;
	UINT_PTR nSig;
	AFX_PMSG pfn;
};

// What CWnd::OnNotify passes to OnCmdMsg for WM_NOTIFY handlers.
struct AFX_NOTIFY
{
	LRESULT *pResult;
	NMHDR *pNMHDR;
};

struct AFX_MSGMAP
{
	const AFX_MSGMAP *(*pfnGetBaseMap)();
	const AFX_MSGMAP_ENTRY *lpEntries;
};

namespace mfcqt::msgmap
{
	// The member pointer is stored type-erased and cast back to its original
	// type before it is called, which is well-defined.
	template <typename F>
	constexpr AFX_PMSG erase(F f) noexcept
	{
		static_assert(std::is_member_function_pointer_v<F>);
		return reinterpret_cast<AFX_PMSG>(f);
	}
	template <typename C> constexpr AFX_PMSG cmd(void (C::*f)()) noexcept { return erase(f); }
	template <typename C> constexpr AFX_PMSG cmd_ex(BOOL (C::*f)(UINT)) noexcept { return erase(f); }
	template <typename C> constexpr AFX_PMSG cmd_range(void (C::*f)(UINT)) noexcept { return erase(f); }
	template <typename C> constexpr AFX_PMSG cmd_ui(void (C::*f)(CCmdUI *)) noexcept { return erase(f); }
	template <typename C> constexpr AFX_PMSG message(LRESULT (C::*f)(WPARAM, LPARAM)) noexcept { return erase(f); }
	template <typename C> constexpr AFX_PMSG notify(void (C::*f)(NMHDR *, LRESULT *)) noexcept { return erase(f); }
	template <typename C> constexpr AFX_PMSG notify_ex(BOOL (C::*f)(UINT, NMHDR *, LRESULT *)) noexcept { return erase(f); }
	template <typename F> constexpr AFX_PMSG wm(F f) noexcept { return erase(f); }
}

#define DECLARE_MESSAGE_MAP() \
protected: \
	static const AFX_MSGMAP *GetThisMessageMap(); \
	const AFX_MSGMAP *GetMessageMap() const override;

#define BEGIN_MESSAGE_MAP(theClass, baseClass) \
	const AFX_MSGMAP *theClass::GetMessageMap() const { return GetThisMessageMap(); } \
	const AFX_MSGMAP *theClass::GetThisMessageMap() \
	{ \
		using ThisClass = theClass; \
		using TheBaseClass = baseClass; \
		static const AFX_MSGMAP_ENTRY _messageEntries[] = \
		{

#define END_MESSAGE_MAP() \
			{ 0, 0, 0, 0, AfxSig_end, nullptr } \
		}; \
		static const AFX_MSGMAP messageMap = { &TheBaseClass::GetThisMessageMap, &_messageEntries[0] }; \
		return &messageMap; \
	}

// ---- commands ------------------------------------------------------------
#define ON_COMMAND(id, memberFxn) \
	{ WM_COMMAND, CN_COMMAND, (UINT)(WORD)(id), (UINT)(WORD)(id), AfxSig_cmd, ::mfcqt::msgmap::cmd(memberFxn) },
#define ON_COMMAND_EX(id, memberFxn) \
	{ WM_COMMAND, CN_COMMAND, (UINT)(WORD)(id), (UINT)(WORD)(id), AfxSig_cmd_ex, ::mfcqt::msgmap::cmd_ex(memberFxn) },
#define ON_COMMAND_RANGE(id, idLast, memberFxn) \
	{ WM_COMMAND, CN_COMMAND, (UINT)(WORD)(id), (UINT)(WORD)(idLast), AfxSig_cmd_range, ::mfcqt::msgmap::cmd_range(memberFxn) },
#define ON_UPDATE_COMMAND_UI(id, memberFxn) \
	{ WM_COMMAND, CN_UPDATE_COMMAND_UI, (UINT)(WORD)(id), (UINT)(WORD)(id), AfxSig_cmd_ui, ::mfcqt::msgmap::cmd_ui(memberFxn) },
#define ON_UPDATE_COMMAND_UI_RANGE(id, idLast, memberFxn) \
	{ WM_COMMAND, CN_UPDATE_COMMAND_UI, (UINT)(WORD)(id), (UINT)(WORD)(idLast), AfxSig_cmd_ui, ::mfcqt::msgmap::cmd_ui(memberFxn) },

// ---- control notifications (WM_COMMAND from a child control) -------------
#define ON_CONTROL(code, id, memberFxn) \
	{ WM_COMMAND, (UINT)(WORD)(code), (UINT)(WORD)(id), (UINT)(WORD)(id), AfxSig_cmd, ::mfcqt::msgmap::cmd(memberFxn) },
#define ON_CONTROL_RANGE(code, id, idLast, memberFxn) \
	{ WM_COMMAND, (UINT)(WORD)(code), (UINT)(WORD)(id), (UINT)(WORD)(idLast), AfxSig_cmd_range, ::mfcqt::msgmap::cmd_range(memberFxn) },
#define ON_BN_CLICKED(id, memberFxn) ON_CONTROL(BN_CLICKED, id, memberFxn)
#define ON_BN_DOUBLECLICKED(id, memberFxn) ON_CONTROL(BN_DOUBLECLICKED, id, memberFxn)
#define ON_EN_SETFOCUS(id, memberFxn) ON_CONTROL(EN_SETFOCUS, id, memberFxn)
#define ON_EN_KILLFOCUS(id, memberFxn) ON_CONTROL(EN_KILLFOCUS, id, memberFxn)
#define ON_EN_CHANGE(id, memberFxn) ON_CONTROL(EN_CHANGE, id, memberFxn)
#define ON_EN_UPDATE(id, memberFxn) ON_CONTROL(EN_UPDATE, id, memberFxn)
#define ON_CBN_SELCHANGE(id, memberFxn) ON_CONTROL(CBN_SELCHANGE, id, memberFxn)
#define ON_CBN_DBLCLK(id, memberFxn) ON_CONTROL(CBN_DBLCLK, id, memberFxn)
#define ON_CBN_SETFOCUS(id, memberFxn) ON_CONTROL(CBN_SETFOCUS, id, memberFxn)
#define ON_CBN_KILLFOCUS(id, memberFxn) ON_CONTROL(CBN_KILLFOCUS, id, memberFxn)
#define ON_CBN_EDITCHANGE(id, memberFxn) ON_CONTROL(CBN_EDITCHANGE, id, memberFxn)
#define ON_CBN_EDITUPDATE(id, memberFxn) ON_CONTROL(CBN_EDITUPDATE, id, memberFxn)
#define ON_CBN_DROPDOWN(id, memberFxn) ON_CONTROL(CBN_DROPDOWN, id, memberFxn)
#define ON_CBN_CLOSEUP(id, memberFxn) ON_CONTROL(CBN_CLOSEUP, id, memberFxn)
#define ON_CBN_SELENDOK(id, memberFxn) ON_CONTROL(CBN_SELENDOK, id, memberFxn)
#define ON_CBN_SELENDCANCEL(id, memberFxn) ON_CONTROL(CBN_SELENDCANCEL, id, memberFxn)
#define ON_LBN_SELCHANGE(id, memberFxn) ON_CONTROL(LBN_SELCHANGE, id, memberFxn)
#define ON_LBN_DBLCLK(id, memberFxn) ON_CONTROL(LBN_DBLCLK, id, memberFxn)
#define ON_LBN_SETFOCUS(id, memberFxn) ON_CONTROL(LBN_SETFOCUS, id, memberFxn)
#define ON_LBN_KILLFOCUS(id, memberFxn) ON_CONTROL(LBN_KILLFOCUS, id, memberFxn)
#define ON_CLBN_CHKCHANGE(id, memberFxn) ON_CONTROL(CLBN_CHKCHANGE, id, memberFxn)

// ---- WM_NOTIFY -----------------------------------------------------------
#define ON_NOTIFY(code, id, memberFxn) \
	{ WM_NOTIFY, (UINT)(WORD)(int)(code), (UINT)(WORD)(id), (UINT)(WORD)(id), AfxSig_notify, ::mfcqt::msgmap::notify(memberFxn) },
#define ON_NOTIFY_EX(code, id, memberFxn) \
	{ WM_NOTIFY, (UINT)(WORD)(int)(code), (UINT)(WORD)(id), (UINT)(WORD)(id), AfxSig_notify_ex, ::mfcqt::msgmap::notify_ex(memberFxn) },
#define ON_NOTIFY_EX_RANGE(code, id, idLast, memberFxn) \
	{ WM_NOTIFY, (UINT)(WORD)(int)(code), (UINT)(WORD)(id), (UINT)(WORD)(idLast), AfxSig_notify_ex, ::mfcqt::msgmap::notify_ex(memberFxn) },
#define ON_NOTIFY_REFLECT(code, memberFxn) \
	{ WM_NOTIFY + WM_REFLECT_BASE, (UINT)(WORD)(int)(code), 0, 0, AfxSig_notify, ::mfcqt::msgmap::notify(memberFxn) },

// ---- user and registered messages ---------------------------------------
#define ON_MESSAGE(msg, memberFxn) \
	{ (UINT)(msg), 0, 0, 0, AfxSig_message, ::mfcqt::msgmap::message(memberFxn) },
#define ON_THREAD_MESSAGE(msg, memberFxn) ON_MESSAGE(msg, memberFxn)

// ---- standard window messages --------------------------------------------
// The handler name is fixed per message, exactly as in MFC.
#define MFCQT_ON_WM(msg, handler) \
	{ (UINT)(msg), 0, 0, 0, AfxSig_wm, ::mfcqt::msgmap::wm(&ThisClass::handler) },

#define ON_WM_ACTIVATE() MFCQT_ON_WM(WM_ACTIVATE, OnActivate)
#define ON_WM_ACTIVATEAPP() MFCQT_ON_WM(WM_ACTIVATEAPP, OnActivateApp)
#define ON_WM_CHAR() MFCQT_ON_WM(WM_CHAR, OnChar)
#define ON_WM_CLOSE() MFCQT_ON_WM(WM_CLOSE, OnClose)
#define ON_WM_CONTEXTMENU() MFCQT_ON_WM(WM_CONTEXTMENU, OnContextMenu)
#define ON_WM_CREATE() MFCQT_ON_WM(WM_CREATE, OnCreate)
#define ON_WM_CTLCOLOR() MFCQT_ON_WM(WM_CTLCOLOR, OnCtlColor)
#define ON_WM_CTLCOLOR_REFLECT() MFCQT_ON_WM(WM_CTLCOLOR + WM_REFLECT_BASE, CtlColor)
#define ON_WM_DESTROY() MFCQT_ON_WM(WM_DESTROY, OnDestroy)
#define ON_WM_DEVICECHANGE() MFCQT_ON_WM(WM_DEVICECHANGE, OnDeviceChange)
#define ON_WM_DRAWITEM() MFCQT_ON_WM(WM_DRAWITEM, OnDrawItem)
#define ON_WM_DROPFILES() MFCQT_ON_WM(WM_DROPFILES, OnDropFiles)
#define ON_WM_ERASEBKGND() MFCQT_ON_WM(WM_ERASEBKGND, OnEraseBkgnd)
#define ON_WM_GETDLGCODE() MFCQT_ON_WM(WM_GETDLGCODE, OnGetDlgCode)
#define ON_WM_GETMINMAXINFO() MFCQT_ON_WM(WM_GETMINMAXINFO, OnGetMinMaxInfo)
#define ON_WM_HSCROLL() MFCQT_ON_WM(WM_HSCROLL, OnHScroll)
#define ON_WM_INITMENU() MFCQT_ON_WM(WM_INITMENU, OnInitMenu)
#define ON_WM_INITMENUPOPUP() MFCQT_ON_WM(WM_INITMENUPOPUP, OnInitMenuPopup)
#define ON_WM_KEYDOWN() MFCQT_ON_WM(WM_KEYDOWN, OnKeyDown)
#define ON_WM_KEYUP() MFCQT_ON_WM(WM_KEYUP, OnKeyUp)
#define ON_WM_KILLFOCUS() MFCQT_ON_WM(WM_KILLFOCUS, OnKillFocus)
#define ON_WM_LBUTTONDBLCLK() MFCQT_ON_WM(WM_LBUTTONDBLCLK, OnLButtonDblClk)
#define ON_WM_LBUTTONDOWN() MFCQT_ON_WM(WM_LBUTTONDOWN, OnLButtonDown)
#define ON_WM_LBUTTONUP() MFCQT_ON_WM(WM_LBUTTONUP, OnLButtonUp)
#define ON_WM_MBUTTONDBLCLK() MFCQT_ON_WM(WM_MBUTTONDBLCLK, OnMButtonDblClk)
#define ON_WM_MBUTTONDOWN() MFCQT_ON_WM(WM_MBUTTONDOWN, OnMButtonDown)
#define ON_WM_MBUTTONUP() MFCQT_ON_WM(WM_MBUTTONUP, OnMButtonUp)
#define ON_WM_MDIACTIVATE() MFCQT_ON_WM(WM_MDIACTIVATE, OnMDIActivate)
#define ON_WM_MENUSELECT() MFCQT_ON_WM(WM_MENUSELECT, OnMenuSelect)
#define ON_WM_MOUSEACTIVATE() MFCQT_ON_WM(WM_MOUSEACTIVATE, OnMouseActivate)
#define ON_WM_MOUSEHOVER() MFCQT_ON_WM(WM_MOUSEHOVER, OnMouseHover)
#define ON_WM_MOUSEHWHEEL() MFCQT_ON_WM(WM_MOUSEHWHEEL, OnMouseHWheel)
#define ON_WM_MOUSELEAVE() MFCQT_ON_WM(WM_MOUSELEAVE, OnMouseLeave)
#define ON_WM_MOUSEMOVE() MFCQT_ON_WM(WM_MOUSEMOVE, OnMouseMove)
#define ON_WM_MOUSEWHEEL() MFCQT_ON_WM(WM_MOUSEWHEEL, OnMouseWheel)
#define ON_WM_MOVE() MFCQT_ON_WM(WM_MOVE, OnMove)
#define ON_WM_NCACTIVATE() MFCQT_ON_WM(WM_NCACTIVATE, OnNcActivate)
#define ON_WM_NCCALCSIZE() MFCQT_ON_WM(WM_NCCALCSIZE, OnNcCalcSize)
#define ON_WM_NCHITTEST() MFCQT_ON_WM(WM_NCHITTEST, OnNcHitTest)
#define ON_WM_NCLBUTTONDBLCLK() MFCQT_ON_WM(WM_NCLBUTTONDBLCLK, OnNcLButtonDblClk)
#define ON_WM_NCLBUTTONDOWN() MFCQT_ON_WM(WM_NCLBUTTONDOWN, OnNcLButtonDown)
#define ON_WM_NCLBUTTONUP() MFCQT_ON_WM(WM_NCLBUTTONUP, OnNcLButtonUp)
#define ON_WM_NCMOUSEMOVE() MFCQT_ON_WM(WM_NCMOUSEMOVE, OnNcMouseMove)
#define ON_WM_NCPAINT() MFCQT_ON_WM(WM_NCPAINT, OnNcPaint)
#define ON_WM_PAINT() MFCQT_ON_WM(WM_PAINT, OnPaint)
#define ON_WM_QUERYENDSESSION() MFCQT_ON_WM(WM_QUERYENDSESSION, OnQueryEndSession)
#define ON_WM_RBUTTONDBLCLK() MFCQT_ON_WM(WM_RBUTTONDBLCLK, OnRButtonDblClk)
#define ON_WM_RBUTTONDOWN() MFCQT_ON_WM(WM_RBUTTONDOWN, OnRButtonDown)
#define ON_WM_RBUTTONUP() MFCQT_ON_WM(WM_RBUTTONUP, OnRButtonUp)
#define ON_WM_SETCURSOR() MFCQT_ON_WM(WM_SETCURSOR, OnSetCursor)
#define ON_WM_SETFOCUS() MFCQT_ON_WM(WM_SETFOCUS, OnSetFocus)
#define ON_WM_SHOWWINDOW() MFCQT_ON_WM(WM_SHOWWINDOW, OnShowWindow)
#define ON_WM_SIZE() MFCQT_ON_WM(WM_SIZE, OnSize)
#define ON_WM_SYSCOMMAND() MFCQT_ON_WM(WM_SYSCOMMAND, OnSysCommand)
#define ON_WM_TIMER() MFCQT_ON_WM(WM_TIMER, OnTimer)
#define ON_WM_VSCROLL() MFCQT_ON_WM(WM_VSCROLL, OnVScroll)
#define ON_WM_XBUTTONDOWN() MFCQT_ON_WM(WM_XBUTTONDOWN, OnXButtonDown)
#define ON_WM_XBUTTONUP() MFCQT_ON_WM(WM_XBUTTONUP, OnXButtonUp)
#define ON_WM_NCDESTROY() MFCQT_ON_WM(WM_NCDESTROY, OnNcDestroy)
#define ON_WM_SYSKEYDOWN() MFCQT_ON_WM(WM_SYSKEYDOWN, OnSysKeyDown)
#define ON_WM_SYSKEYUP() MFCQT_ON_WM(WM_SYSKEYUP, OnSysKeyUp)
#define ON_WM_SYSCHAR() MFCQT_ON_WM(WM_SYSCHAR, OnSysChar)
#define ON_WM_CAPTURECHANGED() MFCQT_ON_WM(WM_CAPTURECHANGED, OnCaptureChanged)
#define ON_WM_WINDOWPOSCHANGED() MFCQT_ON_WM(WM_WINDOWPOSCHANGED, OnWindowPosChanged)
#define ON_WM_WINDOWPOSCHANGING() MFCQT_ON_WM(WM_WINDOWPOSCHANGING, OnWindowPosChanging)
#define ON_WM_PARENTNOTIFY() MFCQT_ON_WM(WM_PARENTNOTIFY, OnParentNotify)
#define ON_WM_ENABLE() MFCQT_ON_WM(WM_ENABLE, OnEnable)
#define ON_WM_MEASUREITEM() MFCQT_ON_WM(WM_MEASUREITEM, OnMeasureItem)
#define ON_WM_ENDSESSION() MFCQT_ON_WM(WM_ENDSESSION, OnEndSession)
// Registered messages: function-local message maps are built on first use,
// after the message variables got their values.
#define ON_REGISTERED_MESSAGE(nMessageVariable, memberFxn) \
	{ (UINT)(nMessageVariable), 0, 0, 0, AfxSig_message, ::mfcqt::msgmap::message(memberFxn) },

// Looks up the first entry in a class's map chain that matches a message,
// notification code and control/command ID. Used by the Qt dispatcher.
const AFX_MSGMAP_ENTRY *AfxFindMessageEntry(const AFX_MSGMAP *map, UINT message, UINT code, UINT id);
