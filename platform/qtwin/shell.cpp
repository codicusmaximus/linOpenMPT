// The clipboard, the shell (opening files and URLs, file operations, file
// icons), help, locale-aware string comparison, the colour chooser, and the
// few COM pieces OpenMPT uses (memory streams for images; there are no COM
// servers, so CoCreateInstance fails and callers take their fallbacks).
#include "QtWin.h"

#include <QtCore/QPointer>
#include <memory>

#include "HtmlHelp.h"
#include "Msctf.h"
#include "atlbase.h"
#include "uxtheme.h"

#include <QtCore/QCollator>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QLocale>
#include <QtCore/QMimeData>
#include <QtCore/QProcess>
#include <QtCore/QUrl>
#include <QtGui/QClipboard>
#include <QtGui/QDesktopServices>
#include <QtGui/QGuiApplication>
#include <QtWidgets/QColorDialog>
#include <QtWidgets/QFileIconProvider>

#include <atomic>
#include <cstring>
#include <map>
#include <string>
#include <vector>

using namespace linopenmpt::qtwin;

// ---- COM -----------------------------------------------------------------------

namespace
{

constexpr HRESULT ClassNotRegistered = static_cast<HRESULT>(0x80040154L); // REGDB_E_CLASSNOTREG

// A stream over a global memory block (or its own copy of some bytes).
class MemoryStream final : public IStream
{
public:
	MemoryStream(HGLOBAL global, bool deleteOnRelease) : m_global(global), m_delete(deleteOnRelease) { }
	explicit MemoryStream(std::vector<BYTE> bytes) : m_bytes(std::move(bytes)) { }
	~MemoryStream() override
	{
		if(m_delete && m_global != nullptr)
			::GlobalFree(m_global);
	}
	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **object) override
	{
		if(object == nullptr)
			return E_POINTER;
		if(std::memcmp(&riid, &IID_IUnknown, sizeof(IID)) == 0)
		{
			*object = static_cast<IUnknown *>(this);
			AddRef();
			return S_OK;
		}
		*object = nullptr;
		return E_NOINTERFACE;
	}
	ULONG STDMETHODCALLTYPE AddRef() override { return ++m_refs; }
	ULONG STDMETHODCALLTYPE Release() override
	{
		const ULONG refs = --m_refs;
		if(refs == 0)
			delete this;
		return refs;
	}
	HRESULT STDMETHODCALLTYPE Read(void *data, ULONG size, ULONG *read) override
	{
		const ULONG left = Size() - std::min(m_pos, Size());
		const ULONG n = std::min(size, left);
		if(n > 0)
			std::memcpy(data, Data() + m_pos, n);
		m_pos += n;
		if(read)
			*read = n;
		return n == size ? S_OK : S_FALSE;
	}
	const BYTE *Data() const override
	{
		return m_global != nullptr ? static_cast<const BYTE *>(::GlobalLock(m_global)) : m_bytes.data();
	}
	ULONG Size() const override
	{
		return m_global != nullptr ? static_cast<ULONG>(::GlobalSize(m_global)) : static_cast<ULONG>(m_bytes.size());
	}

private:
	HGLOBAL m_global = nullptr;
	bool m_delete = false;
	std::vector<BYTE> m_bytes;
	ULONG m_pos = 0;
	std::atomic<ULONG> m_refs{1};
};

} // namespace

