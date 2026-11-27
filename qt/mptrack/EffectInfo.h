/*
 * EffectInfo.h
 * ------------
 * Purpose: Provide information about effect names, parameter interpretation to the tracker interface.
 * Notes  : mfcless twin of upstream's mptrack/EffectInfo.h.
 *          Hand-edited from the twin.py draft.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"

#include "modcommand.h"

OPENMPT_NAMESPACE_BEGIN

class CSoundFile;

class EffectInfo
{
protected:
	const CSoundFile &sndFile;

public:

	EffectInfo(const CSoundFile &sf) : sndFile(sf) {};

	// Effects Description
	
	bool GetEffectName(mpt::ustring &pszDescription, ModCommand::COMMAND command, uint32 param, bool addCommandFormat = false) const; // addCommandFormat: Prepend "Nxx: ..."
	// Get size of list of known effect commands
	uint32 GetNumEffects() const;
	// Get range information, effect name, etc... from a given effect.
	bool GetEffectInfo(uint32 ndx, mpt::ustring *s, bool addCommandFormat = false, ModCommand::PARAM *prangeMin = nullptr, ModCommand::PARAM *prangeMax = nullptr) const;
	// Get effect index in effect list from effect command + param
	int32 GetIndexFromEffect(ModCommand::COMMAND command, ModCommand::PARAM param) const;
	// Get effect command + param from effect index
	EffectCommand GetEffectFromIndex(uint32 ndx, ModCommand::PARAM &refParam) const;
	EffectCommand GetEffectFromIndex(uint32 ndx) const;
	// Get parameter mask from effect (for extended effects)
	uint32 GetEffectMaskFromIndex(uint32 ndx) const;
	// Get precise effect name, also with explanation of effect parameter
	bool GetEffectNameEx(mpt::ustring &pszName, const ModCommand &m, uint32 param, CHANNELINDEX chn) const;
	// Check whether an effect is extended (with parameter nibbles)
	bool IsExtendedEffect(uint32 ndx) const;
	// Map an effect value to slider position
	uint32 MapValueToPos(uint32 ndx, uint32 param) const;
	// Map slider position to an effect value
	uint32 MapPosToValue(uint32 ndx, uint32 pos) const;

	// Volume column effects description

	// Get size of list of known volume commands
	uint32 GetNumVolCmds() const;
	// Get effect index in effect list from volume command
	int32 GetIndexFromVolCmd(ModCommand::VOLCMD volcmd) const;
	// Get volume command from effect index
	VolumeCommand GetVolCmdFromIndex(uint32 ndx) const;
	// Get range information, effect name, etc... from a given effect.
	bool GetVolCmdInfo(uint32 ndx, mpt::ustring *s, ModCommand::VOL *prangeMin = nullptr, ModCommand::VOL *prangeMax = nullptr) const;
	// Get effect name and parameter description
	bool GetVolCmdParamInfo(const ModCommand &m, mpt::ustring *s, bool hex) const;
	// Map an effect value to slider position
	uint32 MapVolumeToPos(VolumeCommand cmd, ModCommand::VOL param) const;
	// Map slider position to an effect value
	ModCommand::VOL MapPosToVolume(VolumeCommand cmd, uint32 pos) const;
};

OPENMPT_NAMESPACE_END
