/*
 * DefaultVstEditor.cpp
 * --------------------
 * Purpose: Implementation of the default plugin editor that is used if a plugin does not provide an own editor GUI.
 * Notes  : mfcless twin of upstream's mptrack/DefaultVstEditor.cpp.
 *          Hand-edited from the twin.py draft: the parameter controls are
 *          made as Qt widgets, and their signals call the handlers.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "DefaultVstEditor.h"
#include "mfcless/Widgets.h"
#include "HighDPISupport.h"
#include "resource.h"
#include "mfcless/Widgets.h"

#include <QtCore/QSignalBlocker>
#include <QtGui/QIntValidator>
#include <QtGui/QWheelEvent>
#include <QtWidgets/QScrollBar>
#include <QtWidgets/QSlider>
#include "../soundlib/Sndfile.h"
#include "../soundlib/plugins/PlugInterface.h"


OPENMPT_NAMESPACE_BEGIN


#ifndef NO_PLUGINS


// Window proportions
struct PluginEditorMeasurements
{
	enum
	{
		edSpacing = 5,		// Spacing between elements
		edLineHeight = 20,	// Line of a single parameter line
		edEditWidth = 45,	// Width of the parameter edit box
		edPerMilWidth = 30,	// Width of the per mil label
		edRightWidth = edEditWidth + edSpacing + edPerMilWidth,	// Width of the right part of a parameter control set (edit box, param value)
		edTotalHeight = 2 * edLineHeight + edSpacing,			// Height of one set of controls
	};

	const int spacing;
	const int lineHeight;
	const int editWidth;
	const int perMilWidth;
	const int rightWidth;
	const int totalHeight;

	PluginEditorMeasurements(QWidget * hWnd)
		: spacing(HighDPISupport::ScalePixels(edSpacing, hWnd))
		, lineHeight(HighDPISupport::ScalePixels(edLineHeight, hWnd))
		, editWidth(HighDPISupport::ScalePixels(edEditWidth, hWnd))
		, perMilWidth(HighDPISupport::ScalePixels(edPerMilWidth, hWnd))
		, rightWidth(HighDPISupport::ScalePixels(edRightWidth, hWnd))
		, totalHeight(HighDPISupport::ScalePixels(edTotalHeight, hWnd))
	{ }
};


// Create a set of parameter controls
ParamControlSet::ParamControlSet(QWidget *parent, const QRect &qrect, int setID, const PluginEditorMeasurements &m)
	: m_setID(ID_PLUGINEDITOR_SLIDERS_BASE + setID)
{
	const mfcless::Rect rect(qrect);
	// Offset of components on the right side
	const int horizSplit = rect.left + rect.Width() - m.rightWidth;

	// Parameter name
	nameLabel = new QLabel(parent);
	nameLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	nameLabel->setGeometry(QRect(mfcless::Rect(rect.left, rect.top, horizSplit - m.spacing, rect.top + m.lineHeight)));

	// Parameter value as reported by the plugin
	valueLabel = new QLabel(parent);
	valueLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	valueLabel->setGeometry(QRect(mfcless::Rect(horizSplit, rect.top, rect.right, rect.top + m.lineHeight)));

	// Parameter value slider
	valueSlider = new QSlider(Qt::Horizontal, parent);
	valueSlider->setGeometry(QRect(mfcless::Rect(rect.left, rect.bottom - m.lineHeight, horizSplit - m.spacing, rect.bottom)));
	valueSlider->setRange(0, PARAM_RESOLUTION);
	valueSlider->setTickPosition(QSlider::TicksBelow);
	valueSlider->setTickInterval(PARAM_RESOLUTION / 10);

	// Parameter value edit box
	valueEdit = new QLineEdit(parent);
	valueEdit->setGeometry(QRect(mfcless::Rect(horizSplit, rect.bottom - m.lineHeight, horizSplit + m.editWidth, rect.bottom)));
	valueEdit->setValidator(new QIntValidator(0, 99999, valueEdit));

	// "Per mil" label
	perMilLabel = new QLabel(QString(QChar(0x2030)), parent);
	perMilLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	perMilLabel->setGeometry(QRect(mfcless::Rect(horizSplit + m.editWidth + m.spacing, rect.bottom - m.lineHeight, rect.right, rect.bottom)));

	for(QWidget *w : {static_cast<QWidget *>(nameLabel), static_cast<QWidget *>(valueLabel), static_cast<QWidget *>(valueSlider), static_cast<QWidget *>(valueEdit), static_cast<QWidget *>(perMilLabel)})
		w->show();
}


ParamControlSet::~ParamControlSet()
{
	delete nameLabel;
	delete valueLabel;
	delete valueSlider;
	delete valueEdit;
	delete perMilLabel;
}


// Enable a set of parameter controls
void ParamControlSet::EnableControls(bool enable)
{
	for(QWidget *w : {static_cast<QWidget *>(nameLabel), static_cast<QWidget *>(valueLabel), static_cast<QWidget *>(valueSlider), static_cast<QWidget *>(valueEdit), static_cast<QWidget *>(perMilLabel)})
		w->setEnabled(enable);
}


// Reset the content of a set of parameter controls
void ParamControlSet::ResetContent()
{
	nameLabel->clear();
	valueLabel->clear();
	valueSlider->setValue(0);
	valueEdit->clear();
}


void ParamControlSet::SetParamName(const mpt::ustring &name)
{
	nameLabel->setText(mfcless::ToQString(name));
}


void ParamControlSet::SetParamValue(int value, const mpt::ustring &text)
{
	valueSlider->setValue(value);
	if(!valueEdit->hasFocus())
	{
		// Don't update textbox when it has focus, else this will prevent user from changing the content.
		valueEdit->setText(QString::number(value));
	}
	valueLabel->setText(mfcless::ToQString(text));
}


int ParamControlSet::GetParamValueFromSlider() const
{
	return valueSlider->value();
}


int ParamControlSet::GetParamValueFromEdit() const
{
	int val = valueEdit->text().trimmed().toInt();
	Limit(val, int(0), int(PARAM_RESOLUTION));
	return val;
}


void CDefaultVstEditor::DoDataExchange(DataExchange &dx)
{
	CAbstractVstEditor::DoDataExchange(dx);
	//{{AFX_DATA_MAP(CDefaultVstEditor)
	ExchangeControl(dx, IDC_SCROLLBAR1, paramScroller);
	//}}AFX_DATA_MAP
}


CDefaultVstEditor::CDefaultVstEditor(IMixPlugin &plugin) : CAbstractVstEditor(plugin)
{
	m_nControlLock = 0;
	paramOffset = 0;

	controls.clear();
}

CDefaultVstEditor::~CDefaultVstEditor()
{
	for(size_t i = 0; i < controls.size(); i++)
	{
		delete controls[i];
	}
}


void CDefaultVstEditor::CreateControls()
{
	// Already initialized.
	if(!controls.empty())
	{
		return;
	}
	PluginEditorMeasurements m(this);

	mfcless::Rect rect = GetClientRect();
	int origHeight = rect.bottom;

	// Get parameter scroll bar dimensions and move it to the right side of the window
	mfcless::Rect scrollRect(0, 0, paramScroller->width(), rect.bottom);
	scrollRect.MoveToX(rect.right - scrollRect.right);

	// Ignore this space in our calculation from now on.
	rect.right -= scrollRect.Width();

	controls.clear();

	// Create a bit of border space
	rect.DeflateRect(m.spacing, m.spacing);
	rect.bottom = m.totalHeight;

	for(int i = 0; i < NUM_PLUGINEDITOR_PARAMETERS; i++)
	{
		auto *set = new ParamControlSet(this, QRect(rect), i, m);
		controls.push_back(set);
		const uint32 sliderID = ID_PLUGINEDITOR_SLIDERS_BASE + i, editID = ID_PLUGINEDITOR_EDIT_BASE + i;
		connect(set->Slider(), &QSlider::valueChanged, this, [this, sliderID] { OnParamSliderChanged(sliderID); });
		connect(set->Edit(), &QLineEdit::textChanged, this, [this, editID] { OnParamTextboxChanged(editID); });
		rect.OffsetRect(0, m.totalHeight + m.spacing);
	}

	// Calculate new ideal window height.
	const int heightChange = (rect.bottom - m.totalHeight + m.spacing) - origHeight;

	// Update parameter scroll bar height
	scrollRect.bottom += heightChange;
	paramScroller->setGeometry(QRect(scrollRect));

	// Resize window height
	setFixedHeight(height() + heightChange);

	// Set scrollbar page size.
	paramScroller->setPageStep(NUM_PLUGINEDITOR_PARAMETERS);
	paramScroller->setSingleStep(1);
	connect(paramScroller, &QScrollBar::valueChanged, this, &CDefaultVstEditor::OnVScroll);

	UpdateControls(true);
}


void CDefaultVstEditor::UpdateControls(bool updateParamNames)
{
	const PlugParamIndex numParams = m_VstPlugin.GetNumVisibleParameters();
	const PlugParamIndex scrollMax = numParams - std::min(numParams, static_cast<PlugParamIndex>(NUM_PLUGINEDITOR_PARAMETERS));
	LimitMax(paramOffset, scrollMax);

	if(static_cast<PlugParamIndex>(paramScroller->maximum()) != scrollMax)
	{
		// Number of parameters changed - update scrollbar limits
		const QSignalBlocker blocker(paramScroller);
		paramScroller->setRange(0, static_cast<int>(scrollMax));
		paramScroller->setValue(static_cast<int>(paramOffset));
		paramScroller->setEnabled(scrollMax > 0);
		updateParamNames = true;
	}

	m_VstPlugin.CacheParameterNames(paramOffset, std::min(paramOffset + NUM_PLUGINEDITOR_PARAMETERS, numParams));
	for(PlugParamIndex i = 0; i < NUM_PLUGINEDITOR_PARAMETERS; i++)
	{
		const PlugParamIndex param = paramOffset + i;

		if(param >= numParams)
		{
			// This param doesn't exist.
			controls[i]->EnableControls(false);
			controls[i]->ResetContent();
			continue;
		}

		controls[i]->EnableControls();

		if(updateParamNames)
		{
			// Update param name
			controls[i]->SetParamName(m_VstPlugin.GetFormattedParamName(param));
		}

		UpdateParamDisplay(param);
	}
}


void CDefaultVstEditor::OnVScroll(int pos)
{
	paramOffset = static_cast<PlugParamIndex>(std::max(pos, 0));
	UpdateControls(true);
}


void CDefaultVstEditor::wheelEvent(QWheelEvent *e)
{
	// Mouse wheel - scroll parameter list
	const int zDelta = e->angleDelta().y();
	if(paramScroller->minimum() != paramScroller->maximum() && zDelta != 0)
	{
		int offset = static_cast<int>(paramOffset) - mpt::signum(zDelta);
		Limit(offset, paramScroller->minimum(), paramScroller->maximum());
		paramScroller->setValue(offset);  // updates the controls
		e->accept();
		return;
	}
	CAbstractVstEditor::wheelEvent(e);
}


bool CDefaultVstEditor::OpenEditor(QWidget *parent)
{
	Create(IDD_DEFAULTPLUGINEDITOR, parent);
	CreateControls();
	return CAbstractVstEditor::OpenEditor(parent);
}


// Called when a change occurs to the parameter textbox
// If the change is triggered by the user, we'll need to notify the plugin and update 
// the other GUI controls 
void CDefaultVstEditor::OnParamTextboxChanged(uint32 id)
{
	if (m_nControlLock)
	{
		// Lock will be set if the GUI change was triggered internally (in UpdateParamDisplays).
		// We're only interested in handling changes triggered by the user.
		return;
	}

	const PlugParamIndex param = paramOffset + id - ID_PLUGINEDITOR_EDIT_BASE;

	// Extract value and update
	SetParam(param, controls[param - paramOffset]->GetParamValueFromEdit());
}


// Called when a change occurs to the parameter slider
// If the change is triggered by the user, we'll need to notify the plugin and update 
// the other GUI controls 
void CDefaultVstEditor::OnParamSliderChanged(uint32 id)
{
	if (m_nControlLock)
	{
		// Lock will be set if the GUI change was triggered internally (in UpdateParamDisplays).
		// We're only interested in handling changes triggered by the user.
		return;
	}

	const PlugParamIndex param = paramOffset + id - ID_PLUGINEDITOR_SLIDERS_BASE;

	// Extract value and update
	SetParam(param, controls[param - paramOffset]->GetParamValueFromSlider());

}


// Update a given parameter to a given value and notify plugin
void CDefaultVstEditor::SetParam(PlugParamIndex param, int value)
{
	if(param >= m_VstPlugin.GetNumVisibleParameters())
	{
		return;
	}

	m_VstPlugin.SetScaledUIParam(param, static_cast<PlugParamValue>(value) / static_cast<PlugParamValue>(PARAM_RESOLUTION));

	// Update other GUI controls
	UpdateParamDisplay(param);

	// Act as if an automation message has been sent by the plugin (record param changes, set document modified, etc...)
	m_VstPlugin.AutomateParameter(param);

}


//Update all GUI controls with the new param value
void CDefaultVstEditor::UpdateParamDisplay(PlugParamIndex param)
{
	if(m_nControlLock || param < paramOffset || param >= paramOffset + NUM_PLUGINEDITOR_PARAMETERS)
	{
		//Just to make sure we're not here as a consequence of an internal GUI change, and avoid modifying a parameter that doesn't exist on the GUI.
		return;
	}

	// Get the actual parameter value from the plugin
	const int val = static_cast<int>(m_VstPlugin.GetScaledUIParam(param) * static_cast<float>(PARAM_RESOLUTION) + 0.5f);

	// Update the GUI controls

	// Set lock to indicate that the changes to the GUI are internal - no need to notify the plug and re-update GUI.
	m_nControlLock++;

	controls[param - paramOffset]->SetParamValue(val, m_VstPlugin.GetFormattedParamValue(param));

	// Unset lock - done with internal GUI updates.
	m_nControlLock--;
	
}

#endif // NO_PLUGINS


OPENMPT_NAMESPACE_END
