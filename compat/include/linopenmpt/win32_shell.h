// linOpenMPT compatibility layer: COM basics, the Windows shell (file
// operations, folder browser, shell links, ShellExecute), time and file
// search, threads, languages, and the taskbar progress interface, with
// Windows' values.
//
// Declared only, apart from the COM interface definitions; the platform
// layer implements them with XDG/Qt equivalents in plan step 3.

#pragma once

#include "win32_user.h"

// ---- COM -----------------------------------------------------------------
struct IUnknown
{
	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject) = 0;
	virtual ULONG STDMETHODCALLTYPE AddRef() = 0;
	virtual ULONG STDMETHODCALLTYPE Release() = 0;
	virtual ~IUnknown() = default;
};
#define CLSCTX_INPROC_SERVER 0x1
#define CLSCTX_INPROC_HANDLER 0x2
#define CLSCTX_LOCAL_SERVER 0x4
#define CLSCTX_REMOTE_SERVER 0x10
#define CLSCTX_ALL (CLSCTX_INPROC_SERVER | CLSCTX_INPROC_HANDLER | CLSCTX_LOCAL_SERVER | CLSCTX_REMOTE_SERVER)
#define COINIT_APARTMENTTHREADED 0x2
#define COINIT_MULTITHREADED 0x0
// Returns E_NOTIMPL-style failure on Linux: there are no COM servers.
HRESULT CoCreateInstance(REFCLSID clsid, IUnknown *outer, DWORD context, REFIID iid, LPVOID *object);
HRESULT CoInitializeEx(LPVOID reserved, DWORD flags);
void CoUninitialize();
void CoTaskMemFree(LPVOID mem);
extern const CLSID CLSID_ShellLink;
extern const CLSID CLSID_FolderShortcut;
extern const CLSID CLSID_TaskbarList;
extern const IID IID_IShellLink;
extern const IID IID_IPersistFile;
extern const IID IID_ITaskbarList3;
extern const IID IID_IUnknown;

#define STGM_READ 0x00000000L
#define STGM_WRITE 0x00000001L
#define STGM_READWRITE 0x00000002L
struct IPersistFile : public IUnknown
{
	virtual HRESULT STDMETHODCALLTYPE Load(LPCWSTR fileName, DWORD mode) = 0;
	virtual HRESULT STDMETHODCALLTYPE Save(LPCWSTR fileName, BOOL remember) = 0;
};
// Windows shell links (.lnk files). No COM server creates them on Linux, so
// upstream's LinkResolver finds no links.
#define SLR_NO_UI 0x0001
#define SLR_ANY_MATCH 0x0002
#define SLR_UPDATE 0x0004
#define SLR_NOUPDATE 0x0008
#define SLR_NOSEARCH 0x0010
#define SLR_NOTRACK 0x0020
struct IShellLink : public IUnknown
{
	virtual HRESULT STDMETHODCALLTYPE SetPath(LPCWSTR file) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetDescription(LPCWSTR name) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetPath(LPWSTR file, int size, struct WIN32_FIND_DATA *data, DWORD flags) = 0;
	virtual HRESULT STDMETHODCALLTYPE Resolve(HWND hwnd, DWORD flags) = 0;
};
using IShellLinkW = IShellLink;

// Windows taskbar progress.
enum TBPFLAG
{
	TBPF_NOPROGRESS = 0,
	TBPF_INDETERMINATE = 0x1,
	TBPF_NORMAL = 0x2,
	TBPF_ERROR = 0x4,
	TBPF_PAUSED = 0x8,
};
struct ITaskbarList3 : public IUnknown
{
	virtual HRESULT STDMETHODCALLTYPE SetProgressValue(HWND hwnd, ULONGLONG completed, ULONGLONG total) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetProgressState(HWND hwnd, TBPFLAG flags) = 0;
};

// ---- time ----------------------------------------------------------------
// FILETIME is in win32_types.h.
union ULARGE_INTEGER
{
	struct { DWORD LowPart; DWORD HighPart; };
	struct { DWORD LowPart; DWORD HighPart; } u;
	ULONGLONG QuadPart;
};
union LARGE_INTEGER
{
	struct { DWORD LowPart; LONG HighPart; };
	struct { DWORD LowPart; LONG HighPart; } u;
	LONGLONG QuadPart;
};
struct SYSTEMTIME { WORD wYear; WORD wMonth; WORD wDayOfWeek; WORD wDay; WORD wHour; WORD wMinute; WORD wSecond; WORD wMilliseconds; };
void GetSystemTimeAsFileTime(LPFILETIME time);
void GetLocalTime(SYSTEMTIME *time);
void GetSystemTime(SYSTEMTIME *time);
BOOL FileTimeToSystemTime(const FILETIME *fileTime, SYSTEMTIME *systemTime);

