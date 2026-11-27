/*
 * ColorConfigDlg.cpp
 * ------------------
 * Purpose: Implementation of the color setup dialog.
 * Notes  : mfcless twin of upstream's mptrack/ColorConfigDlg.h.
 *          Draft from qt/tools/twin.py, not yet edited by hand.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "DialogBase.h"
#include "mfcless/Graphics.h"
#include "mfcless/Controls.h"
#include "AccessibleControls.h"
#include "ColorPickerButton.h"
#include "TrackerSettings.h"

class QAbstractButton;
class QComboBox;
class QLabel;
class QScrollBar;

OPENMPT_NAMESPACE_BEGIN

struct MODPLUGDIB;

class COptionsColors : public PropertyPage
{
	Q_OBJECT
protected:
	std::array<uint32, MAX_MODCOLORS> CustomColors;
	QComboBox *m_ComboDPIAwareness = nullptr, *m_ComboItem = nullptr, *m_ComboFont = nullptr, *m_ComboPreset = nullptr;
	ColorPickerButton *m_BtnColor[3] = {};
	QAbstractButton *m_BtnPreview = nullptr;
	mfcless::UpDown *m_ColorSpin = nullptr, *m_spinRowsPerBeat = nullptr, *m_spinRowsPerMeasure = nullptr;
	AccessibleEdit *m_rpbEdit = nullptr, *m_rpmEdit = nullptr;
	QLabel *m_TxtColor[3] = {};
	MODPLUGDIB *m_pPreviewDib = nullptr;
	FontSetting patternFont, commentFont;
	uint32 m_nColorItem = 0;

public:
	COptionsColors();
	~COptionsColors();

protected:
	bool OnInitDialog() override;
	bool OnKillActive() override;
	void OnOK() override;
	void DoDataExchange(DataExchange &dx) override;
	bool OnSetActive() override;
	void OnVScroll(uint32 nSBCode, uint32 nPos, QScrollBar * pScrollBar);

	void SelectColor(int colorIndex);

	void OnChoosePatternFont();
	void OnChooseCommentFont();
	void OnUpdateDialog();
	void OnDrawItem(int nIdCtl, mfcless::DrawItemInfo * lpdis);
	void OnColorSelChanged();
	void OnHighlightsChanged();
	void OnSettingsChanged();
	void OnSelectColor1() { SelectColor(0); }
	void OnSelectColor2() { SelectColor(1); }
	void OnSelectColor3() { SelectColor(2); }
	void OnPresetChange();
	void OnLoadColorScheme();
	void OnSaveColorScheme();
	void OnClearWindowCache();
	void OnPreviewChanged();
	void RegisterMessageMap() override;;
};

OPENMPT_NAMESPACE_END
