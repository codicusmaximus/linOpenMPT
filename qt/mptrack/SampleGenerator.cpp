/*
 * SampleGenerator.cpp
 * -------------------
 * Purpose: Generate samples from math formulas using muParser
 * Notes  : mfcless twin of upstream's mptrack/SampleGenerator.cpp.
 *          Hand-edited from the twin.py draft.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */



#if MPT_DISABLED_CODE

#include "SampleGenerator.h"
#include "mfcless/Widgets.h"
#include "modsmp_ctrl.h"

int CSampleGenerator::sample_frequency = 44100;
int CSampleGenerator::sample_length = CSampleGenerator::sample_frequency;
mu::string_type CSampleGenerator::expression = UL_("sin(xp * _pi)");
smpgen_clip_methods CSampleGenerator::sample_clipping = smpgen_normalize;

mu::value_type *CSampleGenerator::sample_buffer = nullptr;
size_t CSampleGenerator::samples_written = 0;


CSampleGenerator::CSampleGenerator()
{

	// Setup function callbacks
	muParser.DefineFun(UL_("clip"), &ClipCallback, false);
	muParser.DefineFun(UL_("pwm"), &PWMCallback, false);
	muParser.DefineFun(UL_("rnd"), &RndCallback, false);
	muParser.DefineFun(UL_("smp"), &SampleDataCallback, false);
	muParser.DefineFun(UL_("tri"), &TriangleCallback, false);

	// Setup binary operator callbacks
	muParser.DefineOprt(UL_("mod"), &ModuloCallback, 0);

	//muParser.DefineConst("pi", (mu::value_type)PARSER_CONST_PI);

}


// Open the smpgen dialog
bool CSampleGenerator::ShowDialog()
{
	bool isDone = false, result = false;
	while(!isDone)
	{
		CSmpGenDialog dlg(sample_frequency, sample_length, sample_clipping, expression);
		dlg.DoModal();

		// pressed "OK" button?
		if(dlg.CanApply())
		{
			sample_frequency = dlg.GetFrequency();
			sample_length = dlg.GetLength();
			sample_clipping = dlg.GetClipping();
			expression = dlg.GetExpression();
			isDone = CanRenderSample();
			if(isDone) isDone = TestExpression();	// show dialog again if the formula can't be parsed.
			result = true;
		} else
		{
			isDone = true; // just quit.
			result = false;
		}
	}
	return result;
}


// Check if the currently select expression can be parsed by muParser.
bool CSampleGenerator::TestExpression()
{
	// reset helper variables
	samples_written = 0;
	sample_buffer = nullptr;

	muParser.SetExpr(expression);
	mu::value_type x = 0;
	muParser.DefineVar(UL_("x"), &x);
	muParser.DefineVar(UL_("xp"), &x);
	muParser.DefineVar(UL_("len"), &x);
	muParser.DefineVar(UL_("lens"), &x);
	muParser.DefineVar(UL_("freq"), &x);

	try
	{
		muParser.Eval();
	}
	catch (mu::Parser::exception_type &e)
	{
		ShowError(&e);
		return false;
	}
	return true;
}


// Check if sample parameters are valid.
bool CSampleGenerator::CanRenderSample() const
{
	if(sample_frequency < SMPGEN_MINFREQ || sample_frequency > SMPGEN_MAXFREQ || sample_length < SMPGEN_MINLENGTH || sample_length > SMPGEN_MAXLENGTH) return false;
	return true;
}