// ---- file search ---------------------------------------------------------
#ifndef MAX_PATH
#define MAX_PATH 260
#endif
struct WIN32_FIND_DATA
{
	DWORD dwFileAttributes; FILETIME ftCreationTime; FILETIME ftLastAccessTime; FILETIME ftLastWriteTime;
	DWORD nFileSizeHigh; DWORD nFileSizeLow; DWORD dwReserved0; DWORD dwReserved1;
	TCHAR cFileName[MAX_PATH]; TCHAR cAlternateFileName[14];
};
using WIN32_FIND_DATAW = WIN32_FIND_DATA;
using LPWIN32_FIND_DATA = WIN32_FIND_DATA *;
// Paths are native (narrow); found names are wide, as with UNICODE.
HANDLE FindFirstFile(const char *pattern, LPWIN32_FIND_DATA data);
BOOL FindNextFile(HANDLE find, LPWIN32_FIND_DATA data);
BOOL FindClose(HANDLE find);
DWORD GetFileAttributes(const char *path);
#define INVALID_FILE_ATTRIBUTES ((DWORD)-1)

// ---- shell -------------------------------------------------------------------
#define FO_MOVE 0x0001
#define FO_COPY 0x0002
#define FO_DELETE 0x0003
#define FO_RENAME 0x0004
#define FOF_MULTIDESTFILES 0x0001
#define FOF_SILENT 0x0004
#define FOF_RENAMEONCOLLISION 0x0008
#define FOF_NOCONFIRMATION 0x0010
#define FOF_ALLOWUNDO 0x0040
#define FOF_FILESONLY 0x0080
#define FOF_SIMPLEPROGRESS 0x0100
#define FOF_NOCONFIRMMKDIR 0x0200
#define FOF_NOERRORUI 0x0400
#define FOF_NO_UI (FOF_SILENT | FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_NOCONFIRMMKDIR)
struct SHFILEOPSTRUCT
{
	HWND hwnd; UINT wFunc; LPCTSTR pFrom; LPCTSTR pTo; WORD fFlags; BOOL fAnyOperationsAborted; LPVOID hNameMappings; LPCTSTR lpszProgressTitle;
};
// pFrom is a double-null-terminated list of wide paths. FO_DELETE with
// FOF_ALLOWUNDO moves to the XDG trash.
int SHFileOperation(SHFILEOPSTRUCT *op);

struct ITEMIDLIST;
using LPITEMIDLIST = ITEMIDLIST *;
using PIDLIST_ABSOLUTE = ITEMIDLIST *;
using LPCITEMIDLIST = const ITEMIDLIST *;
using BFFCALLBACK = int (*)(HWND hwnd, UINT msg, LPARAM lParam, LPARAM data);
#define BIF_RETURNONLYFSDIRS 0x00000001
#define BIF_DONTGOBELOWDOMAIN 0x00000002
#define BIF_EDITBOX 0x00000010
#define BIF_NEWDIALOGSTYLE 0x00000040
#define BIF_USENEWUI (BIF_NEWDIALOGSTYLE | BIF_EDITBOX)
#define BFFM_INITIALIZED 1
#define BFFM_SELCHANGED 2
#define BFFM_SETSELECTIONW (WM_USER + 103)
#define BFFM_SETSELECTION BFFM_SETSELECTIONW
struct BROWSEINFO
{
	HWND hwndOwner; LPCITEMIDLIST pidlRoot; LPTSTR pszDisplayName; LPCTSTR lpszTitle; UINT ulFlags; BFFCALLBACK lpfn; LPARAM lParam; int iImage;
};
LPITEMIDLIST SHBrowseForFolder(BROWSEINFO *info);
BOOL SHGetPathFromIDList(LPCITEMIDLIST list, LPTSTR path);

// File icons and types.
#define SHGFI_ICON 0x000000100
#define SHGFI_DISPLAYNAME 0x000000200
#define SHGFI_TYPENAME 0x000000400
#define SHGFI_SYSICONINDEX 0x000004000
#define SHGFI_LARGEICON 0x000000000
#define SHGFI_SMALLICON 0x000000001
#define SHGFI_USEFILEATTRIBUTES 0x000000010
#define SHGFI_ATTRIBUTES 0x000000800
#define SFGAO_LINK 0x00010000L
#define SFGAO_FOLDER 0x20000000L
struct SHFILEINFO
{
	HICON hIcon;
	int iIcon;
	DWORD dwAttributes;
	TCHAR szDisplayName[MAX_PATH];
	TCHAR szTypeName[80];
};
DWORD_PTR SHGetFileInfo(LPCWSTR path, DWORD fileAttributes, SHFILEINFO *info, UINT size, UINT flags);

