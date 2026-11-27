/*
 * EffectInfo.cpp
 * --------------
 * Purpose: Provide information about effect names, parameter interpretation to the tracker interface.
 * Notes  : mfcless twin of upstream's mptrack/EffectInfo.cpp.
 *          Hand-edited from the twin.py draft.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "EffectInfo.h"
#include "mfcless/Widgets.h"
#include "Mptrack.h"	// for szHexChar
#include "../soundlib/Sndfile.h"
#include "../soundlib/mod_specifications.h"
#include "../soundlib/Tables.h"


OPENMPT_NAMESPACE_BEGIN


///////////////////////////////////////////////////////////////////////////
// Effects description

struct MPTEffectInfo
{
	EffectCommand effect;               // CMD_XXXX
	ModCommand::PARAM paramMask;        // 0 = default
	ModCommand::PARAM paramValue;       // 0 = default
	ModCommand::PARAM paramLimit;       // Parameter Editor limit
	FlagSet<MODTYPE> supportedFormats;  // MOD_TYPE_XXX combo
	const mpt::uchar *name;                  // e.g. "Tone Portamento"
};

static constexpr FlagSet<MODTYPE> MOD_TYPE_MODXM = MOD_TYPE_MOD | MOD_TYPE_XM;
static constexpr FlagSet<MODTYPE> MOD_TYPE_S3MITMPT = MOD_TYPE_S3M | MOD_TYPE_IT | MOD_TYPE_MPT;
static constexpr FlagSet<MODTYPE> MOD_TYPE_NOMOD = MOD_TYPE_S3M | MOD_TYPE_XM | MOD_TYPE_IT | MOD_TYPE_MPT;
static constexpr FlagSet<MODTYPE> MOD_TYPE_XMITMPT = MOD_TYPE_XM | MOD_TYPE_IT | MOD_TYPE_MPT;
static constexpr FlagSet<MODTYPE> MOD_TYPE_ITMPT = MOD_TYPE_IT | MOD_TYPE_MPT;
static constexpr FlagSet<MODTYPE> MOD_TYPE_ALL = FlagSet<MODTYPE>::value_type::from_bits(~FlagSet<MODTYPE>::store_type{0});

static constexpr MPTEffectInfo gFXInfo[] =
{
	{CMD_ARPEGGIO,		0,0,		0,	MOD_TYPE_ALL,	UL_("Arpeggio")},
	{CMD_PORTAMENTOUP,	0,0,		0,	MOD_TYPE_ALL,	UL_("Portamento Up")},
	{CMD_PORTAMENTODOWN,0,0,		0,	MOD_TYPE_ALL,	UL_("Portamento Down")},
	{CMD_TONEPORTAMENTO,0,0,		0,	MOD_TYPE_ALL,	UL_("Tone Portamento")},
	{CMD_VIBRATO,		0,0,		0,	MOD_TYPE_ALL,	UL_("Vibrato")},
	{CMD_TONEPORTAVOL,	0,0,		0,	MOD_TYPE_ALL,	UL_("Volslide+Toneporta")},
	{CMD_VIBRATOVOL,	0,0,		0,	MOD_TYPE_ALL,	UL_("VolSlide+Vibrato")},
	{CMD_TREMOLO,		0,0,		0,	MOD_TYPE_ALL,	UL_("Tremolo")},
	{CMD_PANNING8,		0,0,		0,	MOD_TYPE_ALL,	UL_("Set Panning")},
	{CMD_OFFSET,		0,0,		0,	MOD_TYPE_ALL,	UL_("Set Offset")},
	{CMD_VOLUMESLIDE,	0,0,		0,	MOD_TYPE_ALL,	UL_("Volume Slide")},
	{CMD_POSITIONJUMP,	0,0,		0,	MOD_TYPE_ALL,	UL_("Position Jump")},
	{CMD_VOLUME,		0,0,		0,	MOD_TYPE_MODXM,	UL_("Set Volume")},
	{CMD_PATTERNBREAK,	0,0,		0,	MOD_TYPE_ALL,	UL_("Pattern Break")},
	{CMD_RETRIG,		0,0,		0,	MOD_TYPE_NOMOD,	UL_("Retrigger Note")},
	{CMD_SPEED,			0,0,		0,	MOD_TYPE_ALL,	UL_("Set Speed")},
	{CMD_TEMPO,			0,0,		0,	MOD_TYPE_ALL,	UL_("Set Tempo")},
	{CMD_TREMOR,		0,0,		0,	MOD_TYPE_NOMOD,	UL_("Tremor")},
	{CMD_CHANNELVOLUME,	0,0,		0,	MOD_TYPE_S3MITMPT,	UL_("Set Channel Volume")},
	{CMD_CHANNELVOLSLIDE,0,0,		0,	MOD_TYPE_S3MITMPT,	UL_("Channel Volume Slide")},
	{CMD_GLOBALVOLUME,	0,0,		0,	MOD_TYPE_NOMOD,	UL_("Set Global Volume")},
	{CMD_GLOBALVOLSLIDE,0,0,		0,	MOD_TYPE_NOMOD,	UL_("Global Volume Slide")},
	{CMD_KEYOFF,		0,0,		0,	MOD_TYPE_XM,	UL_("Key Off")},
	{CMD_FINEVIBRATO,	0,0,		0,	MOD_TYPE_S3MITMPT,	UL_("Fine Vibrato")},
	{CMD_PANBRELLO,		0,0,		0,	MOD_TYPE_NOMOD,	UL_("Panbrello")},
	{CMD_PANNINGSLIDE,	0,0,		0,	MOD_TYPE_NOMOD,	UL_("Panning Slide")},
	{CMD_SETENVPOSITION,0,0,		0,	MOD_TYPE_XM,	UL_("Envelope position")},
	{CMD_MIDI,			0,0,		0x7F,	MOD_TYPE_NOMOD,	UL_("MIDI Macro")},
	{CMD_SMOOTHMIDI,	0,0,		0x7F,	MOD_TYPE_XMITMPT,	UL_("Smooth MIDI Macro")},
	// Extended MOD/XM effects
	{CMD_MODCMDEX,		0xF0,0x00,	0,	MOD_TYPE_MOD,	UL_("Set Filter")},
	{CMD_MODCMDEX,		0xF0,0x10,	0,	MOD_TYPE_MODXM,	UL_("Fine Porta Up")},
	{CMD_MODCMDEX,		0xF0,0x20,	0,	MOD_TYPE_MODXM,	UL_("Fine Porta Down")},
	{CMD_MODCMDEX,		0xF0,0x30,	0,	MOD_TYPE_MODXM,	UL_("Glissando Control")},
	{CMD_MODCMDEX,		0xF0,0x40,	0,	MOD_TYPE_MODXM,	UL_("Vibrato Waveform")},
	{CMD_MODCMDEX,		0xF0,0x50,	0,	MOD_TYPE_MODXM,	UL_("Set Finetune")},
	{CMD_MODCMDEX,		0xF0,0x60,	0,	MOD_TYPE_MODXM,	UL_("Pattern Loop")},
	{CMD_MODCMDEX,		0xF0,0x70,	0,	MOD_TYPE_MODXM,	UL_("Tremolo Waveform")},
	{CMD_MODCMDEX,		0xF0,0x80,	0,	MOD_TYPE_MODXM,	UL_("Set Panning")},
	{CMD_MODCMDEX,		0xF0,0x90,	0,	MOD_TYPE_MODXM,	UL_("Retrigger Note")},
	{CMD_MODCMDEX,		0xF0,0xA0,	0,	MOD_TYPE_MODXM,	UL_("Fine Volslide Up")},
	{CMD_MODCMDEX,		0xF0,0xB0,	0,	MOD_TYPE_MODXM,	UL_("Fine Volslide Down")},
	{CMD_MODCMDEX,		0xF0,0xC0,	0,	MOD_TYPE_MODXM,	UL_("Note Cut")},
	{CMD_MODCMDEX,		0xF0,0xD0,	0,	MOD_TYPE_MODXM,	UL_("Note Delay")},
	{CMD_MODCMDEX,		0xF0,0xE0,	0,	MOD_TYPE_MODXM,	UL_("Pattern Delay")},
	{CMD_MODCMDEX,		0xF0,0xF0,	0,	MOD_TYPE_XM,	UL_("Set Active Macro")},
	{CMD_MODCMDEX,		0xF0,0xF0,	0,	MOD_TYPE_MOD,	UL_("Invert Loop")},
	// Extended S3M/IT effects
	{CMD_S3MCMDEX,		0xF0,0x10,	0,	MOD_TYPE_S3MITMPT,	UL_("Glissando Control")},
	{CMD_S3MCMDEX,		0xF0,0x20,	0,	MOD_TYPE_S3M,		UL_("Set Finetune")},
	{CMD_S3MCMDEX,		0xF0,0x30,	0,	MOD_TYPE_S3MITMPT,	UL_("Vibrato Waveform")},
	{CMD_S3MCMDEX,		0xF0,0x40,	0,	MOD_TYPE_S3MITMPT,	UL_("Tremolo Waveform")},
	{CMD_S3MCMDEX,		0xF0,0x50,	0,	MOD_TYPE_S3MITMPT,	UL_("Panbrello Waveform")},
	{CMD_S3MCMDEX,		0xF0,0x60,	0,	MOD_TYPE_S3MITMPT,	UL_("Fine Pattern Delay")},
	{CMD_S3MCMDEX,		0xF0,0x80,	0,	MOD_TYPE_S3MITMPT,	UL_("Set Panning")},
	{CMD_S3MCMDEX,		0xF0,0xA0,	0,	MOD_TYPE_ITMPT,		UL_("Set High Offset")},
	{CMD_S3MCMDEX,		0xF0,0xB0,	0,	MOD_TYPE_S3MITMPT,	UL_("Pattern Loop")},
	{CMD_S3MCMDEX,		0xF0,0xC0,	0,	MOD_TYPE_S3MITMPT,	UL_("Note Cut")},
	{CMD_S3MCMDEX,		0xF0,0xD0,	0,	MOD_TYPE_S3MITMPT,	UL_("Note Delay")},
	{CMD_S3MCMDEX,		0xF0,0xE0,	0,	MOD_TYPE_S3MITMPT,	UL_("Pattern Delay")},
	{CMD_S3MCMDEX,		0xF0,0xF0,	0,	MOD_TYPE_ITMPT,		UL_("Set Active Macro")},
	// MPT XM extensions and special effects
	{CMD_XFINEPORTAUPDOWN,0xF0,0x10,0,	MOD_TYPE_XM,	UL_("Extra Fine Porta Up")},
	{CMD_XFINEPORTAUPDOWN,0xF0,0x20,0,	MOD_TYPE_XM,	UL_("Extra Fine Porta Down")},
	{CMD_XFINEPORTAUPDOWN,0xF0,0x50,0,	MOD_TYPE_XM,	UL_("Panbrello Waveform")},
	{CMD_XFINEPORTAUPDOWN,0xF0,0x60,0,	MOD_TYPE_XM,	UL_("Fine Pattern Delay")},
	{CMD_XFINEPORTAUPDOWN,0xF0,0x90,0,	MOD_TYPE_XM,	UL_("Sound Control")},
	{CMD_XFINEPORTAUPDOWN,0xF0,0xA0,0,	MOD_TYPE_XM,	UL_("Set High Offset")},
	// MPT IT extensions and special effects
	{CMD_S3MCMDEX,		0xF0,0x90,	0,	MOD_TYPE_S3MITMPT,	UL_("Sound Control")},
	{CMD_S3MCMDEX,		0xF0,0x70,	0,	MOD_TYPE_ITMPT,	UL_("Instr. Control")},
	{CMD_DELAYCUT,		0x00,0x00,	0,	MOD_TYPE_MPT,	UL_("Note Delay and Cut")},
	{CMD_XPARAM,		0,0,	0,	MOD_TYPE_XMITMPT,	UL_("Parameter Extension")},
	{CMD_NOTESLIDEUP,		0,0,	0,	MOD_TYPE_IMF | MOD_TYPE_PTM,	UL_("Note Slide Up")}, // IMF / PTM effect
	{CMD_NOTESLIDEDOWN,		0,0,	0,	MOD_TYPE_IMF | MOD_TYPE_PTM,	UL_("Note Slide Down")}, // IMF / PTM effect
	{CMD_NOTESLIDEUPRETRIG,	0,0,	0,	MOD_TYPE_PTM,	UL_("Note Slide Up + Retrigger Note")}, // PTM effect
	{CMD_NOTESLIDEDOWNRETRIG,0,0,	0,	MOD_TYPE_PTM,	UL_("Note Slide Down + Retrigger Note")}, // PTM effect
	{CMD_REVERSEOFFSET,		0,0,	0,	MOD_TYPE_PTM,	UL_("Revert Sample + Offset")}, // PTM effect
	{CMD_DBMECHO,			0,0,	0,	MOD_TYPE_DBM,	UL_("Echo Enable")}, // DBM effect
	{CMD_OFFSETPERCENTAGE,	0,0,	0,	MOD_TYPE_PLM,	UL_("Offset (Percentage)")}, // PLM effect
	{CMD_FINETUNE,			0,0,	0,	MOD_TYPE_MPT,	UL_("Finetune")},
	{CMD_FINETUNE_SMOOTH,	0,0,	0,	MOD_TYPE_MPT,	UL_("Finetune (Smooth)")},
	{CMD_DUMMY,	0,0,	0,	MOD_TYPE_NONE,	UL_("Empty") },
	{CMD_DIGIREVERSESAMPLE, 0, 0, 0, MOD_TYPE_NONE, UL_("Reverse Sample")}, // DIGI effect
	{CMD_VOLUME8, 0, 0, 0, MOD_TYPE_NONE, UL_("Set 8-bit Volume")},
	{CMD_HMN_MEGA_ARP, 0, 0, 0, MOD_TYPE_NONE, UL_("His Master's Noise Mega-Arpeggio")},
	{CMD_MED_SYNTH_JUMP, 0, 0, 0, MOD_TYPE_NONE, UL_("Synth Jump / MIDI Panning")},
	{CMD_AUTO_VOLUMESLIDE, 0, 0, 0, MOD_TYPE_NONE, UL_("Automatic Volume Slide")},
	{CMD_AUTO_PORTAUP, 0, 0, 0, MOD_TYPE_NONE, UL_("Automatic Portamento Up")},
	{CMD_AUTO_PORTADOWN, 0, 0, 0, MOD_TYPE_NONE, UL_("Automatic Portamento Down")},
	{CMD_AUTO_PORTAUP_FINE, 0, 0, 0, MOD_TYPE_NONE, UL_("Automatic Fine Portamento Up")},
	{CMD_AUTO_PORTADOWN_FINE, 0, 0, 0, MOD_TYPE_NONE, UL_("Automatic Fine Portamento Down")},
	{CMD_AUTO_PORTAMENTO_FC, 0, 0, 0, MOD_TYPE_NONE, UL_("Automatic Portamento (Future Composer)")},
	{CMD_TONEPORTA_DURATION, 0, 0, 0, MOD_TYPE_NONE, UL_("Tone Portamento with Duration")},
	{CMD_VOLUMEDOWN_DURATION, 0, 0, 0, MOD_TYPE_NONE, UL_("Channel Volume Down with Duration")},
	{CMD_VOLUMEDOWN_ETX, 0, 0, 0, MOD_TYPE_NONE, UL_("ETX Volume Slide Down")},
};


static mpt::ustring FormatPanning(int32 value, int32 center, bool hex = false)
{
	mpt::ustring s = hex ? mpt::ufmt::HEX(value) : mpt::ufmt::dec(value);
	if(value == center)
		s += UL_(" (Center)");
	else
		s += MPT_UFORMAT(" ({} {})")(std::abs(value - center), mpt::ustring(1, (value < center) ? u8'L' : u8'R'));
	return s;
}


uint32 EffectInfo::GetNumEffects() const
{
	return static_cast<uint32>(std::size(gFXInfo));
}


bool EffectInfo::IsExtendedEffect(uint32 ndx) const
{
	return ((ndx < std::size(gFXInfo)) && (gFXInfo[ndx].paramMask));
}


bool EffectInfo::GetEffectName(mpt::ustring &pszDescription, ModCommand::COMMAND command, uint32 param, bool addCommandFormat) const
{
	bool bSupported;
	uint32 fxndx = static_cast<uint32>(std::size(gFXInfo));
	pszDescription.clear();
	for (uint32 i = 0; i < std::size(gFXInfo); i++)
	{
		if ((command == gFXInfo[i].effect) // Effect
			&& ((param & gFXInfo[i].paramMask) == gFXInfo[i].paramValue)) // Value
		{
			fxndx = i;
			// if format is compatible, everything is fine. if not, let's still search
			// for another command. this fixes searching for the EFx command, which
			// does different things in MOD format.
			if((sndFile.GetType() & gFXInfo[i].supportedFormats))
				break;
		}
	}
	if (fxndx == std::size(gFXInfo)) return false;
	bSupported = ((sndFile.GetType() & gFXInfo[fxndx].supportedFormats));
	if (gFXInfo[fxndx].name)
	{
		if (addCommandFormat && bSupported)
		{
			pszDescription = mfcless::PrintF(UL_("%c%c%c: ")
				, sndFile.GetModSpecifications().GetEffectLetter(command)
				, ((gFXInfo[fxndx].paramMask & 0xF0) == 0xF0) ? szHexChar[gFXInfo[fxndx].paramValue >> 4] : 'x'
				, ((gFXInfo[fxndx].paramMask & 0x0F) == 0x0F) ? szHexChar[gFXInfo[fxndx].paramValue & 0x0F] : 'x'
				);
		}
		pszDescription += gFXInfo[fxndx].name;
	}
	return bSupported;
}


int32 EffectInfo::GetIndexFromEffect(ModCommand::COMMAND command, ModCommand::PARAM param) const
{
	uint32 ndx = static_cast<uint32>(std::size(gFXInfo));
	for (uint32 i = 0; i < std::size(gFXInfo); i++)
	{
		if ((command == gFXInfo[i].effect) // Effect
			&& ((param & gFXInfo[i].paramMask) == gFXInfo[i].paramValue)) // Value
		{
			ndx = i;
			if((sndFile.GetType() & gFXInfo[i].supportedFormats))
				break; // found fitting format; this is correct for sure
		}
	}
	return ndx;
}


//Returns command and corrects parameter refParam if necessary
EffectCommand EffectInfo::GetEffectFromIndex(uint32 ndx, ModCommand::PARAM &refParam) const
{
	if (ndx >= std::size(gFXInfo))
	{
		refParam = 0;
		return CMD_NONE;
	}

	// Cap parameter to match FX if necessary.
	if (gFXInfo[ndx].paramMask)
	{
		if (refParam < gFXInfo[ndx].paramValue)
		{
			refParam = gFXInfo[ndx].paramValue;	 // for example: delay with param < D0 becomes SD0
		} else if (refParam > gFXInfo[ndx].paramValue + 15)
		{
			refParam = gFXInfo[ndx].paramValue + 15; // for example: delay with param > DF becomes SDF
		}
	}
	if (gFXInfo[ndx].paramLimit)
	{
		// used for Zxx macro control in parameter editor: limit to 7F max.
		LimitMax(refParam, gFXInfo[ndx].paramLimit);
	}

	return gFXInfo[ndx].effect;
}


EffectCommand EffectInfo::GetEffectFromIndex(uint32 ndx) const
{
	if (ndx >= std::size(gFXInfo))
	{
		return CMD_NONE;
	}

	return gFXInfo[ndx].effect;
}

uint32 EffectInfo::GetEffectMaskFromIndex(uint32 ndx) const
{
	if (ndx >= std::size(gFXInfo))
	{
		return 0;
	}

	return gFXInfo[ndx].paramValue;

}

bool EffectInfo::GetEffectInfo(uint32 ndx, mpt::ustring *s, bool addCommandFormat, ModCommand::PARAM *prangeMin, ModCommand::PARAM *prangeMax) const
{

	if (s) s->clear();
	if (prangeMin) *prangeMin = 0;
	if (prangeMax) *prangeMax = 0;
	if ((ndx >= std::size(gFXInfo)) || (!(sndFile.GetType() & gFXInfo[ndx].supportedFormats))) return false;
	if (s) GetEffectName(*s, gFXInfo[ndx].effect, gFXInfo[ndx].paramValue, addCommandFormat);
	if ((prangeMin) && (prangeMax))
	{
		ModCommand::PARAM nmin = 0, nmax = 0xFF;
		if (gFXInfo[ndx].paramMask == 0xF0)
		{
			nmin = gFXInfo[ndx].paramValue;
			nmax = nmin | 0x0F;
		}
		switch(gFXInfo[ndx].effect)
		{
		case CMD_ARPEGGIO:
			if (sndFile.GetType() & (MOD_TYPE_MOD | MOD_TYPE_XM)) nmin = 1;
			break;
		case CMD_VOLUME:
		case CMD_CHANNELVOLUME:
			nmax = 0x40;
			break;
		case CMD_SPEED:
			nmin = 1;
			if (sndFile.GetType() & (MOD_TYPE_XM | MOD_TYPE_MOD)) nmax = 0x1F;
			else nmax = 0xFF;
			break;
		case CMD_TEMPO:
			if (sndFile.GetType() & (MOD_TYPE_XM | MOD_TYPE_MOD)) nmin = 0x20;
			else nmin = 0;
			break;
		case CMD_VOLUMESLIDE:
		case CMD_TONEPORTAVOL:
		case CMD_VIBRATOVOL:
		case CMD_GLOBALVOLSLIDE:
		case CMD_CHANNELVOLSLIDE:
		case CMD_PANNINGSLIDE:
			nmax = (sndFile.GetType() & MOD_TYPE_S3MITMPT) ? 59 : 30;
			break;
		case CMD_PANNING8:
			if (sndFile.GetType() & (MOD_TYPE_S3M)) nmax = 0x81;
			else nmax = 0xFF;
			break;
		case CMD_GLOBALVOLUME:
			nmax = (sndFile.GetType() & (MOD_TYPE_IT | MOD_TYPE_MPT)) ? 128 : 64;
			break;

		case CMD_MODCMDEX:
			// adjust waveform types for XM/MOD
			if(gFXInfo[ndx].paramValue == 0x40 || gFXInfo[ndx].paramValue == 0x70) nmax = gFXInfo[ndx].paramValue | 0x07;
			if(gFXInfo[ndx].paramValue == 0x00) nmax = 1;
			break;
		case CMD_S3MCMDEX:
			// adjust waveform types for IT/S3M
			if(gFXInfo[ndx].paramValue >= 0x30 && gFXInfo[ndx].paramValue <= 0x50) nmax = gFXInfo[ndx].paramValue | ((sndFile.m_playBehaviour[kITVibratoTremoloPanbrello] || sndFile.GetType() == MOD_TYPE_S3M) ? 0x03 : 0x07);
			break;
		case CMD_PATTERNBREAK:
			// no big patterns in MOD/S3M files, and FT2 disallows breaking to rows > 63
			if(sndFile.GetType() & (MOD_TYPE_MOD | MOD_TYPE_S3M | MOD_TYPE_XM))
				nmax = 63;
			break;
		default:
			break;
		}
		*prangeMin = nmin;
		*prangeMax = nmax;
	}
	return true;
}


uint32 EffectInfo::MapValueToPos(uint32 ndx, uint32 param) const
{
	uint32 pos;

	if (ndx >= std::size(gFXInfo)) return 0;
	pos = param;
	if (gFXInfo[ndx].paramMask == 0xF0)
	{
		pos &= 0x0F;
		pos |= gFXInfo[ndx].paramValue;
	}
	switch(gFXInfo[ndx].effect)
	{
	case CMD_VOLUMESLIDE:
	case CMD_TONEPORTAVOL:
	case CMD_VIBRATOVOL:
	case CMD_GLOBALVOLSLIDE:
	case CMD_CHANNELVOLSLIDE:
	case CMD_PANNINGSLIDE:
		if (sndFile.GetType() & MOD_TYPE_S3MITMPT)
		{
			if (!param)
				pos = 29;
			else if (((param & 0x0F) == 0x0F) && (param & 0xF0))
				pos = 29 + (param >> 4);	// Fine Up
			else if (((param & 0xF0) == 0xF0) && (param & 0x0F))
				pos = 29 - (param & 0x0F);	// Fine Down
			else if (param & 0x0F)
				pos = 15 - (param & 0x0F);	// Down
			else
				pos = (param >> 4) + 44;	// Up
		} else
		{
			if (param & 0x0F)
				pos = 15 - (param & 0x0F);
			else
				pos = (param >> 4) + 15;
		}
		break;
	case CMD_PANNING8:
		if(sndFile.GetType() == MOD_TYPE_S3M)
		{
			pos = Clamp(param, 0u, 0x80u);
			if(param == 0xA4)
				pos = 0x81;
		}
		break;
	default:
		break;
	}
	return pos;
}


uint32 EffectInfo::MapPosToValue(uint32 ndx, uint32 pos) const
{
	uint32 param;

	if (ndx >= std::size(gFXInfo)) return 0;
	param = pos;
	if (gFXInfo[ndx].paramMask == 0xF0) param |= gFXInfo[ndx].paramValue;
	switch(gFXInfo[ndx].effect)
	{
	case CMD_VOLUMESLIDE:
	case CMD_TONEPORTAVOL:
	case CMD_VIBRATOVOL:
	case CMD_GLOBALVOLSLIDE:
	case CMD_CHANNELVOLSLIDE:
	case CMD_PANNINGSLIDE:
		if (sndFile.GetType() & MOD_TYPE_S3MITMPT)
		{
			if (pos < 15)
				param = 15 - pos;
			else if (pos < 29)
				param = (29 - pos) | 0xF0;
			else if (pos == 29)
				param = 0;
			else if (pos <= 44)
				param = ((pos - 29) << 4) | 0x0F;
			else
				if (pos <= 59) param = (pos - 44) << 4;
		} else
		{
			if (pos < 15)
				param = 15 - pos;
			else
				param = (pos - 15) << 4;
		}
		break;
	case CMD_PANNING8:
		if(sndFile.GetType() == MOD_TYPE_S3M)
			param = (pos <= 0x80) ? pos : 0xA4;
		break;
	default:
		break;
	}
	return param;
}


bool EffectInfo::GetEffectNameEx(mpt::ustring &pszName, const ModCommand &m, uint32 param, CHANNELINDEX chn) const
{
	mpt::ustring s;
	const mpt::uchar *continueOrIgnore;

	auto ndx = GetIndexFromEffect(m.command, static_cast<ModCommand::PARAM>(param));

	if(ndx < 0 || static_cast<std::size_t>(ndx) >= std::size(gFXInfo) || !gFXInfo[ndx].name)
		return false;
	pszName = mpt::ustring{gFXInfo[ndx].name} + UL_(": ");

	// for effects that don't have effect memory in MOD format.
	if(sndFile.GetType() == MOD_TYPE_MOD)
		continueOrIgnore = UL_("ignore");
	else
		continueOrIgnore = UL_("continue");

	const mpt::uchar *plusChar = UL_("+"), *minusChar = UL_("-");

	switch(gFXInfo[ndx].effect)
	{
	case CMD_ARPEGGIO:
		if(sndFile.GetType() == MOD_TYPE_XM)	// XM also ignores this!
			continueOrIgnore = UL_("ignore");

		if(param)
			s = mfcless::PrintF(UL_("note+%d note+%d"), param >> 4, param & 0x0F);
		else
			s = continueOrIgnore;
		break;

	case CMD_PORTAMENTOUP:
	case CMD_PORTAMENTODOWN:
		if(param)
		{
			mpt::uchar sign = (gFXInfo[ndx].effect == CMD_PORTAMENTOUP) ? u8'+' : u8'-';

			if((sndFile.UseCombinedPortamentoCommands()) && ((param & 0xF0) == 0xF0))
				s = mfcless::PrintF(UL_("fine %c%d"), sign, (param & 0x0F));
			else if((sndFile.UseCombinedPortamentoCommands()) && ((param & 0xF0) == 0xE0))
				s = mfcless::PrintF(UL_("extra fine %c%d"), sign, (param & 0x0F));
			else
				s = mfcless::PrintF(UL_("%c%d"), sign, param);
		} else
		{
			s = continueOrIgnore;
		}
		break;

	case CMD_TONEPORTAMENTO:
		if (param)
			s = mfcless::PrintF(UL_("speed %d"), param);
		else
			s = UL_("continue");
		break;

	case CMD_VIBRATO:
	case CMD_TREMOLO:
	case CMD_PANBRELLO:
	case CMD_FINEVIBRATO:
		if(param && !(param & 0xF0))
			s = mfcless::PrintF(UL_("speed=continue depth=%d"), param & 0x0F);
		else if(param && !(param & 0x0F))
			s = mfcless::PrintF(UL_("speed=%d depth=continue"), param >> 4);
		else if(param)
			s = mfcless::PrintF(UL_("speed=%d depth=%d"), param >> 4, param & 0x0F);
		else
			s = UL_("continue");
		break;

	case CMD_SPEED:
		s = mfcless::PrintF(UL_("%d ticks/row"), param);
		break;

	case CMD_TEMPO:
		if (param == 0)
			s = UL_("continue");
		else if (param < 0x10)
			s = mfcless::PrintF(UL_("-%d bpm (slower)"), param & 0x0F);
		else if (param < 0x20)
			s = mfcless::PrintF(UL_("+%d bpm (faster)"), param & 0x0F);
		else
			s = mfcless::PrintF(UL_("%d bpm"), param);
		break;

	case CMD_PANNING8:
		if(sndFile.GetType() == MOD_TYPE_S3M && param == 0xA4)
			s = UL_("Surround");
		else
			s = FormatPanning(param, (sndFile.GetType() == MOD_TYPE_S3M) ? 0x40 : 0x80).c_str();
		break;

	case CMD_RETRIG:
		switch(param >> 4)
		{
		case  0:
			if(sndFile.GetType() & MOD_TYPE_XM)
				s = UL_("continue");
			else
				s = UL_("vol *1");
			break;
		case  1: s = UL_("vol -1"); break;
		case  2: s = UL_("vol -2"); break;
		case  3: s = UL_("vol -4"); break;
		case  4: s = UL_("vol -8"); break;
		case  5: s = UL_("vol -16"); break;
		case  6: s = UL_("vol *0.66"); break;
		case  7: s = UL_("vol *0.5"); break;
		case  8: s = UL_("vol *1"); break;
		case  9: s = UL_("vol +1"); break;
		case 10: s = UL_("vol +2"); break;
		case 11: s = UL_("vol +4"); break;
		case 12: s = UL_("vol +8"); break;
		case 13: s = UL_("vol +16"); break;
		case 14: s = UL_("vol *1.5"); break;
		case 15: s = UL_("vol *2"); break;
		}
		s += mfcless::PrintF(UL_(" speed %d"), param & 0x0F);
		break;

	case CMD_VOLUMESLIDE:
		if(sndFile.GetType() == MOD_TYPE_MOD && !param)
		{
			s = continueOrIgnore;
			break;
		}
		[[fallthrough]];
	case CMD_TONEPORTAVOL:
	case CMD_VIBRATOVOL:
	case CMD_GLOBALVOLSLIDE:
	case CMD_CHANNELVOLSLIDE:
	case CMD_PANNINGSLIDE:
		if(gFXInfo[ndx].effect == CMD_PANNINGSLIDE)
		{
			if(sndFile.GetType() == MOD_TYPE_XM)
			{
				plusChar = UL_("-> ");
				minusChar = UL_("<- ");
			} else
			{
				plusChar = UL_("<- ");
				minusChar = UL_("-> ");
			}
		}

		if (!param)
		{
			s = mfcless::PrintF(UL_("continue"));
		} else if ((sndFile.GetType() & MOD_TYPE_S3MITMPT) && ((param & 0x0F) == 0x0F) && (param & 0xF0))
		{
			s = mfcless::PrintF(UL_("fine %s%d"), plusChar, param >> 4);
		} else if ((sndFile.GetType() & MOD_TYPE_S3MITMPT) && ((param & 0xF0) == 0xF0) && (param & 0x0F))
		{
			s = mfcless::PrintF(UL_("fine %s%d"), minusChar, param & 0x0F);
		} else if ((param & 0x0F) != param && (param & 0xF0) != param)	// both nibbles are set.
		{
			s = UL_("undefined");
		} else if (param & 0x0F)
		{
			s = mfcless::PrintF(UL_("%s%d"), minusChar, param & 0x0F);
		} else
		{
			s = mfcless::PrintF(UL_("%s%d"), plusChar, param >> 4);
		}
		break;

	case CMD_PATTERNBREAK:
		pszName = mfcless::PrintF(UL_("Break to row %u"), param);
		break;

	case CMD_POSITIONJUMP:
		pszName = mfcless::PrintF(UL_("Jump to position %u"), param);
		break;

	case CMD_OFFSET:
		if (m.volcmd == VOLCMD_OFFSET && m.vol == 0)
		{
			pszName = mfcless::PrintF(UL_("Percentage: %u%%"), Util::muldivr_unsigned(m.param, 100, 256));
		} else if(param || m.volcmd == VOLCMD_OFFSET)
		{
			if (m.volcmd == VOLCMD_OFFSET && m.IsNote() && m.instr)
			{
				if(SAMPLEINDEX smp = sndFile.GetSampleIndex(m.note, m.instr); smp > 0 && smp <= sndFile.GetNumSamples())
				{
					const ModSample &sample = sndFile.GetSample(smp);
					if(m.vol > 0 && m.vol <= std::size(sample.cues))
						param += sample.cues[m.vol - 1];
				}
			}
			pszName = mfcless::PrintF(UL_("Set Offset to %s"), mpt::ufmt::dec(3, UL_(","), param).c_str());
		} else
		{
			s = UL_("continue");
		}
		break;

	case CMD_CHANNELVOLUME:
	case CMD_GLOBALVOLUME:
		{
			ModCommand::PARAM minVal = 0, maxVal = 128;
			GetEffectInfo(ndx, nullptr, false, &minVal, &maxVal);
			if((sndFile.GetType() & (MOD_TYPE_IT | MOD_TYPE_MPT | MOD_TYPE_S3M)) && param > maxVal)
				s = UL_("undefined");
			else
				s = mfcless::PrintF(UL_("%u"), std::min(static_cast<uint32>(param), static_cast<uint32>(maxVal)));
		}
		break;

	case CMD_TREMOR:
		if(param)
		{
			uint8 ontime = (uint8)(param >> 4), offtime = (uint8)(param & 0x0F);
			if(sndFile.m_SongFlags[SONG_ITOLDEFFECTS] || (sndFile.GetType() & MOD_TYPE_XM))
			{
				ontime++;
				offtime++;
			} else
			{
				if(ontime == 0) ontime = 1;
				if(offtime == 0) offtime = 1;
			}
			s = mfcless::PrintF(UL_("ontime %u, offtime %u"), ontime, offtime);
		} else
		{
			s = UL_("continue");
		}
		break;

	case CMD_SETENVPOSITION:
		s = mfcless::PrintF(UL_("Tick %u"), param);
		break;

	case CMD_MIDI:
	case CMD_SMOOTHMIDI:
		if (param < 0x80)
		{
			if(chn != CHANNELINDEX_INVALID)
			{
				const uint8 macroIndex = sndFile.m_PlayState.Chn[chn].nActiveMacro;
				const PLUGINDEX plugin = sndFile.GetBestPlugin(sndFile.m_PlayState.Chn[chn], chn, PrioritiseChannel, EvenIfMuted) - 1;
				IMixPlugin *pPlugin = (plugin < MAX_MIXPLUGINS ? sndFile.m_MixPlugins[plugin].pMixPlugin : nullptr);
				pszName = mfcless::PrintF(UL_("SFx MIDI Macro z=%d (SF%X: %s)"), param, macroIndex, sndFile.m_MidiCfg.GetParameteredMacroName(macroIndex, pPlugin).c_str());
			} else
			{
				pszName = mfcless::PrintF(UL_("SFx MIDI Macro z=%02X (%d)"), param, param);
			}
		} else
		{
			pszName = mfcless::PrintF(UL_("Fixed Macro Z%02X"), param);
		}
		break;

	case CMD_DELAYCUT:
		pszName = mfcless::PrintF(UL_("Note delay: %d, cut after %d ticks"), (param >> 4), (param & 0x0F));
		break;

	case CMD_FINETUNE:
	case CMD_FINETUNE_SMOOTH:
		{
			int8 pwd = 1;
			const mpt::uchar *unit = UL_(" cents");
			if(m.instr > 0 && m.instr <= sndFile.GetNumInstruments() && sndFile.Instruments[m.instr] != nullptr)
				pwd = sndFile.Instruments[m.instr]->midiPWD;
			else if(chn != CHANNELINDEX_INVALID && sndFile.m_PlayState.Chn[chn].pModInstrument != nullptr)
				pwd = sndFile.m_PlayState.Chn[chn].pModInstrument->midiPWD;
			else if(sndFile.GetNumInstruments())
				unit = UL_("");

			pszName = MPT_UFORMAT("Finetune{}: {}{}{}")(
				mpt::ustring(gFXInfo[ndx].effect == CMD_FINETUNE ? UL_("") : UL_(" (Smooth)")),
				mpt::ustring(param >= 0x8000 ? UL_("+") : UL_("")),
				mpt::ufmt::val((static_cast<int32>(param) - 0x8000) * pwd / 327.68),
				mpt::ustring(unit));
		}
		break;

	default:
		if (gFXInfo[ndx].paramMask == 0xF0)
		{
			// Sound control names
			if (((gFXInfo[ndx].effect == CMD_XFINEPORTAUPDOWN) || (gFXInfo[ndx].effect == CMD_S3MCMDEX))
				&& ((gFXInfo[ndx].paramValue & 0xF0) == 0x90) && ((param & 0xF0) == 0x90))
			{
				switch(param & 0x0F)
				{
				case 0x00:	s = UL_("90: Surround Off"); break;
				case 0x01:	s = UL_("91: Surround On"); break;
				case 0x08:	s = UL_("98: Reverb Off"); break;
				case 0x09:	s = UL_("99: Reverb On"); break;
				case 0x0A:	s = UL_("9A: Center surround"); break;
				case 0x0B:	s = UL_("9B: Quad surround"); break;
				case 0x0C:	s = UL_("9C: Global filters"); break;
				case 0x0D:	s = UL_("9D: Local filters"); break;
				case 0x0E:	s = UL_("9E: Play Forward"); break;
				case 0x0F:	s = UL_("9F: Play Backward"); break;
				default:	s = mfcless::PrintF(UL_("%02X: undefined"), param);
				}
			} else
				if (((gFXInfo[ndx].effect == CMD_XFINEPORTAUPDOWN) || (gFXInfo[ndx].effect == CMD_S3MCMDEX))
					&& ((gFXInfo[ndx].paramValue & 0xF0) == 0x70) && ((param & 0xF0) == 0x70))
				{
					switch(param & 0x0F)
					{
					case 0x00:	s = UL_("70: Past note cut"); break;
					case 0x01:	s = UL_("71: Past note off"); break;
					case 0x02:	s = UL_("72: Past note fade"); break;
					case 0x03:	s = UL_("73: NNA note cut"); break;
					case 0x04:	s = UL_("74: NNA continue"); break;
					case 0x05:	s = UL_("75: NNA note off"); break;
					case 0x06:	s = UL_("76: NNA note fade"); break;
					case 0x07:	s = UL_("77: Volume Env Off"); break;
					case 0x08:	s = UL_("78: Volume Env On"); break;
					case 0x09:	s = UL_("79: Pan Env Off"); break;
					case 0x0A:	s = UL_("7A: Pan Env On"); break;
					case 0x0B:	s = UL_("7B: Pitch Env Off"); break;
					case 0x0C:	s = UL_("7C: Pitch Env On"); break;
					case 0x0D:	if(sndFile.GetType() == MOD_TYPE_MPT) { s = UL_("7D: Force Pitch Env"); break; }
								[[fallthrough]];
					case 0x0E:	if(sndFile.GetType() == MOD_TYPE_MPT) { s = UL_("7E: Force Filter Env"); break; }
								[[fallthrough]];
					default:	s = mfcless::PrintF(UL_("%02X: undefined"), param); break;
					}
				} else
				{
					s = mfcless::PrintF(UL_("%d"), param & 0x0F);
					if(gFXInfo[ndx].effect == CMD_S3MCMDEX)
					{
						switch(param & 0xF0)
						{
						case 0x10: // glissando control
							if((param & 0x0F) == 0)
								s = UL_("smooth");
							else
								s = UL_("semitones");
							break;
						case 0x20: // set finetune
							s = mfcless::PrintF(UL_("%dHz"), S3MFineTuneTable[param & 0x0F]);
							break;
						case 0x30: // vibrato waveform
						case 0x40: // tremolo waveform
						case 0x50: // panbrello waveform
							if(((param & 0x0F) > 0x03) && sndFile.m_playBehaviour[kITVibratoTremoloPanbrello])
							{
								s = UL_("ignore");
								break;
							}
							switch(param & 0x0F)
							{
							case 0x00: s = UL_("sine wave"); break;
							case 0x01: s = UL_("ramp down"); break;
							case 0x02: s = UL_("square wave"); break;
							case 0x03: s = UL_("random"); break;
							case 0x04: s = UL_("sine wave (cont.)"); break;
							case 0x05: s = UL_("ramp down (cont.)"); break;
							case 0x06: s = UL_("square wave (cont.)"); break;
							case 0x07: s = UL_("random (cont.)"); break;
							default: s = UL_("ignore"); break;
							}
							break;

						case 0x60: // fine pattern delay (ticks)
							s += UL_(" ticks");
							break;

						case 0x80: // panning
							s = FormatPanning(param & 0x0F, (param & 0x0F) < 8 ? 8 : 7).c_str();
							break;

						case 0xA0: // high offset
							s = mfcless::PrintF(UL_("+ %u samples"), (param & 0x0F) * 0x10000);
							break;

						case 0xB0: // pattern loop
							if((param & 0x0F) == 0x00)
								s = UL_("loop start");
							else
								s += UL_(" times");
							break;
						case 0xC0: // note cut
						case 0xD0: // note delay
							//IT compatibility 22. SD0 == SD1, SC0 == SC1
							if(((param & 0x0F) == 1) || ((param & 0x0F) == 0 && (sndFile.GetType() & (MOD_TYPE_IT | MOD_TYPE_MPT))))
								s = UL_("1 tick");
							else
								s += UL_(" ticks");
							break;
						case 0xE0: // pattern delay (rows)
							s += UL_(" rows");
							break;
						case 0xF0: // macro
							s = sndFile.m_MidiCfg.GetParameteredMacroName(param & 0x0F);
							break;
						default:
							break;
						}
					} else if(gFXInfo[ndx].effect == CMD_MODCMDEX)
					{
						switch(param & 0xF0)
						{
						case 0x00:
							// Filter
							if(param & 1)
								s = UL_("LED Filter Off");
							else
								s = UL_("LED Filter On");
							break;

						case 0x10:
						case 0x20:
						case 0xA0:
						case 0xB0:
							if(!(param & 0x0F) && sndFile.GetType() == MOD_TYPE_XM)
								s = UL_("continue");
							break;

						case 0x30: // glissando control
							if((param & 0x0F) == 0)
								s = UL_("smooth");
							else
								s = UL_("semitones");
							break;					
						case 0x40: // vibrato waveform
						case 0x70: // tremolo waveform
							switch(param & 0x0F)
							{
							case 0x00: case 0x08: s = UL_("sine wave"); break;
							case 0x01: case 0x09: s = UL_("ramp down"); break;
							case 0x02: case 0x0A: s = UL_("square wave"); break;
							case 0x03: case 0x0B: s = UL_("square wave"); break;

							case 0x04: case 0x0C: s = UL_("sine wave (cont.)"); break;
							case 0x05: case 0x0D: s = UL_("ramp down (cont.)"); break;
							case 0x06: case 0x0E: s = UL_("square wave (cont.)"); break;
							case 0x07: case 0x0F: s = UL_("square wave (cont.)"); break;
							}
							break;
						case 0x50: // set finetune
							{
								int8 nFinetune = (param & 0x0F);
								if(sndFile.GetType() & MOD_TYPE_XM)
								{
									// XM finetune
									nFinetune = (nFinetune - 8) * 16;
								} else
								{
									// MOD finetune
									if(nFinetune > 7) nFinetune -= 16;
								}
								s = mfcless::PrintF(UL_("%d"), nFinetune);
							}
							break;
						case 0x60: // pattern loop
							if((param & 0x0F) == 0x00)
								s = UL_("loop start");
							else
								s += UL_(" times");
							break;
						case 0x80: // panning
							s = FormatPanning(param & 0x0F, (param & 0x0F) < 8 ? 8 : 7).c_str();
							break;
						case 0x90: // retrigger
							s = mfcless::PrintF(UL_("speed %d"), param & 0x0F);
							break;
						case 0xC0: // note cut
						case 0xD0: // note delay
							s += UL_(" ticks");
							break;
						case 0xE0: // pattern delay (rows)
							s += UL_(" rows");
							break;
						case 0xF0:
							if(sndFile.GetType() == MOD_TYPE_MOD)
							{
								// invert loop
								if((param & 0x0F) == 0)
									s = UL_("Stop");
								else
									s = mfcless::PrintF(UL_("Speed %d (%d)"), param & 0x0F, ModEFxTable[param & 0x0F]);
							} else
							{
								// macro
								s = sndFile.m_MidiCfg.GetParameteredMacroName(param & 0x0F);
							}
							break;
						default:
							break;
						}
					}
				}

		} else
		{
			s = mfcless::PrintF(UL_("%u"), param);
		}
	}
	pszName += s;
	return true;
}


////////////////////////////////////////////////////////////////////////////////////////
// Volume column effects description

struct MPTVolCmdInfo
{
	VolumeCommand volCmd;               // VOLCMD_XXXX
	FlagSet<MODTYPE> supportedFormats;  // MOD_TYPE_XXX combo
	const mpt::uchar *name;                  // e.g. "Set Volume"
};

static constexpr MPTVolCmdInfo gVolCmdInfo[] =
{
	{VOLCMD_VOLUME,         MOD_TYPE_NOMOD,   UL_("Set Volume")},
	{VOLCMD_PANNING,        MOD_TYPE_NOMOD,   UL_("Set Panning")},
	{VOLCMD_VOLSLIDEUP,     MOD_TYPE_XMITMPT, UL_("Volume slide up")},
	{VOLCMD_VOLSLIDEDOWN,   MOD_TYPE_XMITMPT, UL_("Volume slide down")},
	{VOLCMD_FINEVOLUP,      MOD_TYPE_XMITMPT, UL_("Fine volume up")},
	{VOLCMD_FINEVOLDOWN,    MOD_TYPE_XMITMPT, UL_("Fine volume down")},
	{VOLCMD_VIBRATOSPEED,   MOD_TYPE_XM,      UL_("Vibrato speed")},
	{VOLCMD_VIBRATODEPTH,   MOD_TYPE_XMITMPT, UL_("Vibrato depth")},
	{VOLCMD_PANSLIDELEFT,   MOD_TYPE_XM,      UL_("Pan slide left")},
	{VOLCMD_PANSLIDERIGHT,  MOD_TYPE_XM,      UL_("Pan slide right")},
	{VOLCMD_TONEPORTAMENTO, MOD_TYPE_XMITMPT, UL_("Tone portamento")},
	{VOLCMD_PORTAUP,        MOD_TYPE_ITMPT,   UL_("Portamento up")},
	{VOLCMD_PORTADOWN,      MOD_TYPE_ITMPT,   UL_("Portamento down")},
	{VOLCMD_PLAYCONTROL,    MOD_TYPE_NONE,    UL_("Play Control")},
	{VOLCMD_OFFSET,         MOD_TYPE_MPT,     UL_("Sample Cue")},
};

static_assert(mpt::array_size<decltype(gVolCmdInfo)>::size == (MAX_VOLCMDS - 1));


uint32 EffectInfo::GetNumVolCmds() const
{
	return static_cast<uint32>(std::size(gVolCmdInfo));
}


int32 EffectInfo::GetIndexFromVolCmd(ModCommand::VOLCMD volcmd) const
{
	for (uint32 i = 0; i < std::size(gVolCmdInfo); i++)
	{
		if (gVolCmdInfo[i].volCmd == volcmd) return i;
	}
	return -1;
}


VolumeCommand EffectInfo::GetVolCmdFromIndex(uint32 ndx) const
{
	return (ndx < std::size(gVolCmdInfo)) ? gVolCmdInfo[ndx].volCmd : VOLCMD_NONE;
}


bool EffectInfo::GetVolCmdInfo(uint32 ndx, mpt::ustring *s, ModCommand::VOL *prangeMin, ModCommand::VOL *prangeMax) const
{
	if(s)
		s->clear();
	if(prangeMin)
		*prangeMin = 0;
	if(prangeMax)
		*prangeMax = 0;
	if(ndx >= std::size(gVolCmdInfo))
		return false;
	if(s)
		*s = mfcless::PrintF(UL_("%c: %s"), sndFile.GetModSpecifications().GetVolEffectLetter(GetVolCmdFromIndex(ndx)), gVolCmdInfo[ndx].name);
	if(prangeMin && prangeMax)
	{
		switch(gVolCmdInfo[ndx].volCmd)
		{
		case VOLCMD_VOLUME:
			*prangeMax = 64;
			break;
		case VOLCMD_PANNING:
			*prangeMax = (sndFile.GetType() & MOD_TYPE_XM) ? 15 : 64;
			break;
		default:
			*prangeMax = (sndFile.GetType() & MOD_TYPE_XM) ? 15 : 9;
			break;
		}
	}
	return (sndFile.GetType() & gVolCmdInfo[ndx].supportedFormats);
}


bool EffectInfo::GetVolCmdParamInfo(const ModCommand &m, mpt::ustring *s, bool hex) const
{
	if(s == nullptr)
		return false;
	s->clear();

	mpt::ustring volume;
	if(hex)
		volume = mpt::ufmt::HEX(m.vol);
	else
		volume = mpt::ufmt::dec(m.vol);

	if(hex && m.volcmd == VOLCMD_VOLUME)
		volume += MPT_UFORMAT(" ({})")(mpt::ufmt::dec(m.vol));

	switch(m.volcmd)
	{
	case VOLCMD_PANNING:
		*s = FormatPanning(m.vol, 32, hex).c_str();
		break;

	case VOLCMD_VOLSLIDEUP:
	case VOLCMD_VOLSLIDEDOWN:
	case VOLCMD_FINEVOLUP:
	case VOLCMD_FINEVOLDOWN:
		if(m.vol > 0 || sndFile.GetType() == MOD_TYPE_XM)
		{
			*s = ((m.volcmd == VOLCMD_VOLSLIDEUP || m.volcmd == VOLCMD_FINEVOLUP) ? u8'+' : u8'-')
				+ volume;
		} else
		{
			*s = UL_("continue");
		}
		break;

	case VOLCMD_PORTAUP:
	case VOLCMD_PORTADOWN:
	case VOLCMD_TONEPORTAMENTO:
		if(m.vol > 0)
		{
			ModCommand::PARAM param = m.vol << 2;
			ModCommand::COMMAND cmd = CMD_PORTAMENTOUP;
			if(m.volcmd == VOLCMD_PORTADOWN)
			{
				cmd = CMD_PORTAMENTODOWN;
			} else if(m.volcmd == VOLCMD_TONEPORTAMENTO)
			{
				cmd = CMD_TONEPORTAMENTO;
				if(sndFile.GetType() != MOD_TYPE_XM) param = ImpulseTrackerPortaVolCmd[m.vol & 0x0F];
				else param = m.vol << 4;
			}
			*s = volume;
			*s += mfcless::PrintF(UL_(" (%c%02X)"),
				sndFile.GetModSpecifications().GetEffectLetter(cmd),
				param);
		} else
		{
			*s = UL_("continue");
		}
		break;

	case VOLCMD_OFFSET:
		if(m.vol)
		{
			SAMPLEINDEX smp = m.instr;
			if(smp > 0 && smp <= sndFile.GetNumInstruments() && m.IsNote() && sndFile.Instruments[smp] != nullptr)
			{
				smp = sndFile.Instruments[smp]->Keyboard[m.note - NOTE_MIN];
			}
			*s = mfcless::PrintF(UL_("Cue %u: "), m.vol);
			if(smp > 0 && smp <= sndFile.GetNumSamples() && m.vol > 0 && m.vol <= std::size(sndFile.GetSample(smp).cues))
			{
				auto cue = sndFile.GetSample(smp).cues[m.vol - 1];
				if(cue < sndFile.GetSample(smp).nLength)
					s->append(mpt::ufmt::dec(3, UL_(","), sndFile.GetSample(smp).cues[m.vol - 1]));
				else
					s->append(UL_("unused"));
			} else
				s->append(UL_("unknown"));
		} else
		{
			if(m.command == CMD_OFFSET)
				*s = mfcless::PrintF(UL_("Percentage: %u%%"), Util::muldivr_unsigned(m.param, 100, 256));
			else
				*s = UL_("continue");
		}
		break;

	case VOLCMD_PLAYCONTROL:
		switch(m.vol)
		{
		case 0: *s = UL_("Pause Playback"); break;
		case 1: *s = UL_("Continue Playback"); break;
		case 2: *s = UL_("Play Forward"); break;
		case 3: *s = UL_("Play Backward"); break;
		case 4: *s = UL_("Switch Play Direction"); break;
		case 5: *s = UL_("Store Offset"); break;
		case 6: *s = UL_("Play From Offset"); break;
		default: *s = UL_("unused"); break;
		}
		break;
	default:
		*s = volume;
		break;
	}
	return true;
}

	// Map an effect value to slider position
uint32 EffectInfo::MapVolumeToPos(VolumeCommand cmd, ModCommand::VOL param) const
{
	if(cmd == VOLCMD_PANNING && sndFile.GetType() == MOD_TYPE_XM)
		return param / 4u;
	else
		return param;
}


// Map slider position to an effect value
ModCommand::VOL EffectInfo::MapPosToVolume(VolumeCommand cmd, uint32 pos) const
{
	if(cmd == VOLCMD_PANNING && sndFile.GetType() == MOD_TYPE_XM)
		return static_cast<ModCommand::VOL>(std::min(pos * 4u, 64u));
	else
		return static_cast<ModCommand::VOL>(std::min(pos, 64u));
}


OPENMPT_NAMESPACE_END
