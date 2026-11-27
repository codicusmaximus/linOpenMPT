/*
 * PathConfigDlg.h
 * ---------------
 * Purpose: Default paths and auto save setup dialog.
 * Notes  : mfcless twin of upstream's mptrack/PathConfigDlg.h.
 *          Draft from qt/tools/twin.py, not yet edited by hand.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "DialogBase.h"
#include "AccessibleControls.h"

OPENMPT_NAMESPACE_BEGIN

class PathConfigDlg : public PropertyPage
{
	Q_OBJECT
	Q_OBJECT
public:
	PathConfigDlg();

protected:
	void DoDataExchange();    // DDX/DDV support

	void OnOK() override;
	bool OnInitDialog() override;

	void OnAutosaveEnable();
	void OnAutosaveUseOrigDir();
	void OnAutosaveRetention();
	void OnBrowseAutosavePath();
	void OnBrowseSongs();
	void OnBrowseSamples();
	void OnBrowseInstruments();
	void OnBrowsePlugins();
	void OnBrowsePresets();

	void OnSettingsChanged();
	bool OnSetActive() override;

	void BrowseFolder(uint32 nID);

	mpt::PathString GetPath(int id);
	std::array<AccessibleEdit, 3> m_accessibleEdits;
	std::array<AccessibleButton, 6> m_browseButtons;
};

OPENMPT_NAMESPACE_END
