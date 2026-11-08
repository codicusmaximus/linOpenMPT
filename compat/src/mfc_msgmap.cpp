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
BOOL CCmdTarget::OnCmdMsg(UINT id, int code, void *extra, void *handlerInfo)
{
	(void)handlerInfo;
	const AFX_MSGMAP_ENTRY *e = AfxFindMessageEntry(GetMessageMap(), WM_COMMAND, static_cast<UINT>(code), id);
	if(e == nullptr)
		return FALSE;
	switch(e->nSig)
	{
	case AfxSig_cmd:
		(this->*e->pfn)();
		return TRUE;
	case AfxSig_cmd_ex:
		return (this->*reinterpret_cast<BOOL (CCmdTarget::*)(UINT)>(e->pfn))(id);
	case AfxSig_cmd_range:
		(this->*reinterpret_cast<void (CCmdTarget::*)(UINT)>(e->pfn))(id);
		return TRUE;
	case AfxSig_cmd_ui:
		(this->*reinterpret_cast<void (CCmdTarget::*)(CCmdUI *)>(e->pfn))(static_cast<CCmdUI *>(extra));
		return TRUE;
	default:
		return FALSE;
	}
}