// Actual render loop.
bool CSampleGenerator::RenderSample(CSoundFile *pSndFile, SAMPLEINDEX nSample)
{
	if(!CanRenderSample() || !TestExpression() || (pSndFile == nullptr) || (nSample < 1) || (nSample > pSndFile->GetNumSamples())) return false;

	// allocate a new buffer
	sample_buffer = (mu::value_type *)malloc(sample_length * sizeof(mu::value_type));
	if(sample_buffer == nullptr) return false;
	memset(sample_buffer, 0, sample_length * sizeof(mu::value_type));

	mu::value_type x = 0, xp = 0;
	mu::value_type v_len = sample_length, v_freq = sample_frequency, v_lens = v_len / v_freq;
	muParser.DefineVar(UL_("x"), &x);
	muParser.DefineVar(UL_("xp"), &xp);
	muParser.DefineVar(UL_("len"), &v_len);
	muParser.DefineVar(UL_("lens"), &v_lens);
	muParser.DefineVar(UL_("freq"), &v_freq);

	bool success = true;
	mu::value_type minmax = 0;

	for(size_t i = 0; i < (size_t)sample_length; i++)
	{
		samples_written = i;
		x = (mu::value_type)i;
		xp = x * 100 / sample_length;

		try
		{
			sample_buffer[i] = muParser.Eval();
		}
		catch (mu::Parser::exception_type &e)
		{
			// let's just ignore div by zero errors (note: this error code is currently unused (muParser 1.30))
			if(e.GetCode() != mu::ecDIV_BY_ZERO)
			{
				ShowError(&e);
				success = false;
				break;
			}
			sample_buffer[i] = 0;
		}
		// new maximum value?
		if(std::abs(sample_buffer[i]) > minmax) minmax = std::abs(sample_buffer[i]);

	}

	if(success)
	{
		MODSAMPLE *pModSample = &pSndFile->Samples[nSample];

		BEGIN_CRITICAL();

		// first, save some memory... (leads to crashes)
		//CSoundFile::FreeSample(pModSample->pSample);
		//pModSample->pSample = nullptr;

		if(minmax == 0) minmax = 1;	// avoid division by 0

		// convert sample to 16-bit (or whateve rhas been specified)
		int16 *pSample = (sampling_type *)CSoundFile::AllocateSample((sample_length + 4) * SMPGEN_MIXBYTES);
		for(size_t i = 0; i < (size_t)sample_length; i++)
		{
			switch(sample_clipping)
			{
			case smpgen_clip: sample_buffer[i] = CLAMP(sample_buffer[i], -1, 1); break;	// option 1: clip
			case smpgen_normalize: sample_buffer[i] /= minmax; break;	// option 3: normalize
			}

			pSample[i] = (sampling_type)(sample_buffer[i] * sample_maxvalue);
		}

		// set new sample proprerties
		pModSample->nC5Speed = sample_frequency;
		CSoundFile::FrequencyToTranspose(pModSample);
		pModSample->uFlags |= CHN_16BIT;	// has to be adjusted if SMPGEN_MIXBYTES changes!
		pModSample->uFlags &= ~(CHN_STEREO|CHN_SUSTAINLOOP|CHN_PINGPONGSUSTAIN);
		pModSample->nLoopStart = 0;
		pModSample->nLoopEnd = sample_length;
		pModSample->nSustainStart = pModSample->nSustainEnd = 0;
		if(sample_length / sample_frequency < 5)	// arbitrary limit for automatic sample loop (5 seconds)
			pModSample->uFlags |= CHN_LOOP;
		else
			pModSample->uFlags &= ~(CHN_LOOP|CHN_PINGPONGLOOP);

		ctrlSmp::ReplaceSample(*pModSample, (char *)pSample, sample_length, pSndFile);

		END_CRITICAL();
	}

	free(sample_buffer);
	sample_buffer = nullptr;

	return success;
}


// Callback function to access sample data
mu::value_type CSampleGenerator::SampleDataCallback(mu::value_type v)
{
	if(sample_buffer == nullptr) return 0;
	v = CLAMP(v, 0, samples_written);
	size_t pos = static_cast<size_t>(v);
	return sample_buffer[pos];
}


void CSampleGenerator::ShowError(mu::Parser::exception_type *e)
{
	std::string errmsg;
	errmsg = "The expression\n    " + e->GetExpr() + "\ncontains an error ";
	if(!e->GetToken().empty())
		errmsg += "in the token\n    " + e->GetToken() + "\n";
	errmsg += "at position " + Stringify(e->GetPos()) + ".\nThe error message was: " + e->GetMsg();
	::MessageBox(0, errmsg.c_str(), UL_("muParser Sample Generator"), 0);
}


//////////////////////////////////////////////////////////////////////////
// Sample Generator Dialog implementation

#define MAX_SAMPLEGEN_EXPRESSIONS 61

