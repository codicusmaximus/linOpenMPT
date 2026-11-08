// linOpenMPT compatibility layer: MFC message maps.
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

struct AFX_MSGMAP
{
	const AFX_MSGMAP *(*pfnGetBaseMap)();
	const AFX_MSGMAP_ENTRY *lpEntries;
};

namespace linopenmpt::msgmap
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
	{ WM_COMMAND, CN_COMMAND, (UINT)(WORD)(id), (UINT)(WORD)(id), AfxSig_cmd, ::linopenmpt::msgmap::cmd(memberFxn) },
#define ON_COMMAND_EX(id, memberFxn) \
	{ WM_COMMAND, CN_COMMAND, (UINT)(WORD)(id), (UINT)(WORD)(id), AfxSig_cmd_ex, ::linopenmpt::msgmap::cmd_ex(memberFxn) },
#define ON_COMMAND_RANGE(id, idLast, memberFxn) \
	{ WM_COMMAND, CN_COMMAND, (UINT)(WORD)(id), (UINT)(WORD)(idLast), AfxSig_cmd_range, ::linopenmpt::msgmap::cmd_range(memberFxn) },
#define ON_UPDATE_COMMAND_UI(id, memberFxn) \
	{ WM_COMMAND, CN_UPDATE_COMMAND_UI, (UINT)(WORD)(id), (UINT)(WORD)(id), AfxSig_cmd_ui, ::linopenmpt::msgmap::cmd_ui(memberFxn) },
#define ON_UPDATE_COMMAND_UI_RANGE(id, idLast, memberFxn) \
	{ WM_COMMAND, CN_UPDATE_COMMAND_UI, (UINT)(WORD)(id), (UINT)(WORD)(idLast), AfxSig_cmd_ui, ::linopenmpt::msgmap::cmd_ui(memberFxn) },

// ---- control notifications (WM_COMMAND from a child control) -------------
#define ON_CONTROL(code, id, memberFxn) \
	{ WM_COMMAND, (UINT)(WORD)(code), (UINT)(WORD)(id), (UINT)(WORD)(id), AfxSig_cmd, ::linopenmpt::msgmap::cmd(memberFxn) },
#define ON_CONTROL_RANGE(code, id, idLast, memberFxn) \
	{ WM_COMMAND, (UINT)(WORD)(code), (UINT)(WORD)(id), (UINT)(WORD)(idLast), AfxSig_cmd_range, ::linopenmpt::msgmap::cmd_range(memberFxn) },
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
	{ WM_NOTIFY, (UINT)(WORD)(int)(code), (UINT)(WORD)(id), (UINT)(WORD)(id), AfxSig_notify, ::linopenmpt::msgmap::notify(memberFxn) },
#define ON_NOTIFY_EX(code, id, memberFxn) \
	{ WM_NOTIFY, (UINT)(WORD)(int)(code), (UINT)(WORD)(id), (UINT)(WORD)(id), AfxSig_notify_ex, ::linopenmpt::msgmap::notify_ex(memberFxn) },
#define ON_NOTIFY_EX_RANGE(code, id, idLast, memberFxn) \
	{ WM_NOTIFY, (UINT)(WORD)(int)(code), (UINT)(WORD)(id), (UINT)(WORD)(idLast), AfxSig_notify_ex, ::linopenmpt::msgmap::notify_ex(memberFxn) },
#define ON_NOTIFY_REFLECT(code, memberFxn) \
	{ WM_NOTIFY + WM_REFLECT_BASE, (UINT)(WORD)(int)(code), 0, 0, AfxSig_notify, ::linopenmpt::msgmap::notify(memberFxn) },

// ---- user and registered messages ---------------------------------------
#define ON_MESSAGE(message, memberFxn) \
	{ (UINT)(message), 0, 0, 0, AfxSig_message, ::linopenmpt::msgmap::message(memberFxn) },
#define ON_THREAD_MESSAGE(message, memberFxn) ON_MESSAGE(message, memberFxn)

// ---- standard window messages --------------------------------------------
// The handler name is fixed per message, exactly as in MFC.
#define LINOPENMPT_ON_WM(msg, handler) \
	{ (UINT)(msg), 0, 0, 0, AfxSig_wm, ::linopenmpt::msgmap::wm(&ThisClass::handler) },

