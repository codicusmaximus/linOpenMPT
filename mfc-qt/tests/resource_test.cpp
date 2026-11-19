// Resources from a script of the tests' own (resources.rc), compiled and
// linked in by mfcqt_add_resources, read through the Win32 and MFC resource
// API: strings, raw data, menus, accelerators, version information, and a
// dialog template with its controls and combo box data (DLGINIT).
#include <afxwin.h>

#include "resource.h"

#include <QtWidgets/QApplication>

#include <cstdio>
#include <cstring>

void RegisterTestResources();

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

class CSettingsDialog : public CDialog
{
public:
	CSettingsDialog() : CDialog(IDD_SETTINGS) {}
	CString name = _T("Ada");
	BOOL loop = TRUE;
	CString caption, label, mode;
	int modes = 0;

protected:
	void DoDataExchange(CDataExchange *dx) override
	{
		CDialog::DoDataExchange(dx);
		DDX_Text(dx, IDC_NAME, name);
		DDX_Check(dx, IDC_LOOP, loop);
	}
	BOOL OnInitDialog() override
	{
		CDialog::OnInitDialog();
		GetWindowText(caption);
		GetDlgItemText(IDC_NAME_LABEL, label);
		auto *combo = static_cast<CComboBox *>(GetDlgItem(IDC_MODE));
		modes = combo ? combo->GetCount() : -1;
		if(combo && modes > 1)
			combo->GetLBText(1, mode);
		// Typing into the edit, then OK: DDX reads it back.
		SetDlgItemText(IDC_NAME, _T("Grace"));
		CheckDlgButton(IDC_LOOP, BST_UNCHECKED);
		PostMessage(WM_COMMAND, IDOK);
		return TRUE;
	}
};

} // namespace

int main(int argc, char **argv)
{
	QApplication qapp(argc, argv);
	CTestApp app;
	RegisterTestResources();
	const HINSTANCE instance = AfxGetResourceHandle();

	TCHAR buffer[64] = {};
	Check(::LoadString(instance, IDS_HELLO, buffer, 64) == 12 && std::wcscmp(buffer, L"Hello, world") == 0, "LoadString");
	CString escapes;
	Check(escapes.LoadString(IDS_ESCAPES) && escapes == _T("Tab\tand\nnew line"), "CString::LoadString with escapes");

	HRSRC data = ::FindResource(instance, MAKEINTRESOURCE(IDB_DATA), RT_RCDATA);
	Check(data != nullptr && ::SizeofResource(instance, data) == 6
		&& std::memcmp(::LockResource(::LoadResource(instance, data)), "mfc-qt", 6) == 0, "RCDATA from a file");

	HMENU menu = ::LoadMenu(instance, MAKEINTRESOURCE(IDR_MAINFRAME));
	HMENU file = menu ? ::GetSubMenu(menu, 0) : nullptr;
	Check(file != nullptr && ::GetMenuItemCount(file) == 3 && ::GetMenuItemID(file, 2) == ID_FILE_EXIT_TEST, "LoadMenu");
	if(menu)
		::DestroyMenu(menu);

	HACCEL accel = ::LoadAccelerators(instance, MAKEINTRESOURCE(IDR_MAINFRAME));
	ACCEL entries[4] = {};
	Check(accel != nullptr && ::CopyAcceleratorTable(accel, entries, 4) == 1 && entries[0].cmd == ID_FILE_OPEN_TEST && entries[0].key == 'O', "LoadAccelerators");

	Check(::FindResource(instance, MAKEINTRESOURCE(1), RT_VERSION) != nullptr, "VERSIONINFO");

	CSettingsDialog dialog;
	const INT_PTR result = dialog.DoModal();
	Check(result == IDOK, "DoModal of a dialog template");
	Check(dialog.caption == _T("Settings") && dialog.label == _T("&Name:"), "dialog caption and controls");
	Check(dialog.modes == 2 && dialog.mode == _T("Slow"), "combo box filled from DLGINIT");
	Check(dialog.name == _T("Grace") && dialog.loop == FALSE, "DDX reads the controls back on OK");

	std::printf("%d failure(s)\n", g_failures);
	return g_failures == 0 ? 0 : 1;
}