void CSmpGenDialog::RegisterMessageMap()
{
	DialogBase::RegisterMessageMap();
	AddNotify(IDC_EDIT_SAMPLE_LENGTH, mfcless::Notify::EditChange, &CSmpGenDialog::OnSampleLengthChanged);
	AddNotify(IDC_EDIT_SAMPLE_LENGTH_SEC, mfcless::Notify::EditChange, &CSmpGenDialog::OnSampleSecondsChanged);
	AddNotify(IDC_EDIT_SAMPLE_FREQ, mfcless::Notify::EditChange, &CSmpGenDialog::OnSampleFreqChanged);
	AddNotify(IDC_EDIT_FORMULA, mfcless::Notify::EditChange, &CSmpGenDialog::OnExpressionChanged);
	AddCommand(IDC_BUTTON_SHOW_EXPRESSIONS, &CSmpGenDialog::OnShowExpressions);
	AddCommand(IDC_BUTTON_SAMPLEGEN_PRESETS, &CSmpGenDialog::OnShowPresets);
	AddCommandRange(ID_SAMPLE_GENERATOR_MENU, ID_SAMPLE_GENERATOR_MENU + MAX_SAMPLEGEN_EXPRESSIONS - 1, &CSmpGenDialog::OnInsertExpression);
	AddCommandRange(ID_SAMPLE_GENERATOR_PRESET_MENU, ID_SAMPLE_GENERATOR_PRESET_MENU + MAX_SAMPLEGEN_PRESETS + 1, &CSmpGenDialog::OnSelectPreset);
}


// List of all possible expression for expression menu
const samplegen_expression menu_descriptions[MAX_SAMPLEGEN_EXPRESSIONS] =
{
	{"Variables", ""},
	{"Current position (sampling point)", "x"},
	{"Current position (percentage)", "xp"},
	{"Sample length", "len"},
	{"Sample length (seconds)", "lens"},
	{"Sampling frequency", "freq"},
	{"Constants", ""},
	{"Pi", "_pi"},
	{"e", "_e"},
	{"Trigonometric functions", ""},
	{"Sine", "sin(x)"},
	{"Cosine", "cos(x)"},
	{"Tangens", "tan(x)"},
	{"Arcus Sine", "asin(x)"},
	{"Arcus Cosine", "acos(x)"},
	{"Arcus Tangens", "atan(x)"},
	{"Hyperbolic Sine", "sinh(x)"},
	{"Hyperbolic Cosine", "cosh(x)"},
	{"Hyperbolic Tangens", "tanh(x)"},
	{"Hyperbolic Arcus Sine", "asinh(x)"},
	{"Hyperbolic Arcus Cosine", "acosh(x)"},
	{"Hyperbolic Arcus Tangens", "atanh(x)"},
	{"Log, Exp, Root", ""},
	{"Logarithm (base 2)", "log2(x)"},
	{"Logarithm (base 10)", "log(x)"},
	{"Natural Logarithm (base e)", "ln(x)"},
	{"e^x", "exp(x)"},
	{"Square Root", "sqrt(x)"},
	{"Sign and rounding", ""},
	{"Sign", "sign(x)"},
	{"Absolute value", "abs(x)"},
	{"Round to nearest integer", "rint(x)"},
	{"Sets", ""},
	{"Minimum", "min(x, y, ...)"},
	{"Maximum", "max(x, y, ...)"},
	{"Sum", "sum(x, y, ...)"},
	{"Mean value", "avg(x, y, ...)"},
	{"Misc functions", ""},
	{"Pulse generator", "pwm(position, duty%, width)"},
	{"Triangle", "tri(position, width)"},
	{"Random value between 0 and x", "rnd(x)"},
	{"Access previous sampling point", "smp(position)"},
	{"Clip between values", "clip(value, minclip, maxclip)"},
	{"If...Then...Else", "if(condition, statement1, statement2)"},
	{"Operators", ""},
	{"Assignment", "x = y"},
	{"Logical And", "x abd y"},
	{"Logical Or", "x or y"},
	{"Logical Xor", "x xor y"},
	{"Less or equal", "x <= y"},
	{"Greater or equal", "x >= y"},
	{"Not equal", "x != y"},
	{"Equal", "x == y"},
	{"Greater than", "x > y"},
	{"Less than", "x < y"},
	{"Addition", "x + y"},
	{"Subtraction", "x - y"},
	{"Multiplication", "x * y"},
	{"Division", "x / y"},
	{"x^y", "x ^ y"},
	{"Modulo", "x mod y"},
};


