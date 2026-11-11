// linOpenMPT: upstream's file and folder selection dialogs
// (mptrack/FileDialog.h) on QFileDialog. Upstream's FileDialog.cpp drives
// the Windows common item dialog; this follows it option for option:
// the MFC-style filter string, the filter index in and out, the working
// directory, multiple selection, the tracker's folders as places, and
// playing samples and instruments while they are picked.
#include "stdafx.h"
#include "FileDialog.h"
#include "InputHandler.h"
#include "Mainfrm.h"
#include "Mptrack.h"
#include "TrackerSettings.h"

#include "QtWin.h"

#include <QtCore/QDir>
#include <QtCore/QPointer>
#include <QtCore/QFileInfo>
#include <QtCore/QRegularExpression>
#include <QtCore/QStringList>
#include <QtCore/QUrl>
#include <QtWidgets/QFileDialog>

#include <memory>

OPENMPT_NAMESPACE_BEGIN

namespace
{

QString ToQString(const mpt::PathString &path)
{
	return QString::fromStdWString(path.ToWide());
}

QString ToQString(const mpt::RawPathString &path)
{
	return ToQString(mpt::PathString::FromNative(path));
}

mpt::PathString ToPath(const QString &path)
{
	return mpt::PathString::FromWide(QDir::toNativeSeparators(path).toStdWString());
}

QWidget *ParentWidget(CWnd *parent)
{
	if(parent == nullptr)
		parent = CMainFrame::GetMainFrame();
	if(parent == nullptr || parent->GetSafeHwnd() == nullptr)
		return nullptr;
	return linopenmpt::qtwin::FromHandle(parent->GetSafeHwnd());
}

// "Description|*.a;*.b|All Files (*.*)|*.*||" as Qt name filters:
// "Description (*.a *.b)". A description that already ends in its own
// pattern list in parentheses keeps the text before it.
QStringList NameFilters(const QString &mfcFilter)
{
	QStringList filters;
	const QStringList parts = mfcFilter.split(u'|');
	for(qsizetype i = 0; i + 1 < parts.size(); i += 2)
	{
		QString description = parts[i].trimmed();
		const QString patterns = parts[i + 1].trimmed();
		if(description.isEmpty() && patterns.isEmpty())
			break;
		static const QRegularExpression trailingPatterns(QStringLiteral("\\s*\\([^()]*\\)\\s*$"));
		description.remove(trailingPatterns);
		QStringList list = patterns.split(u';', Qt::SkipEmptyParts);
		for(QString &pattern : list)
		{
			pattern = pattern.trimmed();
			if(pattern == QLatin1String("*.*"))
				pattern = QStringLiteral("*");
		}
		filters << QStringLiteral("%1 (%2)").arg(description, list.join(u' '));
	}
	return filters;
}

// Where a dialog without a working directory starts. Windows' dialog then
// opens the last folder used or the user's documents, never the process's
// current folder (which the tracker sets to its own install folder).
QString StartDirectory(const mpt::RawPathString &workingDirectory)
{
	if(!workingDirectory.empty())
		return ToQString(workingDirectory);
	const mpt::PathString songs = TrackerSettings::Instance().PathSongs.GetDefaultDir();
	if(!songs.empty() && QFileInfo(ToQString(songs)).isDir())
		return ToQString(songs);
	return QDir::homePath();
}

QList<QUrl> Places(const std::vector<mpt::PathString> &extra)
{
	QList<QUrl> urls;
	urls << QUrl::fromLocalFile(QDir::homePath());
	const auto add = [&urls](const mpt::PathString &path) {
		if(path.empty())
			return;
		const QString dir = ToQString(path);
		if(!QFileInfo(dir).isDir())
			return;
		const QUrl url = QUrl::fromLocalFile(QDir::cleanPath(dir));
		if(!urls.contains(url))
			urls << url;
	};
	// As upstream: the tracker's configured folders, then the caller's.
	const TrackerSettings &settings = TrackerSettings::Instance();
	add(settings.PathPluginPresets.GetDefaultDir());
	add(settings.PathPlugins.GetDefaultDir());
	add(settings.PathSamples.GetDefaultDir());
	add(settings.PathInstruments.GetDefaultDir());
	add(settings.PathSongs.GetDefaultDir());
	for(const auto &path : extra)
		add(path);
	return urls;
}

} // namespace

