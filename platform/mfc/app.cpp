// linOpenMPT MFC layer: what CWinApp::InitInstance uses (MFC's appcore.cpp,
// appui.cpp, appui3.cpp and cmdline.cpp): the command line, the shell
// commands it asks for, the profile, and files dropped on the main window.
#include "MfcInternal.h"

#include "QtWin.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QStandardPaths>

#include <cwchar>
#include <string>

// ---- the command line ----------------------------------------------------------------

void CCommandLineInfo::ParseParam(LPCTSTR param, BOOL flag, BOOL last)
{
	if(flag)
		ParseParamFlag(QString::fromWCharArray(param).toUtf8().constData());
	else
		ParseParamNotFlag(param);
	ParseLast(last);
}

void CCommandLineInfo::ParseParamFlag(const char *param)
{
	// Shell commands are case sensitive, OLE switches are not.
	const QString p = QString::fromUtf8(param);
	if(p == QLatin1String("pt"))
		m_nShellCommand = FilePrintTo;
	else if(p == QLatin1String("p"))
		m_nShellCommand = FilePrint;
	else if(p.compare(QLatin1String("Register"), Qt::CaseInsensitive) == 0 || p.compare(QLatin1String("Regserver"), Qt::CaseInsensitive) == 0)
		m_nShellCommand = AppRegister;
	else if(p.compare(QLatin1String("Unregister"), Qt::CaseInsensitive) == 0 || p.compare(QLatin1String("Unregserver"), Qt::CaseInsensitive) == 0)
		m_nShellCommand = AppUnregister;
	else if(p == QLatin1String("dde"))
		m_nShellCommand = FileDDE;
	else if(p.compare(QLatin1String("Embedding"), Qt::CaseInsensitive) == 0)
	{
		m_bRunEmbedded = TRUE;
		m_bShowSplash = FALSE;
	} else if(p.compare(QLatin1String("Automation"), Qt::CaseInsensitive) == 0)
	{
		m_bRunAutomated = TRUE;
		m_bShowSplash = FALSE;
	} else if(p.compare(QLatin1String("RestartByRestartManager"), Qt::CaseInsensitive) == 0)
	{
		m_nShellCommand = RestartByRestartManager;
	}
}

void CCommandLineInfo::ParseParamNotFlag(LPCTSTR param)
{
	if(m_strFileName.IsEmpty())
		m_strFileName = param;
	else if(m_nShellCommand == FilePrintTo && m_strPrinterName.IsEmpty())
		m_strPrinterName = param;
	else if(m_nShellCommand == FilePrintTo && m_strDriverName.IsEmpty())
		m_strDriverName = param;
	else if(m_nShellCommand == FilePrintTo && m_strPortName.IsEmpty())
		m_strPortName = param;
	else if(m_nShellCommand == RestartByRestartManager && m_strRestartIdentifier.IsEmpty())
		m_strRestartIdentifier = param;
}

void CCommandLineInfo::ParseLast(BOOL last)
{
	if(last)
	{
		if(m_nShellCommand == FileNew && !m_strFileName.IsEmpty())
			m_nShellCommand = FileOpen;
		m_bShowSplash = !m_bRunEmbedded && !m_bRunAutomated;
	}
}

void CWinApp::ParseCommandLine(CCommandLineInfo &info)
{
	// The arguments after the program name. Flags start with '-'; on
	// Windows '/' does too, but here that is how absolute paths start.
	const QStringList args = QCoreApplication::instance() != nullptr ? QCoreApplication::arguments() : QStringList();
	for(int i = 1; i < args.size(); i++)
	{
		std::wstring param = linopenmpt::qtwin::ToWString(args[i]);
		const BOOL flag = !param.empty() && param[0] == L'-';
		const BOOL last = i + 1 == args.size();
		info.ParseParam(param.c_str() + (flag ? 1 : 0), flag, last);
	}
}

