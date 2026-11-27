/*
 * FileDialog.cpp
 * --------------
 * Purpose: File and folder selection dialogs implementation.
 * Notes  : mfcless twin of upstream's mptrack/FileDialog.cpp, on QFileDialog.
 *          The filter string keeps upstream's "description|patterns|...||"
 *          form and its 1-based filter index. Qt's own dialog is used (not
 *          the desktop's), as only it tells which file is selected, for the
 *          instrument preview. The default folders of the path settings are
 *          the sidebar's places, as upstream adds them to the Vista dialog.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "FileDialog.h"
#include "mfcless/QtString.h"
#include "InputHandler.h"
#include "Mainfrm.h"
#include "Mptrack.h"
#include "TrackerSettings.h"
#include "mpt/fs/fs.hpp"

#include <QtCore/QDir>
#include <QtCore/QUrl>
#include <QtWidgets/QFileDialog>


OPENMPT_NAMESPACE_BEGIN


namespace
{

QString ToQPath(const mpt::PathString &path)
{
	return mfcless::ToQString(path.ToUnicode());
}

mpt::PathString FromQPath(const QString &path)
{
	return mpt::PathString::FromUnicode(mfcless::FromQString(QDir::toNativeSeparators(path)));
}


// Upstream's "description|patterns|description|patterns||" as Qt's
// "description (patterns)" list.
QStringList ParseFilter(const mpt::ustring &filter)
{
	QStringList result;
	const QStringList parts = mfcless::ToQString(filter).split(u'|');
	for(qsizetype i = 0; i + 1 < parts.size(); i += 2)
	{
		const QString &description = parts[i];
		if(description.isEmpty())
			break;
		QString patterns = parts[i + 1];
		patterns.replace(u';', u' ');
		patterns = patterns.simplified();
		// Qt reads the patterns from the last parentheses of the text.
		result.push_back(description + QStringLiteral(" (") + patterns + u')');
	}
	return result;
}


void AddPlaces(QFileDialog &dlg, const std::vector<mpt::PathString> &extraPlaces)
{
	QList<QUrl> urls = dlg.sidebarUrls();
	const auto settingPlaces =
	{
		&TrackerSettings::Instance().PathPluginPresets,
		&TrackerSettings::Instance().PathPlugins,
		&TrackerSettings::Instance().PathSamples,
		&TrackerSettings::Instance().PathInstruments,
		&TrackerSettings::Instance().PathSongs,
	};
	std::vector<mpt::PathString> places;
	for(const auto place : settingPlaces)
		places.push_back(place->GetDefaultDir());
	places.insert(places.end(), extraPlaces.begin(), extraPlaces.end());
	// Upstream adds each at the top.
	for(const auto &place : places)
	{
		if(place.empty() || !mpt::native_fs{}.is_directory(place))
			continue;
		const QUrl url = QUrl::fromLocalFile(ToQPath(place));
		if(!urls.contains(url))
			urls.prepend(url);
	}
	dlg.setSidebarUrls(urls);
}

} // namespace


// Display the file dialog.
bool FileDialog::Show(QWidget *parent)
{
	m_filenames.clear();

	QFileDialog dlg(parent != nullptr ? parent : static_cast<QWidget *>(CMainFrame::GetMainFrame()));
	dlg.setOption(QFileDialog::DontUseNativeDialog);
	dlg.setAcceptMode(m_load ? QFileDialog::AcceptOpen : QFileDialog::AcceptSave);
	dlg.setFileMode(m_load ? (m_multiSelect ? QFileDialog::ExistingFiles : QFileDialog::ExistingFile) : QFileDialog::AnyFile);
	if(!m_defaultExtension.empty())
		dlg.setDefaultSuffix(mfcless::ToQString(m_defaultExtension));

	const QStringList filters = ParseFilter(m_extFilter);
	if(!filters.isEmpty())
	{
		dlg.setNameFilters(filters);
		// 1-based, 0 for the first one
		const int index = m_filterIndex != nullptr ? *m_filterIndex : 0;
		if(index >= 1 && index <= filters.size())
			dlg.selectNameFilter(filters[index - 1]);
	}
	if(!m_workingDirectory.empty())
		dlg.setDirectory(ToQPath(m_workingDirectory));
	if(!m_defaultFilename.empty())
		dlg.selectFile(mfcless::ToQString(m_defaultFilename));
	AddPlaces(dlg, m_places);

	// Instrument preview
	bool played = false;
	if(m_preview && TrackerSettings::Instance().previewInFileDialogs)
	{
		QObject::connect(&dlg, &QFileDialog::currentChanged, &dlg, [&](const QString &name) {
			if(name.isEmpty() || QFileInfo(name).isDir())
				return;
			const mpt::PathString path = FromQPath(name);
			if(path != m_lastPreviewFile)
			{
				m_lastPreviewFile = path;
				if(CMainFrame::GetMainFrame()->PlaySoundFile(path, NOTE_MIDDLEC))
					played = true;
			}
		});
	}

	// Do it!
	BypassInputHandler bih;
	const bool ok = dlg.exec() == QDialog::Accepted;
	if(played)
		CMainFrame::GetMainFrame()->StopPreview();
	if(!ok)
		return false;

	// Retrieve variables
	if(m_filterIndex != nullptr && !filters.isEmpty())
		*m_filterIndex = static_cast<int>(filters.indexOf(dlg.selectedNameFilter())) + 1;

	for(const QString &file : dlg.selectedFiles())
	{
		m_filenames.push_back(FromQPath(file));
		if(!m_multiSelect)
			break;
	}

	if(m_filenames.empty())
	{
		return false;
	}

	m_workingDirectory = m_filenames.front().GetDirectoryWithDrive();
	m_extension = m_filenames.front().GetFilenameExtension().ToUnicode();
	if(!m_extension.empty())
		m_extension.erase(0, 1);

	return true;
}


// Display the folder dialog.
bool BrowseForFolder::Show(QWidget *parent)
{
	BypassInputHandler bih;

	QFileDialog dlg(parent != nullptr ? parent : static_cast<QWidget *>(CMainFrame::GetMainFrame()), mfcless::ToQString(m_caption));
	dlg.setOption(QFileDialog::DontUseNativeDialog);
	dlg.setFileMode(QFileDialog::Directory);
	dlg.setOption(QFileDialog::ShowDirsOnly);
	if(!m_workingDirectory.empty())
		dlg.setDirectory(ToQPath(m_workingDirectory));
	AddPlaces(dlg, m_places);
	if(dlg.exec() != QDialog::Accepted || dlg.selectedFiles().isEmpty())
		return false;
	m_workingDirectory = FromQPath(dlg.selectedFiles().front());
	return true;
}


OPENMPT_NAMESPACE_END
