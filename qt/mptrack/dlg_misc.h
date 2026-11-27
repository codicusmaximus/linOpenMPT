/*
 * dlg_misc.h
 * ----------
 * Purpose: Implementation for various OpenMPT dialogs.
 * Notes  : mfcless twin of upstream's mptrack/dlg_misc.h.
 *          Draft from qt/tools/twin.py, not yet edited by hand.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include <QtWidgets/QWidget>

#include "CDecimalSupport.h"
#include "DialogBase.h"
#include "ResizableDialog.h"
#include "../soundlib/Sndfile.h"

class QAbstractButton;
class QComboBox;
class QFont;
class QListWidget;
class QPoint;
class QRect;
class QScrollBar;
class QSlider;
class QSpinBox;

OPENMPT_NAMESPACE_BEGIN

class Version;
class CModDoc;
class CDLSBank;

class CModTypeDlg : public DialogBase
{
	Q_OBJECT
protected:
	QComboBox *m_TypeBox = nullptr, *m_ChannelsBox = nullptr, *m_TempoModeBox = nullptr, *m_PlugMixBox = nullptr;
	QAbstractButton *m_CheckBox1 = nullptr, *m_CheckBox2 = nullptr, *m_CheckBox3 = nullptr, *m_CheckBox4 = nullptr, *m_CheckBox5 = nullptr, *m_CheckBoxPT1x = nullptr, *m_CheckBoxFt2VolRamp = nullptr, *m_CheckBoxAmigaLimits = nullptr;
	HICON m_warnIcon = nullptr;

	CSoundFile &sndFile;
public:
	TempoSwing m_tempoSwing;
	PlayBehaviourSet m_playBehaviour;
	CHANNELINDEX m_nChannels = 0;
	MODTYPE m_nType = MOD_TYPE_NONE;
	bool m_showWarning = false;
	bool m_initialized = false;

public:
	CModTypeDlg(CSoundFile &sf, QWidget *parent);
	bool VerifyData();
	void UpdateDialog();
	void OnPTModeChanged();
	void OnTempoModeChanged();
	void OnTempoSwing();
	void OnLegacyPlaybackSettings();
	void OnDefaultBehaviour();

protected:
	void UpdateChannelCBox();
	mpt::ustring FormatVersionNumber(Version version);

protected:
	void DoDataExchange();
	bool OnInitDialog() override;
	void OnDPIChanged() override;
	void OnOK() override;
	mpt::ustring GetToolTipText(uint32 id, QWidget * QWidget *) const override;
};


class CLegacyPlaybackSettingsDlg : public ResizableDialog
{
	Q_OBJECT
protected:
	CCheckListBox m_CheckList;
	PlayBehaviourSet m_playBehaviour;
	MODTYPE m_modType;

public:
	CLegacyPlaybackSettingsDlg(QWidget *parent, PlayBehaviourSet &playBehaviour, MODTYPE modType);

	PlayBehaviourSet GetPlayBehaviour() const { return m_playBehaviour; }

protected:
	void DoDataExchange();
	bool OnInitDialog() override;

	void OnSelectDefaults();
	void UpdateSelectDefaults();
	void OnFilterStringChanged();
};


class CRemoveChannelsDlg : public DialogBase
{
	Q_OBJECT
public:
	CSoundFile &sndFile;
	std::vector<bool> m_bKeepMask;
	CHANNELINDEX m_nRemove;
	QListWidget *m_RemChansList = nullptr;
	bool m_ShowCancel;

public:
	CRemoveChannelsDlg(CSoundFile &sf, CHANNELINDEX toRemove, bool showCancel = true, QWidget *parent = nullptr);

protected:
	void DoDataExchange();
	bool OnInitDialog() override;
	void OnOK() override;
	void OnChannelChanged();
	DECLARE_MESSAGE_MAP();
};


class InfoDialog : protected ResizableDialog
{
private:
	mpt::ustring m_caption, m_content;

public:
	InfoDialog(QWidget *parent = nullptr);
	void SetCaption(mpt::ustring caption);
	void SetContent(mpt::ustring content);
	using ResizableDialog::DoModal;

protected:
	bool OnInitDialog() override;
};

////////////////////////////////////////////////////////////////////////
// Sound Banks

class CSoundBankProperties : public InfoDialog
{
public:
	CSoundBankProperties(const CDLSBank &bank, QWidget *parent = nullptr);
};


/////////////////////////////////////////////////////////////////////////
// Keyboard control

enum
{
	KBDNOTIFY_MOUSEMOVE=0,
	KBDNOTIFY_LBUTTONDOWN,
	KBDNOTIFY_LBUTTONUP,
};

class CKeyboardControl: public QWidget
{
	Q_OBJECT
public:
	enum
	{
		KEYFLAG_NORMAL    = 0x00,
		KEYFLAG_REDDOT    = 0x01,
		KEYFLAG_BRIGHTDOT = 0x02,
	};
protected:
	QWidget *m_parent = nullptr;
	QFont m_font;
	int m_nOctaves = 1;
	int m_nSelection = -1;
	bool m_mouseCapture = false, m_cursorNotify = false;
	bool m_mouseDown = false;

	std::array<uint8, NOTE_MAX> KeyFlags; // 10 octaves max
	std::array<SAMPLEINDEX, NOTE_MAX> m_sampleNum;

public:
	CKeyboardControl() = default;

public:
	void Init(QWidget *parent, int octaves = 1, bool cursorNotify = false);
	void SetFlags(uint32 key, uint8 flags) { if (key < NOTE_MAX) KeyFlags[key] = flags; }
	uint8 GetFlags(uint32 key) const { return (key < NOTE_MAX) ? KeyFlags[key] : 0; }
	void SetSample(uint32 key, SAMPLEINDEX sample) { if (key < NOTE_MAX) m_sampleNum[key] = sample; }
	SAMPLEINDEX GetSample(uint32 key) const { return (key < NOTE_MAX) ? m_sampleNum[key] : 0; }

protected:
	void DrawKey(CPaintDC &dc, const QRect rect, int key, bool black) const;

	intptr_t OnDPIChanged(uintptr_t, intptr_t);
	void OnDestroy();
	void OnPaint();
	void OnMouseMove(uint32 nFlags, QPoint point);
	void OnLButtonDown(uint32 nFlags, QPoint point);
	void OnLButtonUp(uint32 nFlags, QPoint point);
};


/////////////////////////////////////////////////////////////////////////
// Sample Map

class CSampleMapDlg : public DialogBase
{
	Q_OBJECT
protected:
	enum class MouseAction
	{
		Unknown,  // Didn't mouse-down yet
		Set,      // Set selected sample
		Unset,    // Unset (revert to original keymap)
		Zero,     // Set to zero
	};

	CKeyboardControl m_Keyboard;
	QComboBox *m_CbnSample = nullptr;
	QSlider *m_SbOctave = nullptr;

	CSoundFile &sndFile;
	const INSTRUMENTINDEX m_nInstrument;
	std::array<SAMPLEINDEX, NOTE_MAX - NOTE_MIN + 1> KeyboardMap;
	MouseAction mouseAction = MouseAction::Unknown;

public:
	CSampleMapDlg(CSoundFile &sf, INSTRUMENTINDEX nInstr, QWidget *parent = nullptr);

protected:
	void DoDataExchange();
	bool OnInitDialog() override;
	void OnOK() override;
	void OnDPIChanged() override;

	void OnUpdateSamples();
	void OnUpdateKeyboard();
	void OnUpdateOctave();
	void OnHScroll(uint32, uint32, QScrollBar *);
	intptr_t OnKeyboardNotify(uintptr_t, intptr_t);
};


/////////////////////////////////////////////////////////////////////////
// Edit history dialog

class CEditHistoryDlg: public ResizableDialog
{
	Q_OBJECT
protected:
	CModDoc &m_modDoc;

public:
	CEditHistoryDlg(QWidget *parent, CModDoc &modDoc);

protected:
	bool OnInitDialog() override;
	void OnClearHistory();
};


/////////////////////////////////////////////////////////////////////////
// Generic input dialog

class CInputDlg : public DialogBase
{
	Q_OBJECT
protected:
	CNumberEdit m_edit;
	QSpinBox *m_spin = nullptr;
	const mpt::ustring m_description;
	const double m_minValueDbl = 0.0, m_maxValueDbl = 0.0;
	const int32 m_minValueInt = 0, m_maxValueInt = 0;
	const int32 m_maxLength = 0;

public:
	int32 resultAsInt = 0;
	double resultAsDouble = 0.0;
	mpt::ustring resultAsString;

protected:
	CInputDlg(QWidget *parent, const mpt::uchar *desc, const mpt::uchar *defaultString, int32 maxLength, double minValDbl, double maxValDbl = 0.0, double defaultDbl = 0.0, int32 minValInt = 0, int32 maxValInt = 0, int32 defaultInt = 0);

public:
	// Initialize text input box
	CInputDlg(QWidget *parent, const mpt::uchar *desc, const mpt::uchar *defaultString, int32 maxLength = -1) : CInputDlg{parent, desc, defaultString, maxLength, 0.0} { }
	// Initialize numeric input box (float)
	CInputDlg(QWidget *parent, const mpt::uchar *desc, double minVal, double maxVal, double defaultNumber) : CInputDlg{parent, desc, {}, -1, minVal, maxVal, defaultNumber} { }
	CInputDlg(QWidget *parent, const mpt::uchar *desc, float minVal, float maxVal, float defaultNumber) : CInputDlg{parent, desc, {}, -1, minVal, maxVal, defaultNumber } { }
	// Initialize numeric input box (int)
	CInputDlg(QWidget *parent, const mpt::uchar *desc, int32 minVal, int32 maxVal, int32 defaultNumber) : CInputDlg{parent, desc, {}, -1, 0.0, 0.0, 0.0, minVal, maxVal, defaultNumber } { }

protected:
	void DoDataExchange();
	bool OnInitDialog() override;
	void OnOK() override;
};


/////////////////////////////////////////////////////////////////////////
// Messagebox with 'don't show again'-option.

// Enums for message entries. See dlg_misc.cpp for the array of entries.
enum enMsgBoxHidableMessage
{
	ModSaveHint                = 0,
	ItCompatibilityExportTip   = 1,
	XMCompatibilityExportTip   = 2,
	CompatExportDefaultWarning = 3,
	enMsgBoxHidableMessage_count
};

void MsgBoxHidable(enMsgBoxHidableMessage enMsg);

OPENMPT_NAMESPACE_END
