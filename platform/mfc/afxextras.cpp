// linOpenMPT MFC layer: the smaller pieces InitInstance and the tracker's
// dialogs use: the common colour and font dialogs, hot key names, OLE drop
// targets (files dropped on the tree view), the restart manager's data
// recovery, the clipboard, and BSTRs.
#include "MfcInternal.h"

#include "QtWin.h"

#include <afxdatarecovery.h>
#include <afxole.h>

#include <QtGui/QFont>
#include <QtWidgets/QFontDialog>

#include <cstdlib>
#include <cwchar>

// ---- special window positions (SetWindowPos) ------------------------------------------

const CWnd CWnd::wndTop(HWND_TOP);
const CWnd CWnd::wndBottom(HWND_BOTTOM);
const CWnd CWnd::wndTopMost(HWND_TOPMOST);

BOOL CWnd::OpenClipboard()
{
	return ::OpenClipboard(m_hWnd);
}

// ---- BSTR ------------------------------------------------------------------------------

template <>
BSTR CStringT<wchar_t>::AllocSysString() const
{
	return ::SysAllocString(GetString());
}

// ---- the colour dialog -----------------------------------------------------------------

CColorDialog::CColorDialog(COLORREF init, DWORD flags, CWnd *parent)
{
	m_cc.lStructSize = sizeof(m_cc);
	m_cc.hwndOwner = parent != nullptr ? parent->GetSafeHwnd() : nullptr;
	m_cc.rgbResult = init;
	m_cc.Flags = flags | CC_ENABLEHOOK;
	if(init != 0)
		m_cc.Flags |= CC_RGBINIT;
	m_cc.lpCustColors = GetSavedCustomColors();
}

INT_PTR CColorDialog::DoModal()
{
	if(m_cc.hwndOwner == nullptr && AfxGetMainWnd() != nullptr)
		m_cc.hwndOwner = AfxGetMainWnd()->GetSafeHwnd();
	return ::ChooseColor(&m_cc) ? IDOK : IDCANCEL;
}

COLORREF CColorDialog::GetColor() const
{
	return m_cc.rgbResult;
}

void CColorDialog::SetCurrentColor(COLORREF color)
{
	m_cc.rgbResult = color;
}

COLORREF *CColorDialog::GetSavedCustomColors()
{
	// Shared by all colour dialogs while the program runs, as in MFC.
	static COLORREF colors[16] = {
		RGB(255, 255, 255), RGB(255, 255, 255), RGB(255, 255, 255), RGB(255, 255, 255),
		RGB(255, 255, 255), RGB(255, 255, 255), RGB(255, 255, 255), RGB(255, 255, 255),
		RGB(255, 255, 255), RGB(255, 255, 255), RGB(255, 255, 255), RGB(255, 255, 255),
		RGB(255, 255, 255), RGB(255, 255, 255), RGB(255, 255, 255), RGB(255, 255, 255)};
	return colors;
}

// ---- the font dialog -------------------------------------------------------------------

namespace
{

int ScreenDpi()
{
	HDC dc = ::GetDC(nullptr);
	const int dpi = ::GetDeviceCaps(dc, LOGPIXELSY);
	::ReleaseDC(nullptr, dc);
	return dpi > 0 ? dpi : 96;
}

} // namespace

CFontDialog::CFontDialog(const LOGFONT *initial, DWORD flags, CDC *printerDC, CWnd *parent)
{
	m_cf.lStructSize = sizeof(m_cf);
	m_cf.hwndOwner = parent != nullptr ? parent->GetSafeHwnd() : nullptr;
	m_cf.hDC = printerDC != nullptr ? printerDC->GetSafeHdc() : nullptr;
	m_cf.Flags = flags | CF_ENABLEHOOK;
	if(initial != nullptr)
	{
		m_lf = *initial;
		m_cf.Flags |= CF_INITTOLOGFONTSTRUCT;
	}
	m_cf.lpLogFont = &m_lf;
}