bool FileDialog::Show(CWnd *parent)
{
	m_filenames.clear();

	// On the heap, so that it goes with its parent if that is destroyed
	// while it is open.
	QPointer<QFileDialog> dialogPtr = new QFileDialog(ParentWidget(parent));
	QFileDialog &dialog = *dialogPtr;
	dialog.setAcceptMode(m_load ? QFileDialog::AcceptOpen : QFileDialog::AcceptSave);
	dialog.setFileMode(m_load ? (m_multiSelect ? QFileDialog::ExistingFiles : QFileDialog::ExistingFile) : QFileDialog::AnyFile);
	// Qt's own dialog: it keeps the window inside the application's modal
	// handling, and lets samples play while they are selected.
	dialog.setOption(QFileDialog::DontUseNativeDialog);
	dialog.setSidebarUrls(Places(m_places));

	const QStringList filters = NameFilters(ToQString(m_extFilter));
	if(!filters.isEmpty())
	{
		dialog.setNameFilters(filters);
		// The index is 1-based, as OPENFILENAME's; 0 means the first.
		const int index = m_filterIndex != nullptr ? *m_filterIndex : 0;
		if(index >= 1 && index <= filters.size())
			dialog.selectNameFilter(filters[index - 1]);
	}
	if(!m_defaultExtension.empty())
		dialog.setDefaultSuffix(ToQString(m_defaultExtension));
	dialog.setDirectory(StartDirectory(m_workingDirectory));
	if(!m_defaultFilename.empty())
	{
		const QString name = ToQString(m_defaultFilename);
		// A name with a folder in it picks the folder too, as Windows does.
		if(QFileInfo(name).isAbsolute())
			dialog.selectFile(name);
		else
			dialog.selectFile(QFileInfo(name).fileName());
	}

	bool played = false;
	if(m_preview && TrackerSettings::Instance().previewInFileDialogs)
	{
		QObject::connect(&dialog, &QFileDialog::currentChanged, &dialog, [&played](const QString &path) {
			if(path.isEmpty() || !QFileInfo(path).isFile())
				return;
			if(CMainFrame *mainFrame = CMainFrame::GetMainFrame(); mainFrame != nullptr && mainFrame->PlaySoundFile(ToPath(path), NOTE_MIDDLEC))
				played = true;
		});
	}

	BypassInputHandler bih;
	bool accepted;
	{
		linopenmpt::qtwin::ForeignModalLoop loop;
		accepted = dialogPtr->exec() == QDialog::Accepted;
	}
	if(dialogPtr.isNull())
		return false;
	const std::unique_ptr<QFileDialog> owned(dialogPtr.data());
	if(played)
	{
		if(CMainFrame *mainFrame = CMainFrame::GetMainFrame())
			mainFrame->StopPreview();
	}
	if(!accepted)
		return false;

	if(m_filterIndex != nullptr && !filters.isEmpty())
	{
		const qsizetype index = filters.indexOf(dialog.selectedNameFilter());
		*m_filterIndex = index >= 0 ? static_cast<int>(index + 1) : 1;
	}
	for(const QString &file : dialog.selectedFiles())
		m_filenames.push_back(ToPath(file));
	if(m_filenames.empty())
		return false;

	m_workingDirectory = m_filenames.front().GetDirectoryWithDrive().AsNative();
	m_extension = m_filenames.front().GetFilenameExtension().AsNative();
	if(!m_extension.empty())
		m_extension.erase(0, 1);
	return true;
}

bool BrowseForFolder::Show(CWnd *parent)
{
	QPointer<QFileDialog> dialogPtr = new QFileDialog(ParentWidget(parent));
	QFileDialog &dialog = *dialogPtr;
	dialog.setFileMode(QFileDialog::Directory);
	dialog.setOption(QFileDialog::ShowDirsOnly);
	dialog.setOption(QFileDialog::DontUseNativeDialog);
	dialog.setSidebarUrls(Places(m_places));
	if(!m_caption.IsEmpty())
		dialog.setWindowTitle(QString::fromWCharArray(m_caption.GetString()));
	dialog.setDirectory(StartDirectory(m_workingDirectory.AsNative()));

	BypassInputHandler bih;
	bool accepted;
	{
		linopenmpt::qtwin::ForeignModalLoop loop;
		accepted = dialogPtr->exec() == QDialog::Accepted;
	}
	if(dialogPtr.isNull())
		return false;
	const std::unique_ptr<QFileDialog> owned(dialogPtr.data());
	if(!accepted || dialog.selectedFiles().isEmpty())
		return false;
	m_workingDirectory = ToPath(dialog.selectedFiles().front());
	return true;
}

OPENMPT_NAMESPACE_END
