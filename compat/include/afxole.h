// linOpenMPT compatibility layer: stand-in for MFC <afxole.h>.
// The tracker uses OLE only for dropping files on the tree view
// (View_tre.cpp). The platform layer maps Qt's drag and drop onto these.
#pragma once
#include "afxwin.h"

#include <string>
#include <vector>

using DROPEFFECT = DWORD;
#define DROPEFFECT_NONE 0
#define DROPEFFECT_COPY 1
#define DROPEFFECT_MOVE 2
#define DROPEFFECT_LINK 4
#define DROPEFFECT_SCROLL 0x80000000

using CLIPFORMAT = WORD;
#define TYMED_NULL 0
#define TYMED_HGLOBAL 1
#define TYMED_FILE 2
#define DVASPECT_CONTENT 1
struct DVTARGETDEVICE;
struct FORMATETC
{
	CLIPFORMAT cfFormat;
	DVTARGETDEVICE *ptd;
	DWORD dwAspect;
	LONG lindex;
	DWORD tymed;
};
using LPFORMATETC = FORMATETC *;
struct STGMEDIUM
{
	DWORD tymed;
	union
	{
		HGLOBAL hGlobal;
		LPWSTR lpszFileName;
	};
	IUnknown *pUnkForRelease;
};
using LPSTGMEDIUM = STGMEDIUM *;
void ReleaseStgMedium(LPSTGMEDIUM medium);

// Data being dragged or pasted. For CF_HDROP, hGlobal is an HDROP that
// DragQueryFile() reads.
class COleDataObject
{
public:
	COleDataObject() = default;
	COleDataObject(const COleDataObject &) = delete;
	COleDataObject &operator=(const COleDataObject &) = delete;
	~COleDataObject();
	BOOL IsDataAvailable(CLIPFORMAT format, LPFORMATETC formatEtc = nullptr);
	BOOL GetData(CLIPFORMAT format, LPSTGMEDIUM medium, LPFORMATETC formatEtc = nullptr);
	HGLOBAL GetGlobalData(CLIPFORMAT format, LPFORMATETC formatEtc = nullptr);
	BOOL AttachClipboard();
	void Release();
	// What is being dragged here: files, and where (client coordinates).
	std::vector<std::wstring> m_files;
	POINT m_point{};
	std::vector<HDROP> m_drops;
};

class COleDropTarget : public CCmdTarget
{
public:
	~COleDropTarget() override;
	BOOL Register(CWnd *wnd);
	virtual void Revoke();
	virtual DROPEFFECT OnDragEnter(CWnd *wnd, COleDataObject *dataObject, DWORD keyState, CPoint point) { return DROPEFFECT_NONE; }
	virtual DROPEFFECT OnDragOver(CWnd *wnd, COleDataObject *dataObject, DWORD keyState, CPoint point) { return DROPEFFECT_NONE; }
	virtual void OnDragLeave(CWnd *wnd) { }
	virtual BOOL OnDrop(CWnd *wnd, COleDataObject *dataObject, DROPEFFECT dropEffect, CPoint point) { return FALSE; }
	virtual DROPEFFECT OnDropEx(CWnd *wnd, COleDataObject *dataObject, DROPEFFECT dropDefault, DROPEFFECT dropList, CPoint point) { return static_cast<DROPEFFECT>(-1); }
	virtual DROPEFFECT OnDragScroll(CWnd *wnd, DWORD keyState, CPoint point) { return DROPEFFECT_NONE; }
protected:
	HWND m_hWnd = nullptr;
};

class COleDataSource : public CCmdTarget
{
public:
	void CacheGlobalData(CLIPFORMAT format, HGLOBAL global, LPFORMATETC formatEtc = nullptr);
	DROPEFFECT DoDragDrop(DWORD effects = DROPEFFECT_COPY | DROPEFFECT_MOVE | DROPEFFECT_LINK, LPCRECT startDragRect = nullptr);
	void SetClipboard();
};

BOOL AFXAPI AfxOleInit();