bool CSmpGenDialog::OnInitDialog()
{
	DialogBase::OnInitDialog();
	RecalcParameters(false, true);
	SetItemText(IDC_EDIT_FORMULA, expression.c_str());

	int check = IDC_RADIO_SMPCLIP1;
	switch(sample_clipping)
	{
	case smpgen_clip: check = IDC_RADIO_SMPCLIP1; break;
	case smpgen_overflow: check = IDC_RADIO_SMPCLIP2; break;
	case smpgen_normalize: check = IDC_RADIO_SMPCLIP3; break;
	}
	CheckRadio(IDC_RADIO_SMPCLIP1, IDC_RADIO_SMPCLIP3, check);

	if(presets.GetNumPresets() == 0)
	{
		CreateDefaultPresets();
	}

	// Create font for "dropdown" button (Marlett system font)
	hButtonFont = CreateFont(14, 0, 0, 0, FW_DONTCARE, false, false, false, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, FF_DONTCARE, TEXT("Marlett"));
	::SendMessage(GetDlgItem(IDC_BUTTON_SHOW_EXPRESSIONS), WM_SETFONT, (uintptr_t)hButtonFont, MAKELPARAM(true, 0));

	return true;
}


void CSmpGenDialog::OnOK()
{
	DialogBase::OnOK();
	apply = true;

	int check = GetCheckedRadio(IDC_RADIO_SMPCLIP1, IDC_RADIO_SMPCLIP3);
	switch(check)
	{
	case IDC_RADIO_SMPCLIP1: sample_clipping = smpgen_clip; break;
	case IDC_RADIO_SMPCLIP2: sample_clipping = smpgen_overflow; break;
	case IDC_RADIO_SMPCLIP3: sample_clipping = smpgen_normalize; break;
	}

	DeleteObject(hButtonFont);
}


void CSmpGenDialog::OnCancel()
{
	DialogBase::OnCancel();
	apply = false;
}


// User changed formula
void CSmpGenDialog::OnExpressionChanged()
{
	mpt::ustring result;
	result = GetItemText(IDC_EDIT_FORMULA);
	expression = result;
}


// User changed sample length field
void CSmpGenDialog::OnSampleLengthChanged()
{
	int temp_length = GetItemInt(IDC_EDIT_SAMPLE_LENGTH);
	if(temp_length >= SMPGEN_MINLENGTH && temp_length <= SMPGEN_MAXLENGTH)
	{
		sample_length = temp_length;
		RecalcParameters(false);
	}
}


// User changed sample length (seconds) field
void CSmpGenDialog::OnSampleSecondsChanged()
{
	mpt::ustring str;
	str = GetItemText(IDC_EDIT_SAMPLE_LENGTH_SEC);
	double temp_seconds = atof(str);
	if(temp_seconds > 0)
	{
		sample_seconds = temp_seconds;
		RecalcParameters(true);
	}
}


// User changed sample frequency field
void CSmpGenDialog::OnSampleFreqChanged()
{
	int temp_freq = GetItemInt(IDC_EDIT_SAMPLE_FREQ);
	if(temp_freq >= SMPGEN_MINFREQ && temp_freq <= SMPGEN_MAXFREQ)
	{
		sample_frequency = temp_freq;
		RecalcParameters(false);
	}
}


// Show all expressions that can be input
void CSmpGenDialog::OnShowExpressions()
{
	mfcless::Menu hMenu = mfcless::Menu::CreatePopup(), hSubMenu = NULL;
	if(!hMenu) return;

	for(int i = 0; i < MAX_SAMPLEGEN_EXPRESSIONS; i++)
	{
		if(menu_descriptions[i].expression == "")
		{
			// add sub menu
			if(hSubMenu != NULL) hSubMenu.DestroyMenu();
			hSubMenu = ::CreatePopupMenu();

			hMenu.AppendMenu(mfcless::menu::Popup, hSubMenu, menu_descriptions[i].description.c_str());
		} else
		{
			// add sub menu entry (formula)
			hSubMenu.AppendMenu(mfcless::menu::String, ID_SAMPLE_GENERATOR_MENU + i, menu_descriptions[i].description.c_str());
		}
	}

	// place popup menu below button
	mfcless::Rect button;
	mfcless::GetWindowRect(GetDlgItem(IDC_BUTTON_SHOW_EXPRESSIONS), &button);
	hMenu.TrackPopupMenu(mfcless::menu::LeftAlign | mfcless::menu::RightButton, button.left, button.bottom, this);
	hMenu.DestroyMenu();
	hSubMenu.DestroyMenu();
}


