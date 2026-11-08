// Upstream's resources (mptrack.rc compiled by tools/rc/rc.py and linked in)
// read through the Win32 and MFC resource API, run offscreen.
#include <afxwin.h>

#include "MptrackResources.h"
#include "QtWin.h"
#include "resource.h"

#include <QtWidgets/QApplication>

#include <cstdio>
#include <cstring>

namespace
{
int g_failures = 0;

void Check(bool ok, const char *what)
{
	std::printf("%s %s\n", ok ? "ok  " : "FAIL", what);
	if(!ok)
		g_failures++;
}

class CTestApp : public CWinApp
{
};

} // namespace

int main(int argc, char **argv)
{
	qputenv("QT_QPA_PLATFORM", "offscreen");
	QApplication qapp(argc, argv);
	CTestApp app;
	LinOpenMPTRegisterMptrackResources();
	const HINSTANCE instance = AfxGetResourceHandle();
	Check(instance == ::GetModuleHandle(nullptr), "resources belong to the executable");

	// Strings.
	TCHAR buffer[64] = {};
	Check(::LoadString(instance, IDR_MAINFRAME, buffer, 64) == 7 && std::wcscmp(buffer, L"OpenMPT") == 0, "LoadString");
	Check(::LoadString(instance, IDR_MAINFRAME, buffer, 4) == 3 && std::wcscmp(buffer, L"Ope") == 0, "LoadString truncates");
	CString docString;
	Check(docString.LoadString(IDR_MODULETYPE) && docString.Left(8) == _T("\nModule\n"), "CString::LoadString with escapes");
	CString missing;
	Check(!missing.LoadString(1) && missing.IsEmpty(), "missing string");

	// Raw resources, as upstream's GetResource() reads them.
	HRSRC png = ::FindResource(instance, MAKEINTRESOURCE(IDB_MPTRACK), _T("PNG"));
	Check(png != nullptr && ::SizeofResource(instance, png) > 1000, "FindResource of a PNG");
	const auto *pngData = static_cast<const BYTE *>(::LockResource(::LoadResource(instance, png)));
	Check(pngData != nullptr && std::memcmp(pngData, "\x89PNG", 4) == 0, "LockResource gives the file");
	Check(::FindResource(instance, _T("#304"), _T("png")) == png, "names by #number, types ignore case");
	Check(::FindResource(instance, MAKEINTRESOURCE(IDD_CONTROL_SAMPLES), RT_DIALOG) != nullptr, "dialog template");
	Check(::FindResource(instance, MAKEINTRESOURCE(IDD_TUNING), RT_DIALOG) != nullptr, "dialog from the en-GB section");
	Check(::FindResource(instance, MAKEINTRESOURCE(IDD_MIDI_IO_PLUGIN), RT_DIALOG_LAYOUT) != nullptr, "dynamic layout (AFX_DIALOG_LAYOUT)");
	Check(::FindResource(instance, MAKEINTRESOURCE(IDD_CONTROL_SAMPLES), RT_DLGINIT) != nullptr, "combo box data (DLGINIT)");
	Check(::FindResource(instance, MAKEINTRESOURCE(IDR_MAINFRAME), RT_MENU) != nullptr, "menu template");

	// Bitmaps: 88x117 in res/colors.bmp.
	CBitmap colors;
	BITMAP bm{};
	Check(colors.LoadBitmap(IDB_COLORSETUP) && colors.GetBitmap(&bm) && bm.bmWidth == 88 && bm.bmHeight == 117, "CBitmap::LoadBitmap");

	{
		// Same pixels as the file, top row first.
		const QImage file(QStringLiteral(LINOPENMPT_UPSTREAM_DIR "/mptrack/res/COLORS.BMP"));
		HDC mem = CreateCompatibleDC(nullptr);
		HGDIOBJ old = SelectObject(mem, colors.m_hObject);
		bool same = !file.isNull();
		for(QPoint p : {QPoint(0, 0), QPoint(87, 0), QPoint(10, 100), QPoint(44, 58)})
			same = same && GetPixel(mem, p.x(), p.y()) == linopenmpt::qtwin::ToColorRef(file.pixelColor(p));
		Check(same, "bitmap pixels match the file");
		SelectObject(mem, old);
		DeleteDC(mem);
	}

	// Icons and cursors, at the size the screen shows.
	HICON icon = app.LoadIcon(IDR_MAINFRAME);
	auto *iconData = reinterpret_cast<linopenmpt::qtwin::IconData *>(icon);
	Check(iconData != nullptr && !iconData->image.isNull(), "CWinApp::LoadIcon");
	if(iconData)
	{
		const QSize logical = (QSizeF(iconData->image.size()) / iconData->image.devicePixelRatio()).toSize();
		Check(logical == QSize(GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON)), "icon has the system icon size");
		Check(qAlpha(iconData->image.pixel(iconData->image.width() / 2, iconData->image.height() / 2)) == 255, "icon centre is opaque");
	}
	Check(app.LoadIcon(IDR_MAINFRAME) == icon, "loaded icons are shared");
	HICON small = static_cast<HICON>(::LoadImage(instance, MAKEINTRESOURCE(IDR_MAINFRAME), IMAGE_ICON, 16, 16, 0));
	Check(small != nullptr && small != icon, "LoadImage at 16x16");
	::DestroyIcon(small);

	HCURSOR dragging = app.LoadCursor(IDC_DRAGGING);
	auto *cursorData = reinterpret_cast<linopenmpt::qtwin::IconData *>(dragging);
	Check(cursorData != nullptr && cursorData->isCursor && !cursorData->image.isNull(), "CWinApp::LoadCursor from the resources");
	Check(app.LoadCursor(AFX_IDC_HSPLITBAR) != nullptr, "MFC's splitter cursor");
	Check(app.LoadStandardCursor(IDC_ARROW) == ::LoadCursor(nullptr, IDC_ARROW), "standard cursor");

	// Drawing a loaded icon.
	HDC screen = GetDC(nullptr);
	HDC mem = CreateCompatibleDC(screen);
	HBITMAP target = CreateCompatibleBitmap(screen, 32, 32);
	HGDIOBJ old = SelectObject(mem, target);
	PatBlt(mem, 0, 0, 32, 32, WHITENESS);
	Check(DrawIconEx(mem, 0, 0, icon, 32, 32, 0, nullptr, DI_NORMAL) && GetPixel(mem, 16, 16) != RGB(255, 255, 255), "DrawIconEx draws it");
	SelectObject(mem, old);
	DeleteObject(target);
	DeleteDC(mem);
	ReleaseDC(nullptr, screen);

	std::printf("%s\n", g_failures == 0 ? "all passed" : "FAILURES");
	return g_failures == 0 ? 0 : 1;
}