INT_PTR CFontDialog::DoModal()
{
	LOGFONT &lf = m_cf.lpLogFont != nullptr ? *m_cf.lpLogFont : m_lf;
	const int dpi = ScreenDpi();
	QFont initial;
	if(m_cf.Flags & CF_INITTOLOGFONTSTRUCT)
	{
		if(lf.lfFaceName[0])
			initial.setFamily(QString::fromWCharArray(lf.lfFaceName));
		// A negative height is the character height in pixels, a positive
		// one the cell height; both are close enough to the em size here.
		const int pixels = lf.lfHeight < 0 ? -lf.lfHeight : lf.lfHeight;
		if(pixels > 0)
			initial.setPointSizeF(pixels * 72.0 / dpi);
		initial.setWeight(lf.lfWeight >= 600 ? QFont::Bold : QFont::Normal);
		initial.setItalic(lf.lfItalic != 0);
		initial.setUnderline(lf.lfUnderline != 0);
		initial.setStrikeOut(lf.lfStrikeOut != 0);
	}
	QWidget *parent = m_cf.hwndOwner != nullptr ? linopenmpt::qtwin::FromHandle(m_cf.hwndOwner) : nullptr;
	QFontDialog::FontDialogOptions options;
	if(m_cf.Flags & CF_FIXEDPITCHONLY)
		options |= QFontDialog::MonospacedFonts;
	if(!(m_cf.Flags & CF_EFFECTS))
		options |= QFontDialog::NoButtons & QFontDialog::FontDialogOptions();
	bool ok = false;
	const QFont chosen = QFontDialog::getFont(&ok, initial, parent, QString(), options);
	if(!ok)
		return IDCANCEL;
	lf = LOGFONT{};
	const std::wstring face = chosen.family().toStdWString();
	std::wcsncpy(lf.lfFaceName, face.c_str(), LF_FACESIZE - 1);
	const double points = chosen.pointSizeF() > 0 ? chosen.pointSizeF() : chosen.pixelSize() * 72.0 / dpi;
	lf.lfHeight = -static_cast<LONG>(points * dpi / 72.0 + 0.5);
	lf.lfWeight = chosen.bold() ? FW_BOLD : FW_NORMAL;
	lf.lfItalic = chosen.italic() ? TRUE : FALSE;
	lf.lfUnderline = chosen.underline() ? TRUE : FALSE;
	lf.lfStrikeOut = chosen.strikeOut() ? TRUE : FALSE;
	lf.lfCharSet = DEFAULT_CHARSET;
	m_cf.iPointSize = static_cast<INT>(points * 10.0 + 0.5);
	if(&lf != &m_lf)
		m_lf = lf;
	return IDOK;
}

CString CFontDialog::GetFaceName() const { return CString(m_cf.lpLogFont != nullptr ? m_cf.lpLogFont->lfFaceName : m_lf.lfFaceName); }
CString CFontDialog::GetStyleName() const { return CString(m_cf.lpszStyle != nullptr ? m_cf.lpszStyle : _T("")); }
int CFontDialog::GetSize() const { return m_cf.iPointSize; }
COLORREF CFontDialog::GetColor() const { return m_cf.rgbColors; }
int CFontDialog::GetWeight() const { return static_cast<int>(m_lf.lfWeight); }
BOOL CFontDialog::IsStrikeOut() const { return m_lf.lfStrikeOut ? TRUE : FALSE; }
BOOL CFontDialog::IsUnderline() const { return m_lf.lfUnderline ? TRUE : FALSE; }
BOOL CFontDialog::IsBold() const { return m_lf.lfWeight >= FW_SEMIBOLD ? TRUE : FALSE; }
BOOL CFontDialog::IsItalic() const { return m_lf.lfItalic ? TRUE : FALSE; }
void CFontDialog::GetCurrentFont(LOGFONT *font)
{
	if(font != nullptr)
		*font = m_cf.lpLogFont != nullptr ? *m_cf.lpLogFont : m_lf;
}

// ---- hot keys --------------------------------------------------------------------------

CString CHotKeyCtrl::GetKeyName(UINT virtualKey, BOOL extended)
{
	// As MFC: the key's scan code, and the keyboard's name for it.
	const LONG scanCode = static_cast<LONG>(::MapVirtualKey(virtualKey, MAPVK_VK_TO_VSC));
	LONG param = scanCode << 16;
	if(extended)
		param |= 0x01000000;
	TCHAR name[64] = {};
	if(::GetKeyNameText(param, name, 64) <= 0)
		return CString();
	return CString(name);
}