// Show all expression presets
void CSmpGenDialog::OnShowPresets()
{
	mfcless::Menu hMenu = mfcless::Menu::CreatePopup();
	if(!hMenu) return;

	bool prestsExist = false;
	for(size_t i = 0; i < presets.GetNumPresets(); i++)
	{
		if(presets.GetPreset(i)->expression != "")
		{
			hMenu.AppendMenu(mfcless::menu::String, ID_SAMPLE_GENERATOR_PRESET_MENU + i, presets.GetPreset(i)->description.c_str());
			prestsExist = true;
		}
	}
	
	if(prestsExist) hMenu.AppendMenu(mfcless::menu::Separator, 0, NULL);

	hMenu.AppendMenu(mfcless::menu::String, ID_SAMPLE_GENERATOR_PRESET_MENU + MAX_SAMPLEGEN_PRESETS, _TEXT("Manage..."));

	mpt::ustring result;
	result = GetItemText(IDC_EDIT_FORMULA);
	if((!result.empty()) && (presets.GetNumPresets() < MAX_SAMPLEGEN_PRESETS))
	{
		hMenu.AppendMenu(mfcless::menu::String, ID_SAMPLE_GENERATOR_PRESET_MENU + MAX_SAMPLEGEN_PRESETS + 1, _TEXT("Add current..."));
	}

	// place popup menu below button
	mfcless::Rect button;
	mfcless::GetWindowRect(GetDlgItem(IDC_BUTTON_SAMPLEGEN_PRESETS), &button);
	hMenu.TrackPopupMenu(mfcless::menu::LeftAlign | mfcless::menu::RightButton, button.left, button.bottom, this);
	hMenu.DestroyMenu();
}



// Insert expression from context menu
void CSmpGenDialog::OnInsertExpression(uint32 nId)
{
	if((nId < ID_SAMPLE_GENERATOR_MENU) || (nId >= ID_SAMPLE_GENERATOR_MENU + MAX_SAMPLEGEN_EXPRESSIONS)) return;

	expression += " " +  menu_descriptions[nId - ID_SAMPLE_GENERATOR_MENU].expression;

	SetItemText(IDC_EDIT_FORMULA, expression.c_str());
}


// Select a preset (or manage them, or add one)
void CSmpGenDialog::OnSelectPreset(uint32 nId)
{
	if((nId < ID_SAMPLE_GENERATOR_PRESET_MENU) || (nId >= ID_SAMPLE_GENERATOR_PRESET_MENU + MAX_SAMPLEGEN_PRESETS + 2)) return;

	if(nId - ID_SAMPLE_GENERATOR_PRESET_MENU >= MAX_SAMPLEGEN_PRESETS)
	{
		// add...
		if((nId - ID_SAMPLE_GENERATOR_PRESET_MENU == MAX_SAMPLEGEN_PRESETS + 1))
		{
			samplegen_expression newPreset;
			newPreset.description = newPreset.expression = expression;
			presets.AddPreset(newPreset);
			// call preset manager now.
		}

		// manage...
		CSmpGenPresetDlg dlg(&presets);
		dlg.DoModal();
	} else
	{
		expression = presets.GetPreset(nId - ID_SAMPLE_GENERATOR_PRESET_MENU)->expression;
		SetItemText(IDC_EDIT_FORMULA, expression.c_str());
	}

}