#define SW_SHOWNORMAL 1
HINSTANCE ShellExecute(HWND hwnd, LPCTSTR operation, LPCTSTR file, LPCTSTR parameters, LPCTSTR directory, INT showCmd);
BOOL PathIsRelative(const char *path);
BOOL PathIsRelative(LPCWSTR path);
// Wildcard match of a file name against a ;-separated pattern list.
BOOL PathMatchSpec(LPCWSTR file, LPCWSTR spec);

// ---- windows that adjust to their frames ----------------------------------
BOOL AdjustWindowRectEx(LPRECT rect, DWORD style, BOOL menu, DWORD exStyle);
BOOL AdjustWindowRect(LPRECT rect, DWORD style, BOOL menu);
BOOL AllowSetForegroundWindow(DWORD processID);
#define ASFW_ANY ((DWORD)-1)

// ---- threads -----------------------------------------------------------------
#define THREAD_PRIORITY_IDLE (-15)
#define THREAD_PRIORITY_LOWEST (-2)
#define THREAD_PRIORITY_BELOW_NORMAL (-1)
#define THREAD_PRIORITY_NORMAL 0
#define THREAD_PRIORITY_ABOVE_NORMAL 1
#define THREAD_PRIORITY_HIGHEST 2
#define THREAD_PRIORITY_TIME_CRITICAL 15
HANDLE GetCurrentThread();
BOOL SetThreadPriority(HANDLE thread, int priority);
DWORD GetCurrentProcessId();

// ---- languages ---------------------------------------------------------------
#define MAKELANGID(p, s) ((((WORD)(s)) << 10) | (WORD)(p))
#define PRIMARYLANGID(lgid) ((WORD)(lgid) & 0x3ff)
#define SUBLANGID(lgid) ((WORD)(lgid) >> 10)
#define LANG_NEUTRAL 0x00
#define LANG_CHINESE 0x04
#define LANG_CZECH 0x05
#define LANG_DANISH 0x06
#define LANG_GERMAN 0x07
#define LANG_GREEK 0x08
#define LANG_ENGLISH 0x09
#define LANG_SPANISH 0x0a
#define LANG_FINNISH 0x0b
#define LANG_FRENCH 0x0c
#define LANG_HUNGARIAN 0x0e
#define LANG_ITALIAN 0x10
#define LANG_JAPANESE 0x11
#define LANG_KOREAN 0x12
#define LANG_DUTCH 0x13
#define LANG_NORWEGIAN 0x14
#define LANG_POLISH 0x15
#define LANG_PORTUGUESE 0x16
#define LANG_RUSSIAN 0x19
#define LANG_SWEDISH 0x1d
#define LANG_TURKISH 0x1f
#define SUBLANG_NEUTRAL 0x00
#define SUBLANG_DEFAULT 0x01
#define SUBLANG_ENGLISH_US 0x01
#define SUBLANG_ENGLISH_UK 0x02
#define SUBLANG_SPANISH 0x01
#define SUBLANG_SPANISH_MODERN 0x03
LANGID GetUserDefaultUILanguage();
LANGID GetUserDefaultLangID();

// ---- winmm ---------------------------------------------------------------
#define MIDI_MAPPER ((UINT)-1)

// ---- wide and narrow variants upstream calls explicitly ---------------------
int DrawTextW(HDC dc, LPCWSTR text, int count, LPRECT rect, UINT format);
int DrawTextA(HDC dc, LPCSTR text, int count, LPRECT rect, UINT format);
BOOL AppendMenuW(HMENU menu, UINT flags, UINT_PTR id, LPCWSTR text);
UINT DragQueryFileW(HDROP drop, UINT index, LPWSTR file, UINT size);

// MSVC's secure fcvt, on glibc's fcvt_r. Returns 0 on success.
inline int _fcvt_s(char *buffer, size_t size, double value, int count, int *dec, int *sign)
{
	return ::fcvt_r(value, count, dec, sign, buffer, size) == 0 ? 0 : EINVAL;
}
template <size_t size>
inline int _fcvt_s(char (&buffer)[size], double value, int count, int *dec, int *sign)
{
	return _fcvt_s(buffer, size, value, count, dec, sign);
}
