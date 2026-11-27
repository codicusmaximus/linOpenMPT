/*
 * ColorPickerButton.h
 * -------------------
 * Purpose: A button for picking UI colors
 * Notes  : mfcless twin of upstream's mptrack/ColorPickerButton.h.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "mfcless/Graphics.h"
#include "mfcless/Button.h"
#include "../soundlib/Snd_defs.h"

OPENMPT_NAMESPACE_BEGIN

class CSoundFile;

class ColorPickerButton : public mfcless::Button
{
	Q_OBJECT
public:
	using mfcless::Button::Button;
public:
	void SetColor(uint32 color);
	std::optional<uint32> PickColor(const CSoundFile &sndFile, CHANNELINDEX chn);

protected:
	uint32 m_color = 0;

	void DrawItem(mfcless::DrawItemInfo *dis) override;
};

OPENMPT_NAMESPACE_END