// Update input fields, depending on what has been chagned
void CSmpGenDialog::RecalcParameters(bool secondsChanged, bool forceRefresh)
{
	static bool isLocked = false;
	if(isLocked) return;
	isLocked = true;	// avoid deadlock

	if(secondsChanged)
	{
		// seconds changed => recalc length
		sample_length = (int)(sample_seconds * sample_frequency);
		if(sample_length < SMPGEN_MINLENGTH || sample_length > SMPGEN_MAXLENGTH) sample_length = SMPGEN_MAXLENGTH;
	} else
	{
		// length/freq changed => recalc seconds
		sample_seconds = ((double)sample_length) / ((double)sample_frequency);
	}

	if(secondsChanged || forceRefresh) SetItemInt(IDC_EDIT_SAMPLE_LENGTH, sample_length);
	if(secondsChanged || forceRefresh) SetItemInt(IDC_EDIT_SAMPLE_FREQ, sample_frequency);
	mpt::ustring str;
	str.Format("%.4f", sample_seconds);
	if(!secondsChanged || forceRefresh) SetItemText(IDC_EDIT_SAMPLE_LENGTH_SEC, str);

	int smpsize = sample_length * SMPGEN_MIXBYTES;
	if(smpsize < 1024)
	{
		str.Format("Sample Size: %d Bytes", smpsize);
	} else if((smpsize >> 10) < 1024)
	{
		str.Format("Sample Size: %d KB", smpsize >> 10);
	} else
	{
		str.Format("Sample Size: %d MB", smpsize >> 20);
	}
	SetItemText(IDC_STATIC_SMPSIZE_KB, str);

	isLocked = false;
}


// Create a set of default formla presets
void CSmpGenDialog::CreateDefaultPresets()
{
	samplegen_expression preset;

	preset.description = "A440";
	preset.expression = "sin(xp * _pi / 50 * 440 * len / freq)";
	presets.AddPreset(preset);

	preset.description = "Brown Noise (kind of)";
	preset.expression = "rnd(1) * 0.1 + smp(x - 1) * 0.9";
	presets.AddPreset(preset);

	preset.description = "Noisy Saw";
	preset.expression = "(x mod 800) / 800 - 0.5 + rnd(0.1)";
	presets.AddPreset(preset);

	preset.description = "PWM Filter";
	preset.expression = "pwm(x, 50 + sin(xp * _pi / 100) * 40, 100) + tri(x, 50)";
	presets.AddPreset(preset);
	
	preset.description = "Fat PWM Pad";
	preset.expression = "pwm(x, xp, 500) + pwm(x, abs(50 - xp), 1000)";
	presets.AddPreset(preset);

	preset.description = "Dual Square";
	preset.expression = "if((x mod 100) < 50, (x mod 200), -x mod 200)";
	presets.AddPreset(preset);

	preset.description = "Noise Hit";
	preset.expression = "exp(-xp) * (rnd(x) - x / 2)";
	presets.AddPreset(preset);

	preset.description = "Laser";
	preset.expression = "sin(xp * _pi * 100 /(xp ^ 2)) * 100 / sqrt(xp)";
	presets.AddPreset(preset);

	preset.description = "Noisy Laser Hit";
	preset.expression = "(sin(sqrt(xp) * 100) + rnd(1) - 0.5) * exp(-xp / 10)";
	presets.AddPreset(preset);

	preset.description = "Twinkle, Twinkle...";
	preset.expression = "sin(xp * _pi * 100 / xp) * 100 / sqrt(xp)";
	presets.AddPreset(preset);

	preset.description = "FM Tom";
	preset.expression = "sin(xp * _pi * 2 + (xp / 5 - 50) ^ 2) * exp(-xp / 10)";
	presets.AddPreset(preset);

	preset.description = "FM Warp";
	preset.expression = "sin(_pi * xp / 2 * (1 + (1 + sin(_pi * xp / 4 * 50)) / 4)) * exp(-(xp / 8) * .6)";
	presets.AddPreset(preset);

	preset.description = "Weird Noise";
	preset.expression = "rnd(1) * 0.1 + smp(x - rnd(xp)) * 0.9";
	presets.AddPreset(preset);

}



//////////////////////////////////////////////////////////////////////////
// Sample Generator Preset Dialog implementation