// The GUIDs of the interfaces and classes OpenMPT asks for, with Windows'
// values, so that comparisons work even though nothing implements them.
const IID IID_IUnknown = {0x00000000, 0x0000, 0x0000, {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};
const IID IID_IPersistFile = {0x0000010B, 0x0000, 0x0000, {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};
const IID IID_IShellLink = {0x000214F9, 0x0000, 0x0000, {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};
const IID IID_ITaskbarList3 = {0xEA1AFB91, 0x9E28, 0x4B86, {0x90, 0xE9, 0x9E, 0x9F, 0x8A, 0x5E, 0xEF, 0xAF}};
const CLSID CLSID_ShellLink = {0x00021401, 0x0000, 0x0000, {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};
const CLSID CLSID_FolderShortcut = {0x0AFACED1, 0xE828, 0x11D1, {0x91, 0x87, 0xB5, 0x32, 0xF1, 0xE9, 0x57, 0x5D}};
const CLSID CLSID_TaskbarList = {0x56FDF344, 0xFD6D, 0x11D0, {0x95, 0x8A, 0x00, 0x60, 0x97, 0xC9, 0xA0, 0x90}};
const CLSID CLSID_TF_InputProcessorProfiles = {0x33C53A50, 0xF456, 0x4884, {0xB0, 0x49, 0x85, 0xFD, 0x64, 0x3E, 0xCF, 0xED}};
const IID IID_ITfInputProcessorProfiles = {0x1F02B6C5, 0x7842, 0x4EE6, {0x8A, 0x0B, 0x9A, 0x24, 0x18, 0x3A, 0x95, 0xCA}};
const IID IID_ITfSource = {0x4EA48A35, 0x60AE, 0x446F, {0x8F, 0xD6, 0xE6, 0xA8, 0xD8, 0x24, 0x59, 0xF7}};
const IID IID_ITfLanguageProfileNotifySink = {0x43C9FE15, 0xF494, 0x4C17, {0x9D, 0xE2, 0xB8, 0xA4, 0xAC, 0x35, 0x0A, 0xA8}};

HRESULT CoCreateInstance(REFCLSID, IUnknown *, DWORD, REFIID, LPVOID *object)
{
	if(object)
		*object = nullptr;
	return ClassNotRegistered;
}

HRESULT CreateStreamOnHGlobal(HGLOBAL global, BOOL deleteOnRelease, IStream **stream)
{
	if(stream == nullptr)
		return E_INVALIDARG;
	if(global == nullptr)
		global = ::GlobalAlloc(GMEM_MOVEABLE, 0);
	*stream = new MemoryStream(global, deleteOnRelease != FALSE);
	return S_OK;
}

IStream *SHCreateMemStream(const BYTE *data, UINT size)
{
	return new MemoryStream(std::vector<BYTE>(data, data + (data != nullptr ? size : 0)));
}

// ---- the clipboard ---------------------------------------------------------------
// Between OpenClipboard and CloseClipboard, what is set collects in one
// QMimeData, which goes to the system clipboard on closing. Data read from
// the clipboard stays owned by it until it is closed, as on Windows.

namespace
{

struct ClipboardState
{
	bool open = false;
	HWND owner = nullptr;
	QMimeData *pending = nullptr; // after EmptyClipboard
	std::vector<HGLOBAL> handed;   // GetClipboardData results, freed on close
	std::map<UINT, std::wstring> names;
	UINT nextFormat = 0xC000;
};

// Formats are registered from static initialisers (upstream's global
// CLIPFORMAT constants), so the state is made on first use.
ClipboardState &Clip()
{
	static ClipboardState state;
	return state;
}

QString MimeTypeOf(UINT format)
{
	auto it = Clip().names.find(format);
	if(it != Clip().names.end())
		return QStringLiteral("application/x-linopenmpt-") + QString::fromStdWString(it->second).toLower().replace(QLatin1Char(' '), QLatin1Char('-'));
	return QStringLiteral("application/x-linopenmpt-format-%1").arg(format);
}

HGLOBAL GlobalFromBytes(const void *data, std::size_t size)
{
	HGLOBAL mem = ::GlobalAlloc(GMEM_MOVEABLE, size);
	if(mem != nullptr && size > 0)
		std::memcpy(::GlobalLock(mem), data, size);
	return mem;
}

const QMimeData *Current()
{
	if(Clip().pending != nullptr)
		return Clip().pending;
	return QGuiApplication::clipboard() != nullptr ? QGuiApplication::clipboard()->mimeData() : nullptr;
}

} // namespace

UINT RegisterClipboardFormat(LPCTSTR name)
{
	if(name == nullptr)
		return 0;
	for(const auto &[format, existing] : Clip().names)
	{
		if(::wcscasecmp(existing.c_str(), name) == 0)
			return format;
	}
	const UINT format = Clip().nextFormat++;
	Clip().names[format] = name;
	return format;
}

BOOL OpenClipboard(HWND owner)
{
	if(Clip().open)
		return FALSE;
	Clip().open = true;
	Clip().owner = owner;
	return TRUE;
}

BOOL EmptyClipboard()
{
	if(!Clip().open)
		return FALSE;
	delete Clip().pending;
	Clip().pending = new QMimeData;
	return TRUE;
}

HANDLE SetClipboardData(UINT format, HANDLE mem)
{
	if(!Clip().open || Clip().pending == nullptr || mem == nullptr)
		return nullptr;
	const char *bytes = static_cast<const char *>(::GlobalLock(mem));
	const std::size_t size = ::GlobalSize(mem);
	if(format == CF_UNICODETEXT)
	{
		const auto *text = reinterpret_cast<const wchar_t *>(bytes);
		std::size_t length = 0;
		while(length < size / sizeof(wchar_t) && text[length] != 0)
			length++;
		Clip().pending->setText(QString::fromWCharArray(text, static_cast<int>(length)));
	} else if(format == CF_TEXT)
	{
		const std::size_t length = ::strnlen(bytes, size);
		Clip().pending->setText(QString::fromLocal8Bit(bytes, static_cast<qsizetype>(length)));
	} else
	{
		Clip().pending->setData(MimeTypeOf(format), QByteArray(bytes, static_cast<qsizetype>(size)));
	}
	// The clipboard owns the memory now.
	::GlobalFree(mem);
	return mem;
}

BOOL IsClipboardFormatAvailable(UINT format)
{
	const QMimeData *mime = Current();
	if(mime == nullptr)
		return FALSE;
	if(format == CF_TEXT || format == CF_UNICODETEXT)
		return mime->hasText();
	if(format == CF_HDROP)
		return mime->hasUrls();
	return mime->hasFormat(MimeTypeOf(format));
}

HANDLE GetClipboardData(UINT format)
{
	if(!Clip().open)
		return nullptr;
	const QMimeData *mime = Current();
	if(mime == nullptr)
		return nullptr;
	HGLOBAL mem = nullptr;
	if(format == CF_UNICODETEXT && mime->hasText())
	{
		// Windows' clipboard text has CR LF line ends.
		const std::wstring text = ToWString(mime->text().replace(QStringLiteral("\r\n"), QStringLiteral("\n")).replace(QLatin1Char('\n'), QStringLiteral("\r\n")));
		mem = GlobalFromBytes(text.c_str(), (text.size() + 1) * sizeof(wchar_t));
	} else if(format == CF_TEXT && mime->hasText())
	{
		const QByteArray text = mime->text().replace(QStringLiteral("\r\n"), QStringLiteral("\n")).replace(QLatin1Char('\n'), QStringLiteral("\r\n")).toLocal8Bit();
		mem = GlobalFromBytes(text.constData(), static_cast<std::size_t>(text.size()) + 1);
	} else if(mime->hasFormat(MimeTypeOf(format)))
	{
		const QByteArray data = mime->data(MimeTypeOf(format));
		mem = GlobalFromBytes(data.constData(), static_cast<std::size_t>(data.size()));
	}
	if(mem != nullptr)
		Clip().handed.push_back(mem);
	return mem;
}

BOOL CloseClipboard()
{
	if(!Clip().open)
		return FALSE;
	if(Clip().pending != nullptr)
	{
		if(QClipboard *clipboard = QGuiApplication::clipboard())
			clipboard->setMimeData(Clip().pending);
		else
			delete Clip().pending;
		Clip().pending = nullptr;
	}
	for(HGLOBAL mem : Clip().handed)
		::GlobalFree(mem);
	Clip().handed.clear();
	Clip().open = false;
	Clip().owner = nullptr;
	return TRUE;
}

// ---- the shell -------------------------------------------------------------------

namespace
{

// A double-null-terminated list of paths.
std::vector<QString> PathList(LPCTSTR list)
{
	std::vector<QString> paths;
	if(list == nullptr)
		return paths;
	for(const wchar_t *p = list; *p; p += std::wcslen(p) + 1)
		paths.push_back(QString::fromWCharArray(p));
	return paths;
}

bool CopyRecursively(const QString &from, const QString &to)
{
	const QFileInfo info(from);
	if(!info.isDir())
		return QFile::copy(from, to);
	if(!QDir().mkpath(to))
		return false;
	const QDir dir(from);
	for(const QString &name : dir.entryList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System))
	{
		if(!CopyRecursively(dir.filePath(name), QDir(to).filePath(name)))
			return false;
	}
	return true;
}

QString TargetFor(const QString &source, const QString &to, bool multiple)
{
	// Into a folder, unless one destination is given per source.
	if(!multiple && QFileInfo(to).isDir())
		return QDir(to).filePath(QFileInfo(source).fileName());
	return to;
}

} // namespace

int SHFileOperation(SHFILEOPSTRUCT *op)
{
	if(op == nullptr)
		return ERROR_INVALID_FUNCTION;
	op->fAnyOperationsAborted = FALSE;
	const std::vector<QString> from = PathList(op->pFrom);
	const std::vector<QString> to = PathList(op->pTo);
	const bool multiple = (op->fFlags & FOF_MULTIDESTFILES) != 0;
	for(std::size_t i = 0; i < from.size(); i++)
	{
		const QString &source = from[i];
		const QString dest = to.empty() ? QString() : TargetFor(source, multiple && i < to.size() ? to[i] : to.front(), multiple);
		bool ok = false;
		switch(op->wFunc)
		{
		case FO_DELETE:
			if(op->fFlags & FOF_ALLOWUNDO)
				ok = QFile::moveToTrash(source);
			else if(QFileInfo(source).isDir())
				ok = QDir(source).removeRecursively();
			else
				ok = QFile::remove(source);
			break;
		case FO_COPY:
			ok = !dest.isEmpty() && CopyRecursively(source, dest);
			break;
		case FO_MOVE:
		case FO_RENAME:
			ok = !dest.isEmpty() && (QDir().rename(source, dest) || (op->wFunc == FO_MOVE && CopyRecursively(source, dest) && (QFileInfo(source).isDir() ? QDir(source).removeRecursively() : QFile::remove(source))));
			break;
		default:
			return ERROR_INVALID_FUNCTION;
		}
		if(!ok)
			return ERROR_ACCESS_DENIED;
	}
	return 0;
}

DWORD_PTR SHGetFileInfo(LPCWSTR path, DWORD, SHFILEINFO *info, UINT size, UINT flags)
{
	if(path == nullptr || info == nullptr || size < sizeof(SHFILEINFO))
		return 0;
	const QFileInfo file(QString::fromWCharArray(path));
	if(!(flags & SHGFI_USEFILEATTRIBUTES) && !file.exists())
		return 0;
	if(flags & SHGFI_ATTRIBUTES)
	{
		// Symbolic links are followed by the file system; only Windows
		// shell links (which do not exist here) would be SFGAO_LINK.
		info->dwAttributes = file.isDir() ? SFGAO_FOLDER : 0;
	}
	if(flags & SHGFI_DISPLAYNAME)
	{
		const std::wstring name = ToWString(file.fileName().isEmpty() ? file.filePath() : file.fileName());
		std::wcsncpy(info->szDisplayName, name.c_str(), MAX_PATH - 1);
	}
	if(flags & SHGFI_TYPENAME)
	{
		const std::wstring type = ToWString(file.isDir() ? QStringLiteral("Folder") : file.suffix().toUpper() + QStringLiteral(" File"));
		std::wcsncpy(info->szTypeName, type.c_str(), 79);
	}
	if(flags & SHGFI_ICON)
	{
		static QFileIconProvider provider;
		const int extent = (flags & SHGFI_SMALLICON) ? ::GetSystemMetrics(SM_CXSMICON) : ::GetSystemMetrics(SM_CXICON);
		const QIcon icon = file.exists() ? provider.icon(file) : provider.icon(file.isDir() ? QAbstractFileIconProvider::Folder : QAbstractFileIconProvider::File);
		auto *data = new IconData;
		data->image = icon.pixmap(extent, extent).toImage().convertToFormat(QImage::Format_ARGB32_Premultiplied);
		info->hIcon = reinterpret_cast<HICON>(data);
		info->iIcon = 0;
	}
	return 1;
}

HINSTANCE ShellExecute(HWND, LPCTSTR operation, LPCTSTR file, LPCTSTR parameters, LPCTSTR directory, INT)
{
	// ShellExecute's results: above 32 for success.
	const auto success = reinterpret_cast<HINSTANCE>(static_cast<INT_PTR>(33));
	const auto notFound = reinterpret_cast<HINSTANCE>(static_cast<INT_PTR>(2)); // SE_ERR_FNF
	if(file == nullptr || *file == 0)
		return notFound;
	const QString target = QString::fromWCharArray(file);
	const QString op = operation != nullptr ? QString::fromWCharArray(operation) : QStringLiteral("open");
	const QString args = parameters != nullptr ? QString::fromWCharArray(parameters) : QString();
	const QFileInfo info(target);
	if(info.isFile() && info.isExecutable() && (op == QLatin1String("open") || op.isEmpty()))
	{
		const QString dir = directory != nullptr ? QString::fromWCharArray(directory) : QString();
		return QProcess::startDetached(target, QProcess::splitCommand(args), dir) ? success : notFound;
	}
	if(op == QLatin1String("explore"))
		return QDesktopServices::openUrl(QUrl::fromLocalFile(info.isDir() ? target : info.absolutePath())) ? success : notFound;
	const QUrl url = info.exists() ? QUrl::fromLocalFile(info.absoluteFilePath()) : QUrl(target);
	if(!url.isValid() || (!info.exists() && url.scheme().isEmpty()))
		return notFound;
	return QDesktopServices::openUrl(url) ? success : notFound;
}

// ---- help --------------------------------------------------------------------------

HWND HtmlHelp(HWND caller, LPCTSTR file, UINT command, DWORD_PTR)
{
	switch(command)
	{
	case HH_INITIALIZE:
	case HH_UNINITIALIZE:
	case HH_CLOSE_ALL:
		return caller != nullptr ? caller : reinterpret_cast<HWND>(1);
	default:
		break;
	}
	// OpenMPT's manual is a compiled HTML help file, which nothing reads
	// here; the same manual is on OpenMPT's wiki. "OpenMPT Manual.chm::/
	// Patterns.html>OpenMPT" opens the wiki's Patterns page.
	QString page;
	if(file != nullptr)
	{
		const QString name = QString::fromWCharArray(file);
		const int start = name.indexOf(QStringLiteral("::/"));
		if(start >= 0)
		{
			page = name.mid(start + 3);
			page = page.left(page.indexOf(QLatin1Char('>')) >= 0 ? page.indexOf(QLatin1Char('>')) : page.size());
			page.remove(QStringLiteral(".html"));
			page.replace(QLatin1Char(' '), QLatin1Char('_'));
		}
	}
	const QUrl url(page.isEmpty() ? QStringLiteral("https://wiki.openmpt.org/Manual") : QStringLiteral("https://wiki.openmpt.org/Manual:_") + page);
	return QDesktopServices::openUrl(url) ? (caller != nullptr ? caller : reinterpret_cast<HWND>(1)) : nullptr;
}

HRESULT EnableThemeDialogTexture(HWND, DWORD)
{
	// Dialog backgrounds already follow the Qt style.
	return S_OK;
}

// ---- locale ------------------------------------------------------------------------

int CompareString(LCID, DWORD flags, LPCWSTR a, int lenA, LPCWSTR b, int lenB)
{
	if(a == nullptr || b == nullptr)
	{
		::SetLastError(ERROR_INVALID_PARAMETER);
		return 0;
	}
	QCollator collator;
	collator.setCaseSensitivity((flags & NORM_IGNORECASE) ? Qt::CaseInsensitive : Qt::CaseSensitive);
	collator.setNumericMode((flags & SORT_DIGITSASNUMBERS) != 0);
	collator.setIgnorePunctuation((flags & NORM_IGNORESYMBOLS) != 0);
	const int result = collator.compare(ToQString(a, lenA), ToQString(b, lenB));
	return result < 0 ? CSTR_LESS_THAN : (result > 0 ? CSTR_GREATER_THAN : CSTR_EQUAL);
}

int GetLocaleInfo(LCID, DWORD type, LPTSTR data, int size)
{
	const QLocale locale;
	QString value;
	switch(type & 0xFFFF)
	{
	case LOCALE_SDECIMAL: value = locale.decimalPoint(); break;
	case LOCALE_SNEGATIVESIGN: value = locale.negativeSign(); break;
	default:
		::SetLastError(ERROR_INVALID_FLAGS);
		return 0;
	}
	const std::wstring text = ToWString(value);
	if(data == nullptr || size == 0)
		return static_cast<int>(text.size() + 1);
	if(size < static_cast<int>(text.size() + 1))
	{
		::SetLastError(ERROR_INSUFFICIENT_BUFFER);
		return 0;
	}
	std::wmemcpy(data, text.c_str(), text.size() + 1);
	return static_cast<int>(text.size() + 1);
}

// ---- the colour chooser ------------------------------------------------------------

BOOL ChooseColor(LPCHOOSECOLOR cc)
{
	if(cc == nullptr)
		return FALSE;
	QWidget *parent = cc->hwndOwner != nullptr ? FromHandle(cc->hwndOwner) : nullptr;
	// On the heap, so that it goes with its owner if that is destroyed.
	QPointer<QColorDialog> dialogPtr = new QColorDialog(ToQColor(cc->rgbResult), parent);
	QColorDialog &dialog = *dialogPtr;
	// The 16 custom colours go both ways.
	if(cc->lpCustColors != nullptr)
	{
		for(int i = 0; i < 16 && i < QColorDialog::customCount(); i++)
			QColorDialog::setCustomColor(i, ToQColor(cc->lpCustColors[i]));
	}
	if(cc->Flags & CC_RGBINIT)
		dialog.setCurrentColor(ToQColor(cc->rgbResult));
	const bool ok = dialogPtr->exec() == QDialog::Accepted;
	if(dialogPtr.isNull())
		return FALSE;
	const std::unique_ptr<QColorDialog> owned(dialogPtr.data());
	if(cc->lpCustColors != nullptr)
	{
		for(int i = 0; i < 16 && i < QColorDialog::customCount(); i++)
			cc->lpCustColors[i] = ToColorRef(QColorDialog::customColor(i));
	}
	if(ok)
		cc->rgbResult = ToColorRef(dialog.currentColor());
	return ok ? TRUE : FALSE;
}