BOOL CWinApp::ProcessShellCommand(CCommandLineInfo &info)
{
	BOOL result = TRUE;
	switch(info.m_nShellCommand)
	{
	case CCommandLineInfo::FileNew:
		if(!OnCmdMsg(ID_FILE_NEW, 0, nullptr, nullptr))
			OnFileNew();
		if(m_pMainWnd == nullptr)
			result = FALSE;
		break;
	case CCommandLineInfo::FileOpen:
		if(!OpenDocumentFile(info.m_strFileName))
			result = FALSE;
		break;
	case CCommandLineInfo::FilePrintTo:
	case CCommandLineInfo::FilePrint:
		// No printing from the command line here.
		m_nCmdShow = SW_HIDE;
		result = FALSE;
		break;
	case CCommandLineInfo::FileDDE:
		m_nCmdShow = SW_HIDE;
		break;
	case CCommandLineInfo::AppRegister:
	case CCommandLineInfo::AppUnregister:
		// There is no registration to do; that is all that was asked.
		result = FALSE;
		break;
	case CCommandLineInfo::RestartByRestartManager:
		// No restart manager to restore files from.
		result = FALSE;
		break;
	case CCommandLineInfo::FileNothing:
		break;
	}
	return result;
}

void CWinApp::EnableShellOpen()
{
	// DDE opens files in a running instance on Windows; the desktop opens
	// them through the command line here.
}

void CWnd::DragAcceptFiles(BOOL accept)
{
	::DragAcceptFiles(m_hWnd, accept);
}

// ---- the profile ---------------------------------------------------------------------

namespace
{

// The ini file of the profile: m_pszProfileName, or with SetRegistryKey()
// (the registry on Windows) <config>/<key>/<profile>.ini.
std::string ProfilePath(const CWinApp *app)
{
	if(app->m_pszProfileName == nullptr)
		return std::string();
	QString path = QString::fromWCharArray(app->m_pszProfileName);
	if(app->m_pszRegistryKey != nullptr)
	{
		const QString dir = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QLatin1Char('/') + QString::fromWCharArray(app->m_pszRegistryKey);
		QDir().mkpath(dir);
		path = dir + QLatin1Char('/') + path + QStringLiteral(".ini");
	}
	return path.toStdString();
}

} // namespace

void CWinApp::SetRegistryKey(LPCTSTR key)
{
	m_pszRegistryKey = key != nullptr ? _wcsdup(key) : nullptr;
	// The profile is then named after the application.
	if(m_pszProfileName == nullptr && m_pszAppName != nullptr)
		m_pszProfileName = _wcsdup(m_pszAppName);
}

UINT CWinApp::GetProfileInt(LPCTSTR section, LPCTSTR entry, int defaultValue)
{
	const std::string path = ProfilePath(this);
	if(path.empty())
		return static_cast<UINT>(defaultValue);
	return ::GetPrivateProfileInt(section, entry, defaultValue, path.c_str());
}

BOOL CWinApp::WriteProfileInt(LPCTSTR section, LPCTSTR entry, int value)
{
	const std::string path = ProfilePath(this);
	if(path.empty())
		return FALSE;
	const std::wstring text = std::to_wstring(value);
	return ::WritePrivateProfileString(section, entry, text.c_str(), path.c_str());
}

CString CWinApp::GetProfileString(LPCTSTR section, LPCTSTR entry, LPCTSTR defaultValue)
{
	const std::string path = ProfilePath(this);
	if(path.empty())
		return CString(defaultValue != nullptr ? defaultValue : _T(""));
	std::wstring buffer(4096, L'\0');
	const DWORD length = ::GetPrivateProfileString(section, entry, defaultValue != nullptr ? defaultValue : L"", buffer.data(), static_cast<DWORD>(buffer.size()), path.c_str());
	buffer.resize(length);
	return CString(buffer.c_str());
}

BOOL CWinApp::WriteProfileString(LPCTSTR section, LPCTSTR entry, LPCTSTR value)
{
	const std::string path = ProfilePath(this);
	if(path.empty())
		return FALSE;
	// A null entry removes the section, a null value the entry.
	return ::WritePrivateProfileString(section, entry, value, path.c_str());
}

void CWinApp::LoadStdProfileSettings(UINT maxMRU)
{
	// The recent file list, as MFC keeps it ("Recent File List", File1...).
	m_nMaxMRU = maxMRU;
	m_recentFiles.clear();
	for(UINT i = 1; i <= maxMRU; i++)
	{
		CString entry;
		entry.Format(_T("File%u"), i);
		const CString file = GetProfileString(_T("Recent File List"), entry);
		if(!file.IsEmpty())
			m_recentFiles.push_back(file);
	}
}