void CSmpGenPresetDlg::RegisterMessageMap()
{
	DialogBase::RegisterMessageMap();
	AddCommand(IDC_BUTTON_ADD, &CSmpGenPresetDlg::OnAddPreset);
	AddCommand(IDC_BUTTON_REMOVE, &CSmpGenPresetDlg::OnRemovePreset);
	AddNotify(IDC_EDIT_PRESET_NAME, mfcless::Notify::EditChange, &CSmpGenPresetDlg::OnTextChanged);
	AddNotify(IDC_EDIT_PRESET_EXPR, mfcless::Notify::EditChange, &CSmpGenPresetDlg::OnExpressionChanged);
	AddNotify(IDC_LIST_SAMPLEGEN_PRESETS, mfcless::Notify::SelChange, &CSmpGenPresetDlg::OnListSelChange);
}


bool CSmpGenPresetDlg::OnInitDialog()
{
	DialogBase::OnInitDialog();

	RefreshList();

	return true;
}


void CSmpGenPresetDlg::OnOK()
{
	// remove empty presets
	for(size_t i = 0; i < presets->GetNumPresets(); i++)
	{
		if(presets->GetPreset(i)->expression.empty())
		{
			presets->RemovePreset(i);
		}
	}
	DialogBase::OnOK();
}


void CSmpGenPresetDlg::OnListSelChange()
{
	currentItem = (Item<QListWidget>(IDC_LIST_SAMPLEGEN_PRESETS))->currentIndex() + 1;
	if(currentItem == 0 || currentItem > presets->GetNumPresets()) return;
	samplegen_expression *preset = presets->GetPreset(currentItem - 1);
	if(preset == nullptr) return;
	SetItemText(IDC_EDIT_PRESET_NAME, preset->description.c_str());
	SetItemText(IDC_EDIT_PRESET_EXPR, preset->expression.c_str());
}


void CSmpGenPresetDlg::OnTextChanged()
{
	if(currentItem == 0 || currentItem > presets->GetNumPresets()) return;
	mpt::ustring result;
	result = GetItemText(IDC_EDIT_PRESET_NAME);

	samplegen_expression *preset = presets->GetPreset(currentItem - 1);
	if(preset == nullptr) return;
	preset->description = result;

	QListWidget *clist = Item<QListWidget>(IDC_LIST_SAMPLEGEN_PRESETS);
	mfcless::DeleteString(clist, currentItem - 1);
	mfcless::InsertItem(clist, currentItem - 1, (preset->description).c_str());
	clist->setCurrentIndex(currentItem - 1);
}


void CSmpGenPresetDlg::OnExpressionChanged()
{
	if(currentItem == 0 || currentItem > presets->GetNumPresets()) return;
	mpt::ustring result;
	result = GetItemText(IDC_EDIT_PRESET_EXPR);

	samplegen_expression *preset = presets->GetPreset(currentItem - 1);
	if(preset != nullptr) preset->expression = result;

}


void CSmpGenPresetDlg::OnAddPreset()
{
	samplegen_expression newPreset;
	newPreset.description = "New Preset";
	newPreset.expression = "";
	if(presets->AddPreset(newPreset))
	{
		currentItem = presets->GetNumPresets();
		RefreshList();
	}
}


void CSmpGenPresetDlg::OnRemovePreset()
{
	if(currentItem == 0 || currentItem > presets->GetNumPresets()) return;
	if(presets->RemovePreset(currentItem - 1))
		RefreshList();
}


void CSmpGenPresetDlg::RefreshList()
{
	QListWidget *clist = Item<QListWidget>(IDC_LIST_SAMPLEGEN_PRESETS);
	clist->setUpdatesEnabled(false);	//disable lisbox refreshes during fill to avoid flicker
	clist->clear();
	for(size_t i = 0; i < presets->GetNumPresets(); i++)
	{
		samplegen_expression *preset = presets->GetPreset(i);
		if(preset != nullptr)
			mfcless::AddItem(clist, (preset->description).c_str());
	}
	clist->setUpdatesEnabled(true);		//re-enable lisbox refreshes
	if(currentItem == 0 || currentItem > presets->GetNumPresets())
	{
		currentItem = presets->GetNumPresets();
	}
	if(currentItem != 0) clist->setCurrentIndex(currentItem - 1);
	OnListSelChange();
}

#endif // MPT_DISABLED_CODE
