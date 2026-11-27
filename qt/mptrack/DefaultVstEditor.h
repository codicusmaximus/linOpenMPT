/*
 * DefaultVstEditor.h
 * ------------------
 * Purpose: Implementation of the default plugin editor that is used if a plugin does not provide an own editor GUI.
 * Notes  : mfcless twin of upstream's mptrack/DefaultVstEditor.h.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"

#ifndef NO_PLUGINS

#include "Mptrack.h"
#include "AbstractVstEditor.h"

class QLabel;
class QWheelEvent;
class QLineEdit;
class QRect;
class QScrollBar;
class QSlider;
class QWidget;

OPENMPT_NAMESPACE_BEGIN

enum
{
	PARAM_RESOLUTION = 1000,
	NUM_PLUGINEDITOR_PARAMETERS = 8,	// Parameters on screen
};

struct PluginEditorMeasurements;

class ParamControlSet
{
protected:
	QSlider *valueSlider = nullptr;
	QLineEdit *valueEdit = nullptr;
	QLabel *nameLabel = nullptr;
	QLabel *valueLabel = nullptr;
	QLabel *perMilLabel = nullptr;

public:
	ParamControlSet(QWidget *parent, const QRect &rect, int setID, const PluginEditorMeasurements &m);
	~ParamControlSet();

	void EnableControls(bool enable = true);
	void ResetContent();

	void SetParamName(const mpt::ustring &name);
	void SetParamValue(int value, const mpt::ustring &text);

	int GetParamValueFromSlider() const;
	int GetParamValueFromEdit() const;

	int GetSliderID() const { return m_setID; };
	// mfcless: for connecting the controls' signals.
	QSlider *Slider() const { return valueSlider; }
	QLineEdit *Edit() const { return valueEdit; }
private:
	int m_setID = 0;  // mfcless: the slider's control ID upstream
};


class CDefaultVstEditor : public CAbstractVstEditor
{
	Q_OBJECT
protected:

	std::vector<ParamControlSet *> controls;

	QScrollBar *paramScroller = nullptr;
	PlugParamIndex paramOffset;

	int m_nControlLock;

public:

	CDefaultVstEditor(IMixPlugin &plugin);
	~CDefaultVstEditor() override;

	void UpdateParamDisplays() override { CAbstractVstEditor::UpdateParamDisplays(); UpdateControls(false); };

	bool OpenEditor(QWidget *parent) override;

	// Plugins may not request to change the GUI size, since we use our own GUI.
	bool IsResizable() const override { return false; };
	bool SetSize(int, int) override { return false; };

protected:

	void DoDataExchange(DataExchange &dx) override;
	void OnParamTextboxChanged(uint32 id);
	void OnParamSliderChanged(uint32 id);

	// mfcless: the parameter scroll bar moved (upstream's OnVScroll).
	void OnVScroll(int pos);
	void wheelEvent(QWheelEvent *e) override;

protected:

	void CreateControls();
	void UpdateControls(bool updateParamNames);
	void SetParam(PlugParamIndex param, int value);
	void UpdateParamDisplay(PlugParamIndex param);

};

OPENMPT_NAMESPACE_END

#endif // NO_PLUGINS
