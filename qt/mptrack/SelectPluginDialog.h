/*
 * SelectPluginDialog.h
 * --------------------
 * Purpose: Dialog for adding plugins to a song.
 * Notes  : mfcless twin of upstream's mptrack/SelectPluginDialog.h.
 *          Draft from qt/tools/twin.py, not yet edited by hand.
 * Authors: Olivier Lapicque
 *          OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "mfcless/Keyboard.h"
#include "ResizableDialog.h"
#include "../common/ComponentManager.h"
#include "../soundlib/Snd_defs.h"

class QAbstractButton;
#include "mfcless/TreeCtrl.h"
class QTreeWidgetItem;
class QWidget;

OPENMPT_NAMESPACE_BEGIN

class CModDoc;
struct SNDMIXPLUGIN;
struct VSTPluginLib;
class ComponentPluginBridge_x86;
class ComponentPluginBridgeLegacy_x86;
class ComponentPluginBridge_amd64;
class ComponentPluginBridgeLegacy_amd64;
#if defined(MPT_WITH_WINDOWS10)
class ComponentPluginBridge_arm;
class ComponentPluginBridgeLegacy_arm;
class ComponentPluginBridge_arm64;
class ComponentPluginBridgeLegacy_arm64;
#endif  // MPT_WITH_WINDOWS10

class CSelectPluginDlg : public ResizableDialog
{
	Q_OBJECT
protected:
	SNDMIXPLUGIN *m_pPlugin = nullptr;
	CModDoc *m_pModDoc = nullptr;
	mfcless::TreeCtrl *m_treePlugins = nullptr;
	QAbstractButton *m_chkBridge = nullptr;
	QAbstractButton *m_chkShare = nullptr;
	QAbstractButton *m_chkLegacyBridge = nullptr;
	mpt::ustring m_nameFilter;
#ifdef MPT_WITH_VST
	ComponentHandle<ComponentPluginBridge_x86> pluginBridge_x86;
	ComponentHandle<ComponentPluginBridgeLegacy_x86> pluginBridgeLegacy_x86;
	ComponentHandle<ComponentPluginBridge_amd64> pluginBridge_amd64;
	ComponentHandle<ComponentPluginBridgeLegacy_amd64> pluginBridgeLegacy_amd64;
#if defined(MPT_WITH_WINDOWS10)
	ComponentHandle<ComponentPluginBridge_arm> pluginBridge_arm;
	ComponentHandle<ComponentPluginBridgeLegacy_arm> pluginBridgeLegacy_arm;
	ComponentHandle<ComponentPluginBridge_arm64> pluginBridge_arm64;
	ComponentHandle<ComponentPluginBridgeLegacy_arm64> pluginBridgeLegacy_arm64;
#endif  // MPT_WITH_WINDOWS10
#endif  // !MPT_WITH_VST
	PLUGINDEX m_nPlugSlot = 0;

public:
	CSelectPluginDlg(CModDoc *pModDoc, PLUGINDEX pluginSlot, QWidget *parent);
	~CSelectPluginDlg();

	static VSTPluginLib *ScanPlugins(const mpt::PathString &path, QWidget *parent);
	static bool VerifyPlugin(VSTPluginLib *plug, QWidget *parent);

protected:
	QTreeWidgetItem * AddTreeItem(const mpt::ustring &title, int image, bool sort, QTreeWidgetItem * hParent = nullptr, intptr_t lParam = NULL);

	VSTPluginLib *GetSelectedPlugin();
	void SaveWindowPos() const;

	void ReloadMissingPlugins(const VSTPluginLib &lib) const;

	void UpdatePluginsList(const VSTPluginLib *forceSelect = nullptr);

	void DoDataExchange(DataExchange &dx) override;
	bool OnInitDialog() override;
	void OnDPIChanged() override;
	void OnOK() override;
	void OnCancel() override;
	bool PreTranslateMessage(mfcless::Message *pMsg) override;

	void RegisterMessageMap() override;
	void OnAddPlugin();
	void OnScanFolder();
	void OnRemovePlugin();
	void OnNameFilterChanged();
	void OnSetBridge();
	void OnSelChanged();
	void OnSelDblClk();
	void OnPluginTagsChanged();
};

OPENMPT_NAMESPACE_END
