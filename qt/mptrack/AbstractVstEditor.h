/*
 * AbstractVstEditor.h
 * -------------------
 * Purpose: Common plugin editor interface class. This code is shared between custom and default plugin user interfaces.
 * Notes  : mfcless twin of upstream's mptrack/AbstractVstEditor.h. Menus are
 *          QMenus in a QMenuBar; message handlers are plain member functions.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"

#ifndef NO_PLUGINS

#include "ResizableDialog.h"
#include "mfcless/Keyboard.h"
#include "Moddoc.h"
#include "../soundlib/Snd_defs.h"

#include <QtWidgets/QMenu>

class QMenuBar;
class QCloseEvent;
class QDragEnterEvent;
class QDropEvent;
class QKeyEvent;
class QMoveEvent;

OPENMPT_NAMESPACE_BEGIN

class IMixPlugin;
struct UpdateHint;

class CAbstractVstEditor : public ResizableDialog
{
	Q_OBJECT
protected:
	QMenuBar *m_Menu = nullptr;
	QMenu m_PresetMenu;
	std::vector<std::unique_ptr<QMenu>> m_presetMenuGroup;
	QMenu m_InputMenu;
	QMenu m_OutputMenu;
	QMenu m_MacroMenu;
	QMenu m_OptionsMenu;
	int32 m_currentPresetMenu = 0;
	int32 m_clientHeight;
	int m_nLearnMacro = -1;
	int m_nCurProg = -1;
	INSTRUMENTINDEX m_nInstrument;
	bool m_isMinimized = false;
	bool m_updateDisplay = false;
	CModDoc::NoteToChannelMap m_noteChannel;	// Note -> Preview channel assignment
	bool m_hasWindow = false;

	// Adjust window size if menu bar height changes
	class WindowSizeAdjuster
	{
		QWidget &m_wnd;
		int m_menuHeight = 0;
	public:
		WindowSizeAdjuster(QWidget &wnd);
		~WindowSizeAdjuster();
	};

public:
	IMixPlugin &m_VstPlugin;

	CAbstractVstEditor(IMixPlugin &plugin);
	~CAbstractVstEditor() override;

	void SetupMenu(bool force = false);
	void SetTitle();
	void SetLearnMacro(int inMacro);
	int GetLearnMacro();

	void SetPreset(int32 preset);
	void UpdatePresetField();

	void OnLoadPreset();
	void OnSavePreset();
	void OnCopyParameters();
	void OnPasteParameters();
	void OnRandomizePreset();
	void OnRenamePlugin();
	void OnSetPreset(uint32 nID);
	void OnBypassPlug();
	void OnRecordAutomation();
	void OnRecordMIDIOut();
	void OnPassKeypressesToPlug();
	void OnSetPreviousVSTPreset();
	void OnSetNextVSTPreset();
	void OnVSTPresetBackwardJump();
	void OnVSTPresetForwardJump();
	void OnVSTPresetRename();
	void OnCreateInstrument();
	intptr_t OnCustomKeyMsg(uintptr_t, intptr_t);
	intptr_t OnMidiMsg(uintptr_t, intptr_t);
	void OnMove(int x, int y);
	void OnClose() { DoClose(); }

	// mfcless: posted to the editor, as PostMessage was (WM_MOD_* IDs).
	void PostModMessage(uint32 msg, uintptr_t wParam = 0, intptr_t lParam = 0);
	// mfcless: whether the editor window exists (upstream: m_hWnd != nullptr).
	bool HasWindow() const { return m_hasWindow; }

	// Overridden methods:
	void OnOK() override { DoClose(); }
	void OnCancel() override { DoClose(); }

	virtual bool OpenEditor(QWidget *parent);
	virtual void DoClose();
	virtual void UpdateParamDisplays() { if(m_updateDisplay) { SetupMenu(true); m_updateDisplay = false; } }
	virtual void UpdateParam(int32 /*param*/) { }
	virtual void UpdateView(UpdateHint hint);

	virtual bool IsResizable() const = 0;
	virtual bool SetSize(int contentWidth, int contentHeight) = 0;

	void UpdateDisplay() { m_updateDisplay = true; }

protected:
	bool event(QEvent *e) override;
	void closeEvent(QCloseEvent *e) override;
	void dragEnterEvent(QDragEnterEvent *e) override;
	void dropEvent(QDropEvent *e) override;
	void moveEvent(QMoveEvent *e) override;
	bool HandleKeyMessage(mfcless::Message &msg, bool handleGlobal = false);
	void UpdatePresetMenu(bool force = false);
	void GeneratePresetMenu(int32 offset, QMenu &parent);
	void UpdateInputMenu();
	void UpdateOutputMenu();
	void UpdateMacroMenu();
	void UpdateOptionsMenu();
	INSTRUMENTINDEX GetBestInstrumentCandidate() const;
	bool CheckInstrument(INSTRUMENTINDEX ins) const;
	bool ValidateCurrentInstrument();

	void OnToggleEditor(uint32 nID);
	void OnSetInputInstrument(uint32 nID);
	void OnInitMenu();
	void PrepareToLearnMacro(uint32 nID);

	void OnActivate(bool active);

	void StoreWindowPos();
	void RestoreWindowPos();
};

OPENMPT_NAMESPACE_END

#endif // NO_PLUGINS