#define ON_WM_ACTIVATE() LINOPENMPT_ON_WM(WM_ACTIVATE, OnActivate)
#define ON_WM_ACTIVATEAPP() LINOPENMPT_ON_WM(WM_ACTIVATEAPP, OnActivateApp)
#define ON_WM_CHAR() LINOPENMPT_ON_WM(WM_CHAR, OnChar)
#define ON_WM_CLOSE() LINOPENMPT_ON_WM(WM_CLOSE, OnClose)
#define ON_WM_CONTEXTMENU() LINOPENMPT_ON_WM(WM_CONTEXTMENU, OnContextMenu)
#define ON_WM_CREATE() LINOPENMPT_ON_WM(WM_CREATE, OnCreate)
#define ON_WM_CTLCOLOR() LINOPENMPT_ON_WM(WM_CTLCOLOR, OnCtlColor)
#define ON_WM_CTLCOLOR_REFLECT() LINOPENMPT_ON_WM(WM_CTLCOLOR + WM_REFLECT_BASE, CtlColor)
#define ON_WM_DESTROY() LINOPENMPT_ON_WM(WM_DESTROY, OnDestroy)
#define ON_WM_DEVICECHANGE() LINOPENMPT_ON_WM(WM_DEVICECHANGE, OnDeviceChange)
#define ON_WM_DRAWITEM() LINOPENMPT_ON_WM(WM_DRAWITEM, OnDrawItem)
#define ON_WM_DROPFILES() LINOPENMPT_ON_WM(WM_DROPFILES, OnDropFiles)
#define ON_WM_ERASEBKGND() LINOPENMPT_ON_WM(WM_ERASEBKGND, OnEraseBkgnd)
#define ON_WM_GETDLGCODE() LINOPENMPT_ON_WM(WM_GETDLGCODE, OnGetDlgCode)
#define ON_WM_GETMINMAXINFO() LINOPENMPT_ON_WM(WM_GETMINMAXINFO, OnGetMinMaxInfo)
#define ON_WM_HSCROLL() LINOPENMPT_ON_WM(WM_HSCROLL, OnHScroll)
#define ON_WM_INITMENU() LINOPENMPT_ON_WM(WM_INITMENU, OnInitMenu)
#define ON_WM_INITMENUPOPUP() LINOPENMPT_ON_WM(WM_INITMENUPOPUP, OnInitMenuPopup)
#define ON_WM_KEYDOWN() LINOPENMPT_ON_WM(WM_KEYDOWN, OnKeyDown)
#define ON_WM_KEYUP() LINOPENMPT_ON_WM(WM_KEYUP, OnKeyUp)
#define ON_WM_KILLFOCUS() LINOPENMPT_ON_WM(WM_KILLFOCUS, OnKillFocus)
#define ON_WM_LBUTTONDBLCLK() LINOPENMPT_ON_WM(WM_LBUTTONDBLCLK, OnLButtonDblClk)
#define ON_WM_LBUTTONDOWN() LINOPENMPT_ON_WM(WM_LBUTTONDOWN, OnLButtonDown)
#define ON_WM_LBUTTONUP() LINOPENMPT_ON_WM(WM_LBUTTONUP, OnLButtonUp)
#define ON_WM_MBUTTONDBLCLK() LINOPENMPT_ON_WM(WM_MBUTTONDBLCLK, OnMButtonDblClk)
#define ON_WM_MBUTTONDOWN() LINOPENMPT_ON_WM(WM_MBUTTONDOWN, OnMButtonDown)
#define ON_WM_MBUTTONUP() LINOPENMPT_ON_WM(WM_MBUTTONUP, OnMButtonUp)
#define ON_WM_MDIACTIVATE() LINOPENMPT_ON_WM(WM_MDIACTIVATE, OnMDIActivate)
#define ON_WM_MENUSELECT() LINOPENMPT_ON_WM(WM_MENUSELECT, OnMenuSelect)
#define ON_WM_MOUSEACTIVATE() LINOPENMPT_ON_WM(WM_MOUSEACTIVATE, OnMouseActivate)
#define ON_WM_MOUSEHOVER() LINOPENMPT_ON_WM(WM_MOUSEHOVER, OnMouseHover)
#define ON_WM_MOUSEHWHEEL() LINOPENMPT_ON_WM(WM_MOUSEHWHEEL, OnMouseHWheel)
#define ON_WM_MOUSELEAVE() LINOPENMPT_ON_WM(WM_MOUSELEAVE, OnMouseLeave)
#define ON_WM_MOUSEMOVE() LINOPENMPT_ON_WM(WM_MOUSEMOVE, OnMouseMove)
#define ON_WM_MOUSEWHEEL() LINOPENMPT_ON_WM(WM_MOUSEWHEEL, OnMouseWheel)
#define ON_WM_MOVE() LINOPENMPT_ON_WM(WM_MOVE, OnMove)
#define ON_WM_NCACTIVATE() LINOPENMPT_ON_WM(WM_NCACTIVATE, OnNcActivate)
#define ON_WM_NCCALCSIZE() LINOPENMPT_ON_WM(WM_NCCALCSIZE, OnNcCalcSize)
#define ON_WM_NCHITTEST() LINOPENMPT_ON_WM(WM_NCHITTEST, OnNcHitTest)
#define ON_WM_NCLBUTTONDBLCLK() LINOPENMPT_ON_WM(WM_NCLBUTTONDBLCLK, OnNcLButtonDblClk)
#define ON_WM_NCLBUTTONDOWN() LINOPENMPT_ON_WM(WM_NCLBUTTONDOWN, OnNcLButtonDown)
#define ON_WM_NCLBUTTONUP() LINOPENMPT_ON_WM(WM_NCLBUTTONUP, OnNcLButtonUp)
#define ON_WM_NCMOUSEMOVE() LINOPENMPT_ON_WM(WM_NCMOUSEMOVE, OnNcMouseMove)
#define ON_WM_NCPAINT() LINOPENMPT_ON_WM(WM_NCPAINT, OnNcPaint)
#define ON_WM_PAINT() LINOPENMPT_ON_WM(WM_PAINT, OnPaint)
#define ON_WM_QUERYENDSESSION() LINOPENMPT_ON_WM(WM_QUERYENDSESSION, OnQueryEndSession)
#define ON_WM_RBUTTONDBLCLK() LINOPENMPT_ON_WM(WM_RBUTTONDBLCLK, OnRButtonDblClk)
#define ON_WM_RBUTTONDOWN() LINOPENMPT_ON_WM(WM_RBUTTONDOWN, OnRButtonDown)
#define ON_WM_RBUTTONUP() LINOPENMPT_ON_WM(WM_RBUTTONUP, OnRButtonUp)
#define ON_WM_SETCURSOR() LINOPENMPT_ON_WM(WM_SETCURSOR, OnSetCursor)
#define ON_WM_SETFOCUS() LINOPENMPT_ON_WM(WM_SETFOCUS, OnSetFocus)
#define ON_WM_SHOWWINDOW() LINOPENMPT_ON_WM(WM_SHOWWINDOW, OnShowWindow)
#define ON_WM_SIZE() LINOPENMPT_ON_WM(WM_SIZE, OnSize)
#define ON_WM_SYSCOMMAND() LINOPENMPT_ON_WM(WM_SYSCOMMAND, OnSysCommand)
#define ON_WM_TIMER() LINOPENMPT_ON_WM(WM_TIMER, OnTimer)
#define ON_WM_VSCROLL() LINOPENMPT_ON_WM(WM_VSCROLL, OnVScroll)
#define ON_WM_XBUTTONDOWN() LINOPENMPT_ON_WM(WM_XBUTTONDOWN, OnXButtonDown)
#define ON_WM_XBUTTONUP() LINOPENMPT_ON_WM(WM_XBUTTONUP, OnXButtonUp)

// Looks up the first entry in a class's map chain that matches a message,
// notification code and control/command ID. Used by the Qt dispatcher.
const AFX_MSGMAP_ENTRY *AfxFindMessageEntry(const AFX_MSGMAP *map, UINT message, UINT code, UINT id);
