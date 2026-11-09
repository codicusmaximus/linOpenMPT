// linOpenMPT MFC layer: internals shared by its source files.
//
// The MFC classes declared in compat/ (afxwin.h and friends) are implemented
// here on the Win32 API only, as MFC is implemented on Windows; the Win32 API
// itself comes from platform/qtwin.
#pragma once

#include <afxwin.h>

#include <memory>

namespace linopenmpt::mfc
{

struct ThreadState
{
	// The message being processed (CWnd::GetCurrentMessage, Default()).
	MSG lastSent{};
	// The object being attached to the window under creation.
	CWnd *wndInit = nullptr;
	HHOOK cbtHook = nullptr;
	// Notifications from this window's controls are ignored (UpdateData).
	HWND lockoutNotifyWindow = nullptr;
};
ThreadState &State();

// Attaches wnd to the next window created on this thread (MFC's
// AfxHookWindowCreate); Unhook returns false if no window was created.
void HookWindowCreate(CWnd *wnd);
bool UnhookWindowCreate();

// MFC's PreTranslateMessage() runs for messages while a loop of its own
// (CWinThread::Run, CWnd::RunModalLoop) is running.
void PushPreTranslate();
void PopPreTranslate();

// Temporary objects for handles without a permanent one, deleted at idle.
void DeleteTempWindowMap();
void DeleteTempGdiMaps();
void DetachFromMaps(HWND hwnd);

} // namespace linopenmpt::mfc

LRESULT AfxCallWndProc(CWnd *wnd, HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
LRESULT CALLBACK AfxWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
