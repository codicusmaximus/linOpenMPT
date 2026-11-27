/*
* IPCWindow.h
* -----------
* Purpose: Hidden window to receive file open commands from another OpenMPT instance
* Notes  : mfcless twin of upstream's mptrack/IPCWindow.h. The window that
 *          other instances send WM_COPYDATA to is a local socket server
 *          (QLocalServer), found by its name; an "IPC window" is that name.
* Authors: OpenMPT Devs
* The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
*/

#pragma once

#include "openmpt/all/BuildSettings.hpp"

#include <QtCore/QString>

OPENMPT_NAMESPACE_BEGIN

namespace IPCWindow
{

	enum class Function : uint32
	{
		Open                               = 0x01,
		SetWindowForeground                = 0x02,
		GetVersion                         = 0x03,  // returns Version::GewRawVersion()
		GetArchitecture                    = 0x04,  // returns mpt::OS::Windows::Architecture
		HasSameBinaryPath                  = 0x05,
		HasSameSettingsPath                = 0x06,
		PlayCurrent                        = 0x07,
	};

	void Open();

	void Close();

	void UpdateLastUsed();

	// -1 if the other instance did not answer.
	intptr_t SendIPC(const QString &ipcWnd, Function function, mpt::const_byte_span data = mpt::const_byte_span());

	template <typename Tdata> intptr_t SendIPC(const QString &ipcWnd, Function function, mpt::span<const Tdata> data) { return SendIPC(ipcWnd, function, mpt::const_byte_span(reinterpret_cast<const std::byte*>(data.data()), data.size() * sizeof(Tdata))); }

	enum InstanceRequirements
	{
		None             = 0x00u,
		SamePath         = 0x01u,
		SameSettings     = 0x02u,
		SameArchitecture = 0x04u,
		SameVersion      = 0x08u,
	};
	MPT_DECLARE_ENUM(InstanceRequirements)

	QString FindIPCWindow(FlagSet<InstanceRequirements> require = None);

	// Send file open requests to other OpenMPT instance, if there is one
	bool SendToIPC(const std::vector<mpt::PathString> &filenames, bool autoplay);

}

OPENMPT_NAMESPACE_END