// ---- OLE drag and drop -----------------------------------------------------------------

BOOL COleDataObject::IsDataAvailable(CLIPFORMAT format, LPFORMATETC)
{
	return format == CF_HDROP && !m_files.empty();
}

BOOL COleDataObject::GetData(CLIPFORMAT format, LPSTGMEDIUM medium, LPFORMATETC formatEtc)
{
	if(medium == nullptr)
		return FALSE;
	HGLOBAL global = GetGlobalData(format, formatEtc);
	if(global == nullptr)
		return FALSE;
	medium->tymed = TYMED_HGLOBAL;
	medium->hGlobal = global;
	medium->pUnkForRelease = nullptr;
	return TRUE;
}

HGLOBAL COleDataObject::GetGlobalData(CLIPFORMAT format, LPFORMATETC)
{
	if(!IsDataAvailable(format))
		return nullptr;
	// The drop owns it until the data object is released.
	HDROP drop = linopenmpt::qtwin::CreateDrop(m_files, m_point, true);
	m_drops.push_back(drop);
	return reinterpret_cast<HGLOBAL>(drop);
}

BOOL COleDataObject::AttachClipboard()
{
	// Only dragged files are offered this way here.
	return FALSE;
}

void COleDataObject::Release()
{
	for(HDROP drop : m_drops)
		::DragFinish(drop);
	m_drops.clear();
	m_files.clear();
}

COleDataObject::~COleDataObject()
{
	Release();
}

BOOL COleDropTarget::Register(CWnd *wnd)
{
	if(wnd == nullptr || wnd->m_hWnd == nullptr)
		return FALSE;
	Revoke();
	m_hWnd = wnd->m_hWnd;
	linopenmpt::qtwin::DropTargetCallbacks callbacks;
	callbacks.over = [this](const std::vector<std::wstring> &files, DWORD keys, POINT client, bool enter) -> DWORD {
		CWnd *target = CWnd::FromHandle(m_hWnd);
		COleDataObject data;
		data.m_files = files;
		data.m_point = client;
		const CPoint point(client.x, client.y);
		return enter ? OnDragEnter(target, &data, keys, point) : OnDragOver(target, &data, keys, point);
	};
	callbacks.leave = [this]() {
		OnDragLeave(CWnd::FromHandle(m_hWnd));
	};
	callbacks.drop = [this](const std::vector<std::wstring> &files, DWORD effect, POINT client) -> BOOL {
		CWnd *target = CWnd::FromHandle(m_hWnd);
		COleDataObject data;
		data.m_files = files;
		data.m_point = client;
		const CPoint point(client.x, client.y);
		// OnDropEx first, then OnDrop, as MFC.
		const DROPEFFECT result = OnDropEx(target, &data, effect, effect, point);
		if(result != static_cast<DROPEFFECT>(-1))
			return result != DROPEFFECT_NONE;
		return OnDrop(target, &data, effect, point);
	};
	linopenmpt::qtwin::RegisterDropTarget(m_hWnd, std::move(callbacks));
	return TRUE;
}

void COleDropTarget::Revoke()
{
	if(m_hWnd != nullptr)
		linopenmpt::qtwin::RevokeDropTarget(m_hWnd);
	m_hWnd = nullptr;
}

COleDropTarget::~COleDropTarget()
{
	Revoke();
}

BOOL AFXAPI AfxOleInit()
{
	// Nothing to start: drag and drop goes through Qt.
	return TRUE;
}

// ---- data recovery ---------------------------------------------------------------------
// The restart manager is Windows'. Without it there is nothing to recover
// from, so the handler does not start, and MFC's callers do without it
// (OpenMPT keeps its own autosave).

BOOL CDataRecoveryHandler::Initialize() { return FALSE; }
BOOL CDataRecoveryHandler::SaveOpenDocumentList() { return FALSE; }
BOOL CDataRecoveryHandler::ReadOpenDocumentList() { return FALSE; }
BOOL CDataRecoveryHandler::ReopenPreviousDocuments() { return FALSE; }
BOOL CDataRecoveryHandler::AutosaveAllDocumentInfo() { return TRUE; }
BOOL CDataRecoveryHandler::RemoveDocumentInfo(CDocument *) { return TRUE; }
