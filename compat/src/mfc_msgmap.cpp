// linOpenMPT compatibility layer: message map lookup.

#include <afxwin.h>

const AFX_MSGMAP *CCmdTarget::GetThisMessageMap()
{
	static const AFX_MSGMAP_ENTRY entries[] = { { 0, 0, 0, 0, AfxSig_end, nullptr } };
	static const AFX_MSGMAP map = { nullptr, &entries[0] };
	return &map;
}

const AFX_MSGMAP_ENTRY *AfxFindMessageEntry(const AFX_MSGMAP *map, UINT message, UINT code, UINT id)
{
	for(; map != nullptr; map = map->pfnGetBaseMap ? map->pfnGetBaseMap() : nullptr)
	{
		for(const AFX_MSGMAP_ENTRY *e = map->lpEntries; e->nSig != AfxSig_end; ++e)
		{
			if(e->nMessage == message && e->nCode == code && id >= e->nID && id <= e->nLastID)
				return e;
		}
	}
	return nullptr;
}

// Calls a handler through the type-erased pointer, as MFC does. This relies
// on CCmdTarget being at offset 0 of the handler's class (it is the primary
// base of every MFC class), which is the same assumption MFC makes.
//
// code is a WM_COMMAND notification code (CN_COMMAND for menu commands), or
// MAKELONG(code, message) for WM_NOTIFY and reflected messages, as in MFC.
BOOL CCmdTarget::OnCmdMsg(UINT id, int code, void *extra, void *handlerInfo)
{
	(void)handlerInfo;
	UINT message = WM_COMMAND;
	UINT notifyCode = static_cast<UINT>(code);
	if(notifyCode != static_cast<UINT>(CN_UPDATE_COMMAND_UI))
	{
		if(HIWORD(notifyCode))
			message = HIWORD(notifyCode);
		notifyCode = LOWORD(notifyCode);
	}
	for(const AFX_MSGMAP *map = GetMessageMap(); map != nullptr; map = map->pfnGetBaseMap ? map->pfnGetBaseMap() : nullptr)
	{
		for(const AFX_MSGMAP_ENTRY *e = map->lpEntries; e->nSig != AfxSig_end; ++e)
		{
			if(e->nMessage != message || e->nCode != notifyCode || id < e->nID || id > e->nLastID)
				continue;
			switch(e->nSig)
			{
			case AfxSig_cmd:
				(this->*e->pfn)();
				return TRUE;
			case AfxSig_cmd_ex:
				// FALSE passes the command on to later handlers.
				if((this->*reinterpret_cast<BOOL (CCmdTarget::*)(UINT)>(e->pfn))(id))
					return TRUE;
				continue;
			case AfxSig_cmd_range:
				(this->*reinterpret_cast<void (CCmdTarget::*)(UINT)>(e->pfn))(id);
				return TRUE;
			case AfxSig_cmd_ui:
				(this->*reinterpret_cast<void (CCmdTarget::*)(CCmdUI *)>(e->pfn))(static_cast<CCmdUI *>(extra));
				return TRUE;
			case AfxSig_notify:
				{
					auto *notify = static_cast<AFX_NOTIFY *>(extra);
					if(notify == nullptr)
						return FALSE;
					(this->*reinterpret_cast<void (CCmdTarget::*)(NMHDR *, LRESULT *)>(e->pfn))(notify->pNMHDR, notify->pResult);
					return TRUE;
				}
			case AfxSig_notify_ex:
				{
					auto *notify = static_cast<AFX_NOTIFY *>(extra);
					if(notify == nullptr)
						return FALSE;
					if((this->*reinterpret_cast<BOOL (CCmdTarget::*)(UINT, NMHDR *, LRESULT *)>(e->pfn))(id, notify->pNMHDR, notify->pResult))
						return TRUE;
					continue;
				}
			default:
				return FALSE;
			}
		}
	}
	return FALSE;
}
