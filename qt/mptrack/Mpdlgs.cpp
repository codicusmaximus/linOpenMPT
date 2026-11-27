/*
 * MPDlgs.cpp
 * ----------
 * Purpose: Implementation of various player setup dialogs.
 * Notes  : mfcless twin of upstream's mptrack/Mpdlgs.cpp.
 *          Draft from qt/tools/twin.py, not yet edited by hand.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "Mpdlgs.h"
#include "DialogBase.h"
#include "dlg_misc.h"
#include "ImageLists.h"
#include "Mainfrm.h"
#include "Moddoc.h"
#include "Mptrack.h"
#include "Reporting.h"
#include "resource.h"
#include "Sndfile.h"
#include "WindowMessages.h"
#include "../common/mptStringBuffer.h"
#include "../soundlib/mod_specifications.h"
#include "mpt/parse/parse.hpp"
#include "openmpt/sounddevice/SoundDevice.hpp"
#include "openmpt/sounddevice/SoundDeviceManager.hpp"


OPENMPT_NAMESPACE_BEGIN


const mpt::uchar *gszChnCfgNames[3] =
{
	U_("Mono"),
	U_("Stereo"),
	U_("Quad")
};


static double ParseTime(mpt::ustring str)
{
	return mpt::parse<double>(mpt::ToCharset(mpt::Charset::ASCII, str)) / 1000.0;
}


static mpt::ustring PrintTime(double seconds)
{
	int32 microseconds = mpt::saturate_round<int32>(seconds * 1000000.0);
	int precision = 0;
	if(microseconds < 1000)
	{
		precision = 3;
	} else if(microseconds < 10000)
	{
		precision = 2;
	} else if(microseconds < 100000)
	{
		precision = 1;
	} else
	{
		precision = 0;
	}
	return MPT_UFORMAT("{} ms")(mpt::ufmt::fix(seconds * 1000.0, precision));
}


void COptionsSoundcard::RegisterMessageMap()
{
	PropertyPage::RegisterMessageMap();
	AddScroll(Qt::Horizontal, &COptionsSoundcard::OnHScroll);
	AddCommand(IDC_CHECK4, &COptionsSoundcard::OnExclusiveModeChanged);
	AddCommand(IDC_CHECK5, &COptionsSoundcard::OnSettingsChanged);
	AddCommand(IDC_CHECK7, &COptionsSoundcard::OnSettingsChanged);
	AddCommand(IDC_CHECK9, &COptionsSoundcard::OnSettingsChanged);
	AddCommand(IDC_CHECK_SOUNDCARD_SHOWALL, &COptionsSoundcard::OnSoundCardShowAll);
	AddNotify(IDC_COMBO1, mfcless::Notify::SelChange, &COptionsSoundcard::OnDeviceChanged);
	AddNotify(IDC_COMBO2, mfcless::Notify::SelChange, &COptionsSoundcard::OnSettingsChanged);
	AddNotify(IDC_COMBO_UPDATEINTERVAL, mfcless::Notify::SelChange, &COptionsSoundcard::OnSettingsChanged);
	AddNotify(IDC_COMBO3, mfcless::Notify::SelChange, &COptionsSoundcard::OnSettingsChanged);
	AddNotify(IDC_COMBO4, mfcless::Notify::SelChange, &COptionsSoundcard::OnSettingsChanged);
	AddNotify(IDC_COMBO5, mfcless::Notify::SelChange, &COptionsSoundcard::OnChannelsChanged);
	AddNotify(IDC_COMBO6, mfcless::Notify::SelChange, &COptionsSoundcard::OnSampleFormatChanged);
	AddNotify(IDC_COMBO10, mfcless::Notify::SelChange, &COptionsSoundcard::OnSettingsChanged);
	AddNotify(IDC_COMBO2, mfcless::Notify::EditChange, &COptionsSoundcard::OnSettingsChanged);
	AddNotify(IDC_COMBO_UPDATEINTERVAL, mfcless::Notify::EditChange, &COptionsSoundcard::OnSettingsChanged);
	AddNotify(IDC_COMBO11, mfcless::Notify::SelChange, &COptionsSoundcard::OnSettingsChanged);
	AddCommand(IDC_BUTTON1, &COptionsSoundcard::OnSoundCardRescan);
	AddCommand(IDC_BUTTON2, &COptionsSoundcard::OnSoundCardDriverPanel);
	AddNotify(IDC_COMBO_CHANNEL_FRONTLEFT, mfcless::Notify::SelChange, &COptionsSoundcard::OnChannel1Changed);
	AddNotify(IDC_COMBO_CHANNEL_FRONTRIGHT, mfcless::Notify::SelChange, &COptionsSoundcard::OnChannel2Changed);
	AddNotify(IDC_COMBO_CHANNEL_REARLEFT, mfcless::Notify::SelChange, &COptionsSoundcard::OnChannel3Changed);
	AddNotify(IDC_COMBO_CHANNEL_REARRIGHT, mfcless::Notify::SelChange, &COptionsSoundcard::OnChannel4Changed);
	AddNotify(IDC_COMBO_RECORDING_CHANNELS, mfcless::Notify::SelChange, &COptionsSoundcard::OnRecordingChanged);
	AddNotify(IDC_COMBO_RECORDING_SOURCE, mfcless::Notify::SelChange, &COptionsSoundcard::OnSettingsChanged);
}


void COptionsSoundcard::OnSampleFormatChanged()
{
	OnSettingsChanged();
	UpdateDither();
}


void COptionsSoundcard::OnRecordingChanged()
{
	uintptr_t inputChannels = mfcless::GetItemData(m_CbnRecordingChannels, m_CbnRecordingChannels->currentIndex());
	m_CbnRecordingSource->setEnabled((m_CurrentDeviceCaps.HasNamedInputSources && inputChannels > 0) ? true : false);
	OnSettingsChanged();
}


void COptionsSoundcard::DoDataExchange(DataExchange &dx)
{
	PropertyPage::DoDataExchange(dx);
	//{{AFX_DATA_MAP(COptionsSoundcard)
	ExchangeControl(dx, IDC_COMBO1, m_CbnDevice);
	ExchangeControl(dx, IDC_COMBO2, m_CbnLatencyMS);
	ExchangeControl(dx, IDC_COMBO_UPDATEINTERVAL, m_CbnUpdateIntervalMS);
	ExchangeControl(dx, IDC_COMBO3, m_CbnMixingFreq);
	ExchangeControl(dx, IDC_COMBO5, m_CbnChannels);
	ExchangeControl(dx, IDC_COMBO6, m_CbnSampleFormat);
	ExchangeControl(dx, IDC_COMBO10, m_CbnDither);
	ExchangeControl(dx, IDC_BUTTON2, m_BtnDriverPanel);
	ExchangeControl(dx, IDC_COMBO6, m_CbnSampleFormat);
	ExchangeControl(dx, IDC_COMBO11, m_CbnStoppedMode);
	ExchangeControl(dx, IDC_COMBO_CHANNEL_FRONTLEFT , m_CbnChannelMapping[0]);
	ExchangeControl(dx, IDC_COMBO_CHANNEL_FRONTRIGHT, m_CbnChannelMapping[1]);
	ExchangeControl(dx, IDC_COMBO_CHANNEL_REARLEFT  , m_CbnChannelMapping[2]);
	ExchangeControl(dx, IDC_COMBO_CHANNEL_REARRIGHT , m_CbnChannelMapping[3]);
	ExchangeControl(dx, IDC_COMBO_RECORDING_CHANNELS, m_CbnRecordingChannels);
	ExchangeControl(dx, IDC_COMBO_RECORDING_SOURCE, m_CbnRecordingSource);
	ExchangeControl(dx, IDC_EDIT_STATISTICS, m_EditStatistics);
	//}}AFX_DATA_MAP
}


COptionsSoundcard::COptionsSoundcard(SoundDevice::Identifier deviceIdentifier)
	: CPropertyPage(IDD_OPTIONS_SOUNDCARD)
	, m_InitialDeviceIdentifier(deviceIdentifier)
{
	return;
}


void COptionsSoundcard::SetInitialDevice()
{
	SetDevice(m_InitialDeviceIdentifier, true);
}


void COptionsSoundcard::SetDevice(SoundDevice::Identifier dev, bool forceReload)
{
	SoundDevice::Identifier olddev = m_CurrentDeviceInfo.GetIdentifier();
	SoundDevice::Info newInfo;
	SoundDevice::Caps newCaps;
	SoundDevice::DynamicCaps newDynamicCaps;
	SoundDevice::Settings newSettings;
	newInfo = theApp.GetSoundDevicesManager()->FindDeviceInfo(dev);
	newCaps = theApp.GetSoundDevicesManager()->GetDeviceCaps(dev, CMainFrame::GetMainFrame()->gpSoundDevice);
	newDynamicCaps = theApp.GetSoundDevicesManager()->GetDeviceDynamicCaps(dev, TrackerSettings::Instance().GetSampleRates(), CMainFrame::GetMainFrame(), CMainFrame::GetMainFrame()->gpSoundDevice, true);
	bool deviceChanged = (dev != olddev);
	if(deviceChanged || forceReload)
	{
		newSettings = TrackerSettings::Instance().GetSoundDeviceSettings(dev);
	} else
	{
		newSettings = m_Settings;
	}
	m_CurrentDeviceInfo = newInfo;
	m_CurrentDeviceCaps = newCaps;
	m_CurrentDeviceDynamicCaps = newDynamicCaps;
	m_Settings = newSettings;
}


void COptionsSoundcard::OnSoundCardShowAll()
{
	TrackerSettings::Instance().m_SoundShowDeprecatedDevices = (IsChecked(IDC_CHECK_SOUNDCARD_SHOWALL) == 1);
	SetDevice(m_CurrentDeviceInfo.GetIdentifier(), true);
	UpdateEverything();
}


void COptionsSoundcard::OnSoundCardRescan()
{
	{
		// Close sound device because IDs might change when re-enumerating which could cause all kinds of havoc.
		CMainFrame::GetMainFrame()->audioCloseDevice();
		delete CMainFrame::GetMainFrame()->gpSoundDevice;
		CMainFrame::GetMainFrame()->gpSoundDevice = nullptr;
	}
	theApp.GetSoundDevicesManager()->ReEnumerate();
	SetDevice(m_CurrentDeviceInfo.GetIdentifier(), true);
	UpdateEverything();
}


bool COptionsSoundcard::OnInitDialog()
{
	PropertyPage::OnInitDialog();
	SetInitialDevice();
	UpdateEverything();
	return true;
}


void COptionsSoundcard::UpdateLatency()
{
	{
		EnableItem(IDC_STATIC_LATENCY, true);
		m_CbnLatencyMS->setEnabled(true);
	}
	// latency
	{
		static constexpr double latencies [] = {
			0.001,
			0.002,
			0.003,
			0.004,
			0.005,
			0.010,
			0.015,
			0.020,
			0.025,
			0.030,
			0.040,
			0.050,
			0.075,
			0.100,
			0.150,
			0.200,
			0.250
		};
		m_CbnLatencyMS->clear();
		m_CbnLatencyMS->SetWindowText(PrintTime(m_Settings.Latency));
		for(auto lat : latencies)
		{
			if(m_CurrentDeviceCaps.LatencyMin <= lat && lat <= m_CurrentDeviceCaps.LatencyMax)
			{
				mfcless::AddItem(m_CbnLatencyMS, PrintTime(lat));
			}
		}
	}
	if(theApp.GetSoundDevicesManager()->IsDeviceUnavailable(m_CurrentDeviceInfo.GetIdentifier()))
	{
		EnableItem(IDC_STATIC_LATENCY, false);
		m_CbnLatencyMS->setEnabled(false);
	}
}


void COptionsSoundcard::UpdateUpdateInterval()
{
	{
		m_CbnUpdateIntervalMS->setEnabled(true);
	}
	// update interval
	{
		static constexpr double updateIntervals [] = {
			0.001,
			0.002,
			0.005,
			0.010,
			0.015,
			0.020,
			0.025,
			0.050
		};
		m_CbnUpdateIntervalMS->clear();
		m_CbnUpdateIntervalMS->SetWindowText(PrintTime(m_Settings.UpdateInterval));
		for(auto upd : updateIntervals)
		{
			if(m_CurrentDeviceCaps.UpdateIntervalMin <= upd && upd <= m_CurrentDeviceCaps.UpdateIntervalMax)
			{
				mfcless::AddItem(m_CbnUpdateIntervalMS, PrintTime(upd));
			}
		}
	}
	if(theApp.GetSoundDevicesManager()->IsDeviceUnavailable(m_CurrentDeviceInfo.GetIdentifier()) || !m_CurrentDeviceCaps.CanUpdateInterval)
	{
		m_CbnUpdateIntervalMS->setEnabled(false);
	}
}


void COptionsSoundcard::UpdateGeneral()
{
	// General
	{
		if(m_CurrentDeviceCaps.CanKeepDeviceRunning)
		{
			m_CbnStoppedMode->clear();
			mfcless::AddItem(m_CbnStoppedMode, U_("Close driver"));
			mfcless::AddItem(m_CbnStoppedMode, U_("Pause driver"));
			mfcless::AddItem(m_CbnStoppedMode, U_("Play silence"));
			m_CbnStoppedMode->setCurrentIndex(TrackerSettings::Instance().m_SoundSettingsStopMode);
		} else
		{
			m_CbnStoppedMode->clear();
			mfcless::AddItem(m_CbnStoppedMode, U_("Close driver"));
			mfcless::AddItem(m_CbnStoppedMode, U_("Close driver"));
			mfcless::AddItem(m_CbnStoppedMode, U_("Close driver"));
			m_CbnStoppedMode->setCurrentIndex(TrackerSettings::Instance().m_SoundSettingsStopMode);
		}
		SetChecked(IDC_CHECK7, TrackerSettings::Instance().m_SoundSettingsOpenDeviceAtStartup ? 1 : 0);
	}
	bool isUnavailble = theApp.GetSoundDevicesManager()->IsDeviceUnavailable(m_CurrentDeviceInfo.GetIdentifier());
	m_CbnStoppedMode->setEnabled(isUnavailble ? false : (m_CurrentDeviceCaps.CanKeepDeviceRunning ? true : false));
	CPropertySheet *sheet = dynamic_cast<CPropertySheet *>(GetParent());
	if(sheet) sheet->EnableItem(IDOK, isUnavailble ? false : true);
}


void COptionsSoundcard::UpdateEverything()
{
	// Sound Device
	{
		if(m_CurrentDeviceInfo.IsDeprecated())
		{
			TrackerSettings::Instance().m_SoundShowDeprecatedDevices = true;
		}
		SetChecked(IDC_CHECK_SOUNDCARD_SHOWALL, TrackerSettings::Instance().m_SoundShowDeprecatedDevices ? 1 : 0);

		m_CbnDevice->clear();
		m_CbnDevice->SetImageList(&CMainFrame::GetMainFrame()->m_MiscIcons);

		uint32 iItem = 0;

		for(const auto &it : *theApp.GetSoundDevicesManager())
		{

			if(!TrackerSettings::Instance().m_SoundShowDeprecatedDevices)
			{
				if(it.IsDeprecated())
				{
					continue;
				}
			}

			{
				COMBOBOXEXITEM cbi;
				MemsetZero(cbi);
				cbi.iItem = iItem;
				cbi.cchTextMax = 0;
				cbi.mask = CBEIF_LPARAM | CBEIF_TEXT;
				cbi.lParam = theApp.GetSoundDevicesManager()->GetGlobalID(it.GetIdentifier());
				mpt::ustring TypeWineNative = U_("Wine-Native");
				if(it.type == SoundDevice::TypeWAVEOUT || it.type == SoundDevice::TypePORTAUDIO_WMME)
				{
					cbi.mask |= CBEIF_IMAGE | CBEIF_SELECTEDIMAGE | CBEIF_OVERLAY;
					cbi.iImage = IMAGE_WAVEOUT;
				} else if(it.type == SoundDevice::TypeDSOUND || it.type == SoundDevice::TypePORTAUDIO_DS || it.type == U_("RtAudio-ds"))
				{
					cbi.mask |= CBEIF_IMAGE | CBEIF_SELECTEDIMAGE | CBEIF_OVERLAY;
					cbi.iImage = IMAGE_DIRECTX;
				} else if(it.type == SoundDevice::TypeASIO || it.type == U_("RtAudio-asio"))
				{
					cbi.mask |= CBEIF_IMAGE | CBEIF_SELECTEDIMAGE | CBEIF_OVERLAY;
					cbi.iImage = IMAGE_ASIO;
				} else if(it.type == SoundDevice::TypePORTAUDIO_WASAPI || it.type == U_("RtAudio-wasapi"))
				{
					cbi.mask |= CBEIF_IMAGE | CBEIF_SELECTEDIMAGE | CBEIF_OVERLAY;
					cbi.iImage = IMAGE_SAMPLEMUTE; // // No real image available for now,
				} else if(it.type == SoundDevice::TypePORTAUDIO_WDMKS)
				{
					cbi.mask |= CBEIF_IMAGE | CBEIF_SELECTEDIMAGE | CBEIF_OVERLAY;
					cbi.iImage = IMAGE_CHIP; // No real image available for now,
				} else if(it.type.find(TypeWineNative + U_("-")) == 0)
				{
					if(theApp.GetWineVersion() && (theApp.GetWineVersion()->HostClass() == mpt::osinfo::osclass::Linux))
					{
						cbi.mask |= CBEIF_IMAGE | CBEIF_SELECTEDIMAGE | CBEIF_OVERLAY;
						cbi.iImage = IMAGE_TUX;
					} else
					{
						cbi.iImage = 0;
					}
				} else
				{
					cbi.iImage = 0;
				}
				cbi.iSelectedImage = cbi.iImage;
				cbi.iOverlay = cbi.iImage;
				mpt::ustring tmp = mpt::ToUnicode(it.GetDisplayName());
				cbi.pszText = const_cast<mpt::uchar *>(tmp.GetString());
				cbi.iIndent = 0;
				int pos = m_CbnDevice->InsertItem(&cbi);
				if(static_cast<SoundDevice::Manager::GlobalID>(cbi.lParam) == theApp.GetSoundDevicesManager()->GetGlobalID(m_CurrentDeviceInfo.GetIdentifier()))
				{
					m_CbnDevice->setCurrentIndex(pos);
				}
				iItem++;
			}
		}
	}

	UpdateDevice();

}


void COptionsSoundcard::UpdateDevice()
{
	EnableItem(IDC_CHECK_SOUNDCARD_SHOWALL, m_CurrentDeviceInfo.IsDeprecated() ? false : true);
	UpdateGeneral();
	UpdateControls();
	UpdateLatency();
	UpdateUpdateInterval();
	UpdateSampleRates();
	UpdateChannels();
	UpdateSampleFormat();
	UpdateDither();
	UpdateChannelMapping();
	UpdateRecording();
}


void COptionsSoundcard::UpdateChannels()
{
	{
		m_CbnChannels->setEnabled(true);
	}
	m_CbnChannels->clear();
	int maxChannels = 0;
	if(m_CurrentDeviceDynamicCaps.channelNames.size() > 0)
	{
		maxChannels = static_cast<int>(std::min(std::size_t(4), m_CurrentDeviceDynamicCaps.channelNames.size()));
	} else
	{
		maxChannels = 4;
	}
	int sel = 0;
	for(int channels = maxChannels; channels >= 1; channels /= 2)
	{
		int ndx = mfcless::AddItem(m_CbnChannels, gszChnCfgNames[(channels+2)/2-1]);
		mfcless::SetItemData(m_CbnChannels, ndx, channels);
		if(channels == m_Settings.Channels)
		{
			sel = ndx;
		}
	}
	m_CbnChannels->setCurrentIndex(sel);
	if(theApp.GetSoundDevicesManager()->IsDeviceUnavailable(m_CurrentDeviceInfo.GetIdentifier()))
	{
		m_CbnChannels->setEnabled(false);
	}
}


void COptionsSoundcard::UpdateRecording()
{
	ShowItem(IDC_STATIC_RECORDING, TrackerSettings::Instance().m_SoundShowRecordingSettings ? true : false);
	m_CbnRecordingChannels->setVisible(TrackerSettings::Instance().m_SoundShowRecordingSettings ? true : false);
	m_CbnRecordingSource->setVisible(TrackerSettings::Instance().m_SoundShowRecordingSettings ? true : false);
	m_CbnRecordingChannels->clear();
	m_CbnRecordingSource->clear();
	if(m_CurrentDeviceCaps.CanInput && ((m_CurrentDeviceCaps.HasNamedInputSources && m_CurrentDeviceDynamicCaps.inputSourceNames.size() > 0) || !m_CurrentDeviceCaps.HasNamedInputSources))
	{
		EnableItem(IDC_STATIC_RECORDING, true);
		m_CbnRecordingChannels->setEnabled(true);
		int sel = 0;
		{
			int ndx = mfcless::AddItem(m_CbnRecordingChannels, U_("off"));
			mfcless::SetItemData(m_CbnRecordingChannels, ndx, 0);
			if(0 == m_Settings.InputChannels)
			{
				sel = ndx;
			}
		}
		for(int channels = 4; channels >= 1; channels /= 2)
		{
			int ndx = mfcless::AddItem(m_CbnRecordingChannels, gszChnCfgNames[(channels+2)/2-1]);
			mfcless::SetItemData(m_CbnRecordingChannels, ndx, channels);
			if(channels == m_Settings.InputChannels)
			{
				sel = ndx;
			}
		}
		m_CbnRecordingChannels->setCurrentIndex(sel);
		if(m_CurrentDeviceCaps.HasNamedInputSources)
		{
			m_CbnRecordingSource->setEnabled((m_Settings.InputChannels > 0) ? true : false);
			sel = -1;
			for(size_t ch = 0; ch < m_CurrentDeviceDynamicCaps.inputSourceNames.size(); ch++)
			{
				const int pos = (int)::SendMessageW(m_CbnRecordingSource->m_hWnd, CB_ADDSTRING, 0, (intptr_t)m_CurrentDeviceDynamicCaps.inputSourceNames[ch].second.c_str());
				mfcless::SetItemData(m_CbnRecordingSource, pos, (uintptr_t)m_CurrentDeviceDynamicCaps.inputSourceNames[ch].first);
				if(m_CurrentDeviceDynamicCaps.inputSourceNames[ch].first == m_Settings.InputSourceID)
				{
					sel = pos;
				}
			}
			if(sel == -1 ) sel = 0;
			m_CbnRecordingSource->setCurrentIndex(sel);
		} else
		{
			m_CbnRecordingSource->setEnabled(false);
		}
	} else
	{
		EnableItem(IDC_STATIC_RECORDING, false);
		m_CbnRecordingChannels->setEnabled(false);
		int ndx = mfcless::AddItem(m_CbnRecordingChannels, U_("off"));
		mfcless::SetItemData(m_CbnRecordingChannels, ndx, 0);
		m_CbnRecordingChannels->setCurrentIndex(ndx);
		m_CbnRecordingSource->setEnabled(false);
	}
}


void COptionsSoundcard::UpdateSampleFormat()
{
	{
		m_CbnSampleFormat->setEnabled(true);
	}
	uint32 n = 0;
	m_CbnSampleFormat->clear();
	std::vector<SampleFormat> sampleformats;
	if(IsChecked(IDC_CHECK4))
	{
		sampleformats = m_CurrentDeviceDynamicCaps.supportedExclusiveModeSampleFormats;
	} else
	{
		sampleformats = m_CurrentDeviceDynamicCaps.supportedSampleFormats;
	}
	m_CbnSampleFormat->setEnabled(m_CurrentDeviceCaps.CanSampleFormat && (sampleformats.size() != 1) ? true : false);
	const std::vector<SampleFormat> allSampleFormats = AllSampleFormats<std::vector<SampleFormat>>();
	for(const auto sampleFormat : allSampleFormats)
	{
		if(!sampleformats.empty() && !mpt::contains(sampleformats, sampleFormat))
		{
			continue;
		}
		mpt::ustring name;
		if(sampleFormat.IsFloat())
		{
			name = MPT_UFORMAT("Float {} bit")(sampleFormat.GetBitsPerSample());
		} else if(sampleFormat.IsUnsigned())
		{
			name = MPT_UFORMAT("{} Bit uint")(sampleFormat.GetBitsPerSample());
		} else
		{
			name = MPT_UFORMAT("{} Bit")(sampleFormat.GetBitsPerSample());
		}
		uint32 ndx = mfcless::AddItem(m_CbnSampleFormat, name);
		mfcless::SetItemData(m_CbnSampleFormat, ndx, mpt::to_underlying<SampleFormat::Enum>(sampleFormat));
		if(sampleFormat == m_Settings.sampleFormat)
		{
			n = ndx;
		}
	}
	m_CbnSampleFormat->setCurrentIndex(n);
	if(theApp.GetSoundDevicesManager()->IsDeviceUnavailable(m_CurrentDeviceInfo.GetIdentifier()))
	{
		m_CbnSampleFormat->setEnabled(false);
	}
}


void COptionsSoundcard::UpdateDither()
{
	{
		m_CbnDither->setEnabled(true);
	}
	m_CbnDither->clear();
	SampleFormat sampleFormat = SampleFormat::FromInt(static_cast<int>(mfcless::GetItemData(m_CbnSampleFormat, m_CbnSampleFormat->currentIndex())));
	if(sampleFormat.IsInt() && sampleFormat.GetBitsPerSample() < 32)
	{
		m_CbnDither->setEnabled(true);
		for(std::size_t i = 0; i < DithersOpenMPT::GetNumDithers(); ++i)
		{
			mfcless::AddItem(m_CbnDither, mpt::ToUnicode(DithersOpenMPT::GetModeName(i) + U_(" dither")));
		}
	} else if(m_CurrentDeviceCaps.HasInternalDither)
	{
		m_CbnDither->setEnabled(true);
		mfcless::AddItem(m_CbnDither, mpt::ToUnicode(DithersOpenMPT::GetModeName(DithersOpenMPT::GetNoDither()) + U_(" dither")));
		mfcless::AddItem(m_CbnDither, mpt::ToUnicode(DithersOpenMPT::GetModeName(DithersOpenMPT::GetDefaultDither()) + U_(" dither")));
	} else
	{
		m_CbnDither->setEnabled(false);
		for(std::size_t i = 0; i < DithersOpenMPT::GetNumDithers(); ++i)
		{
			mfcless::AddItem(m_CbnDither, mpt::ToUnicode(DithersOpenMPT::GetModeName(DithersOpenMPT::GetNoDither()) + U_(" dither")));
		}
	}
	if(m_Settings.DitherType < 0 || m_Settings.DitherType >= m_CbnDither->count())
	{
		m_CbnDither->setCurrentIndex(1);
	} else
	{
		m_CbnDither->setCurrentIndex(m_Settings.DitherType);
	}
	if(theApp.GetSoundDevicesManager()->IsDeviceUnavailable(m_CurrentDeviceInfo.GetIdentifier()))
	{
		m_CbnDither->setEnabled(false);
	}
}


void COptionsSoundcard::UpdateChannelMapping()
{
	{
		EnableItem(IDC_STATIC_CHANNELMAPPING, true);
		EnableItem(IDC_STATIC_CHANNEL_FRONT, true);
		EnableItem(IDC_STATIC_CHANNEL_REAR, true);
		for(int mch = 0; mch < NUM_CHANNELCOMBOBOXES; mch++)
		{
			CComboBox *combo = &m_CbnChannelMapping[mch];
			combo->EnableWindow(true);
		}
	}
	int usedChannels = static_cast<int>(mfcless::GetItemData(m_CbnChannels, m_CbnChannels->currentIndex()));
	if(m_Settings.Channels.GetNumHostChannels() != static_cast<uint32>(usedChannels))
	{
		// If the channel mapping is not valid for the selected number of channels, reset it to default identity mapping.
		m_Settings.Channels = SoundDevice::ChannelMapping(usedChannels);
	}
	EnableItem(IDC_STATIC_CHANNELMAPPING, m_CurrentDeviceCaps.CanChannelMapping ? true : false);
	if(m_CurrentDeviceCaps.CanChannelMapping && usedChannels > 2)
	{
		EnableItem(IDC_STATIC_CHANNEL_FRONT, true);
		EnableItem(IDC_STATIC_CHANNEL_REAR, true);
	} else
	{
		EnableItem(IDC_STATIC_CHANNEL_FRONT, false);
		EnableItem(IDC_STATIC_CHANNEL_REAR, false);
	}
	for(int mch = 0; mch < NUM_CHANNELCOMBOBOXES; mch++)	// Host channels
	{
		CComboBox *combo = &m_CbnChannelMapping[mch];
		combo->EnableWindow((m_CurrentDeviceCaps.CanChannelMapping && mch < usedChannels) ? true : false);
		combo->ResetContent();
		if(m_CurrentDeviceCaps.CanChannelMapping)
		{
			combo->SetItemData(combo->AddString(U_("Unassigned")), (uintptr_t)-1);
			combo->SetCurSel(0);
			if(mch < usedChannels)
			{
				for(size_t dch = 0; dch < m_CurrentDeviceDynamicCaps.channelNames.size(); dch++)	// Device channels
				{
					const int pos = (int)::SendMessageW(combo->m_hWnd, CB_ADDSTRING, 0, (intptr_t)m_CurrentDeviceDynamicCaps.channelNames[dch].c_str());
					combo->SetItemData(pos, (uintptr_t)dch);
					if(static_cast<int32>(dch) == m_Settings.Channels.ToDevice(mch))
					{
						combo->SetCurSel(pos);
					}
				}
			}
		}
	}
	if(theApp.GetSoundDevicesManager()->IsDeviceUnavailable(m_CurrentDeviceInfo.GetIdentifier()))
	{
		EnableItem(IDC_STATIC_CHANNELMAPPING, false);
		EnableItem(IDC_STATIC_CHANNEL_FRONT, false);
		EnableItem(IDC_STATIC_CHANNEL_REAR, false);
		for(int mch = 0; mch < NUM_CHANNELCOMBOBOXES; mch++)
		{
			CComboBox *combo = &m_CbnChannelMapping[mch];
			combo->EnableWindow(false);
		}
	}
}


void COptionsSoundcard::OnDeviceChanged()
{
	int n = m_CbnDevice->currentIndex();
	if(n >= 0)
	{
		SetDevice(theApp.GetSoundDevicesManager()->FindDeviceInfo(static_cast<SoundDevice::Manager::GlobalID>(mfcless::GetItemData(m_CbnDevice, n))).GetIdentifier());
		UpdateDevice();
		OnSettingsChanged();
	}
}


void COptionsSoundcard::OnExclusiveModeChanged()
{
	UpdateSampleRates();
	UpdateSampleFormat();
	UpdateDither();
	OnSettingsChanged();
}


void COptionsSoundcard::OnChannelsChanged()
{
	UpdateChannelMapping();
	OnSettingsChanged();
}


void COptionsSoundcard::OnSoundCardDriverPanel()
{
	theApp.GetSoundDevicesManager()->OpenDriverSettings(
		theApp.GetSoundDevicesManager()->FindDeviceInfo(static_cast<SoundDevice::Manager::GlobalID>(mfcless::GetItemData(m_CbnDevice, m_CbnDevice->currentIndex()))).GetIdentifier(),
		CMainFrame::GetMainFrame(),
		CMainFrame::GetMainFrame()->gpSoundDevice
		);
}


void COptionsSoundcard::OnChannelChanged(int channel)
{
	CComboBox *combo = &m_CbnChannelMapping[channel];
	const LONG_PTR newChn = combo->GetItemData(combo->GetCurSel());
	if(newChn == -1)
	{
		return;
	}
	// Ensure that no channel is used twice
	for(int mch = 0; mch < NUM_CHANNELCOMBOBOXES; mch++)	// Host channels
	{
		if(mch != channel)
		{
			combo = &m_CbnChannelMapping[mch];
			if((int)combo->GetItemData(combo->GetCurSel()) == newChn)
			{
				// find an unused channel
				bool found = false;
				int deviceChannel = 0;
				for(; deviceChannel < static_cast<int>(m_CurrentDeviceDynamicCaps.channelNames.size()); ++deviceChannel)
				{
					bool used = false;
					for(int hostChannel = 0; hostChannel < NUM_CHANNELCOMBOBOXES; ++hostChannel)
					{
						if(static_cast<int>(m_CbnChannelMapping[hostChannel].GetItemData(m_CbnChannelMapping[hostChannel].GetCurSel())) == deviceChannel)
						{
							used = true;
							break;
						}
					}
					if(!used)
					{
						found = true;
						break;
					}
				}
				if(found)
				{
					combo->SetCurSel(deviceChannel+1);
				} else
				{
					combo->SetCurSel(0);
				}
				break;
			}
		}
	}
	OnSettingsChanged();
}


// Fill the dropdown box with a list of valid sample rates, depending on the selected sound device.
void COptionsSoundcard::UpdateSampleRates()
{
	{
		EnableItem(IDC_STATIC_FORMAT, true);
		m_CbnMixingFreq->setEnabled(true);
	}

	m_CbnMixingFreq->clear();

	std::vector<uint32> samplerates;

	if(IsChecked(IDC_CHECK4))
	{
		samplerates = m_CurrentDeviceDynamicCaps.supportedExclusiveSampleRates;
	} else
	{
		samplerates = m_CurrentDeviceDynamicCaps.supportedSampleRates;
	}

	if(samplerates.empty())
	{
		// We have no valid list of supported playback rates! Assume all rates supported by OpenMPT are possible...
		samplerates = TrackerSettings::Instance().GetSampleRates();
	}

	int n = 0;
	for(size_t i = 0; i < samplerates.size(); i++)
	{
		int pos = mfcless::AddItem(m_CbnMixingFreq, MPT_UFORMAT("{} Hz")(samplerates[i]));
		mfcless::SetItemData(m_CbnMixingFreq, pos, samplerates[i]);
		if(m_Settings.Samplerate == samplerates[i])
		{
			n = pos;
		}
	}
	m_CbnMixingFreq->setCurrentIndex(n);
	if(theApp.GetSoundDevicesManager()->IsDeviceUnavailable(m_CurrentDeviceInfo.GetIdentifier()))
	{
		EnableItem(IDC_STATIC_FORMAT, false);
		m_CbnMixingFreq->setEnabled(false);
	}
}


void COptionsSoundcard::UpdateControls()
{
	{
		m_BtnDriverPanel->setEnabled(true);
		EnableItem(IDC_CHECK4, true);
		EnableItem(IDC_CHECK5, true);
		EnableItem(IDC_CHECK9, true);
		EnableItem(IDC_STATIC_UPDATEINTERVAL, true);
		EnableItem(IDC_COMBO_UPDATEINTERVAL, true);
	}
	if(!m_CurrentDeviceCaps.CanKeepDeviceRunning)
	{
		m_Settings.KeepDeviceRunning = false;
	}
	m_BtnDriverPanel->setEnabled(m_CurrentDeviceCaps.CanDriverPanel ? true : false);
	EnableItem(IDC_CHECK4, m_CurrentDeviceCaps.CanExclusiveMode ? true : false);
	EnableItem(IDC_CHECK5, m_CurrentDeviceCaps.CanBoostThreadPriority ? true : false);
	EnableItem(IDC_CHECK9, m_CurrentDeviceCaps.CanUseHardwareTiming ? true : false);
	EnableItem(IDC_STATIC_UPDATEINTERVAL, m_CurrentDeviceCaps.CanUpdateInterval ? true : false);
	EnableItem(IDC_COMBO_UPDATEINTERVAL, m_CurrentDeviceCaps.CanUpdateInterval ? true : false);
	GetDlgItem(IDC_CHECK4)->SetWindowText(mpt::ToUnicode(m_CurrentDeviceCaps.ExclusiveModeDescription));
	SetChecked(IDC_CHECK4, m_CurrentDeviceCaps.CanExclusiveMode && m_Settings.ExclusiveMode ? 1 : 0);
	SetChecked(IDC_CHECK5, m_CurrentDeviceCaps.CanBoostThreadPriority && m_Settings.BoostThreadPriority ? 1 : 0);
	SetChecked(IDC_CHECK9, m_CurrentDeviceCaps.CanUseHardwareTiming && m_Settings.UseHardwareTiming ? 1 : 0);
	if(theApp.GetSoundDevicesManager()->IsDeviceUnavailable(m_CurrentDeviceInfo.GetIdentifier()))
	{
		m_BtnDriverPanel->setEnabled(false);
		EnableItem(IDC_CHECK4, false);
		EnableItem(IDC_CHECK5, false);
		EnableItem(IDC_CHECK9, false);
		EnableItem(IDC_STATIC_UPDATEINTERVAL, false);
		EnableItem(IDC_COMBO_UPDATEINTERVAL, false);
	}
}


bool COptionsSoundcard::OnSetActive()
{
	CMainFrame::m_nLastOptionsPage = OPTIONS_PAGE_SOUNDCARD;
	return PropertyPage::OnSetActive();
}


void COptionsSoundcard::OnOK()
{
	if(!theApp.GetSoundDevicesManager()->IsDeviceUnavailable(m_CurrentDeviceInfo.GetIdentifier()))
	{

	// General
	{
		TrackerSettings::Instance().m_SoundSettingsOpenDeviceAtStartup = IsChecked(IDC_CHECK7) != 0;
	}
	m_Settings.ExclusiveMode = IsChecked(IDC_CHECK4) != 0;
	m_Settings.BoostThreadPriority = IsChecked(IDC_CHECK5) != 0;
	m_Settings.UseHardwareTiming = IsChecked(IDC_CHECK9) != 0;
	// Mixing Freq
	{
		m_Settings.Samplerate = static_cast<uint32>(mfcless::GetItemData(m_CbnMixingFreq, m_CbnMixingFreq->currentIndex()));
	}
	// Channels
	{
		uintptr_t n = mfcless::GetItemData(m_CbnChannels, m_CbnChannels->currentIndex());
		m_Settings.Channels = static_cast<int>(n);
		if((m_Settings.Channels != 1) && (m_Settings.Channels != 4))
		{
			m_Settings.Channels = 2;
		}
	}
	// SampleFormat
	{
		uintptr_t n = mfcless::GetItemData(m_CbnSampleFormat, m_CbnSampleFormat->currentIndex());
		m_Settings.sampleFormat = SampleFormat::FromInt(static_cast<int>(n));
	}
	// Dither
	{
		m_Settings.DitherType = m_CbnDither->currentIndex();
	}
	// Latency
	{
		mpt::ustring s;
		m_CbnLatencyMS->GetWindowText(s);
		m_Settings.Latency = ParseTime(s);
		//Check given value.
		if(m_Settings.Latency == 0.0) m_Settings.Latency = m_CurrentDeviceCaps.DefaultSettings.Latency;
		m_Settings.Latency = Clamp(m_Settings.Latency, m_CurrentDeviceCaps.LatencyMin, m_CurrentDeviceCaps.LatencyMax);
		m_CbnLatencyMS->SetWindowText(PrintTime(m_Settings.Latency));
	}
	// Update Interval
	{
		mpt::ustring s;
		m_CbnUpdateIntervalMS->GetWindowText(s);
		m_Settings.UpdateInterval = ParseTime(s);
		//Check given value.
		if(m_Settings.UpdateInterval == 0.0) m_Settings.UpdateInterval = m_CurrentDeviceCaps.DefaultSettings.UpdateInterval;
		m_Settings.UpdateInterval = Clamp(m_Settings.UpdateInterval, m_CurrentDeviceCaps.UpdateIntervalMin, m_CurrentDeviceCaps.UpdateIntervalMax);
		m_CbnUpdateIntervalMS->SetWindowText(PrintTime(m_Settings.UpdateInterval));
	}
	// Channel Mapping
	{
		if(m_CurrentDeviceCaps.CanChannelMapping)
		{
			int numChannels = std::min(static_cast<int>(m_Settings.Channels), static_cast<int>(NUM_CHANNELCOMBOBOXES));
			std::vector<int32> channels(numChannels);
			for(int mch = 0; mch < numChannels; mch++)	// Host channels
			{
				CComboBox *combo = &m_CbnChannelMapping[mch];
				channels[mch] = static_cast<int32>(combo->GetItemData(combo->GetCurSel()));
			}
			m_Settings.Channels = channels;
		}
	}
	// Recording
	{
		if(TrackerSettings::Instance().m_SoundShowRecordingSettings && m_CurrentDeviceCaps.CanInput && ((m_CurrentDeviceCaps.HasNamedInputSources && m_CurrentDeviceDynamicCaps.inputSourceNames.size() > 0) || !m_CurrentDeviceCaps.HasNamedInputSources))
		{
			uintptr_t n = mfcless::GetItemData(m_CbnRecordingChannels, m_CbnRecordingChannels->currentIndex());
			m_Settings.InputChannels = static_cast<uint8>(n);
			if((m_Settings.InputChannels != 1) && (m_Settings.InputChannels != 2) && (m_Settings.InputChannels != 4))
			{
				m_Settings.InputChannels = 0;
			}
			if(m_CurrentDeviceCaps.HasNamedInputSources)
			{
				uintptr_t sourceID = mfcless::GetItemData(m_CbnRecordingSource, m_CbnRecordingSource->currentIndex());
				m_Settings.InputSourceID = static_cast<uint32>(sourceID);
			} else
			{
				m_Settings.InputSourceID = 0;
			}
		} else
		{
			m_Settings.InputChannels = 0;
			m_Settings.InputSourceID = 0;
		}
	}
	CMainFrame::GetMainFrame()->SetupSoundCard(m_Settings, m_CurrentDeviceInfo.GetIdentifier(), (SoundDeviceStopMode)m_CbnStoppedMode->currentIndex());
	SetDevice(m_CurrentDeviceInfo.GetIdentifier(), true); // Poll changed ASIO sample format and channel names
	UpdateDevice();
	UpdateStatistics();

	} else
	{

		Reporting::Error("Sound card currently not available.");

	}

	PropertyPage::OnOK();
}


void COptionsSoundcard::UpdateStatistics()
{
	if (!m_EditStatistics) return;
	CMainFrame *pMainFrm = CMainFrame::GetMainFrame();
	if(pMainFrm->gpSoundDevice && pMainFrm->IsPlaying())
	{
		const SoundDevice::BufferAttributes bufferAttributes = pMainFrm->gpSoundDevice->GetEffectiveBufferAttributes();
		const SoundDevice::Statistics stats = pMainFrm->gpSoundDevice->GetStatistics();
#if 0
		const SoundDevice::TimeInfo timeInfo = pMainFrm->gpSoundDevice->GetTimeInfo();
		const SoundDevice::StreamPosition streamPosition = pMainFrm->gpSoundDevice->GetStreamPosition();
#endif
		const uint32 samplerate = pMainFrm->gpSoundDevice->GetSettings().Samplerate;
		mpt::ustring s;
		if(bufferAttributes.NumBuffers > 2)
		{
			s += MPT_UFORMAT("Buffer: {}% ({}/{})\r\n")((bufferAttributes.Latency > 0.0) ? mpt::saturate_round<int64>(stats.InstantaneousLatency / bufferAttributes.Latency * 100.0) : 0, (stats.LastUpdateInterval > 0.0) ? mpt::saturate_round<int64>(bufferAttributes.Latency / stats.LastUpdateInterval) : 0, bufferAttributes.NumBuffers);
		} else
		{
			s += MPT_UFORMAT("Buffer: {}%\r\n")((bufferAttributes.Latency > 0.0) ? mpt::saturate_round<int64>(stats.InstantaneousLatency / bufferAttributes.Latency * 100.0) : 0);
		}
		s += MPT_UFORMAT("Latency: {} ms (current: {} ms, {} frames)\r\n")(mpt::ufmt::fix(bufferAttributes.Latency * 1000.0, 1), mpt::ufmt::fix(stats.InstantaneousLatency * 1000.0, 1), mpt::saturate_round<int64>(stats.InstantaneousLatency * samplerate));
		s += MPT_UFORMAT("Period: {} ms (current: {} ms, {} frames)\r\n")(mpt::ufmt::fix(bufferAttributes.UpdateInterval * 1000.0, 1), mpt::ufmt::fix(stats.LastUpdateInterval * 1000.0, 1), mpt::saturate_round<int64>(stats.LastUpdateInterval * samplerate));
#if 0
		s += MPT_UFORMAT("TimeInfo: latency = {} ms / speed = {} / latency = {} ms\r\n")(
			mpt::ufmt::fix(timeInfo.Latency * 1000.0, 1),
			mpt::ufmt::flt(timeInfo.Speed, 4),
			mpt::ufmt::fix((timeInfo.RenderStreamPositionBefore.Seconds - streamPosition.Seconds) * 1000.0, 1));
#endif
		s += stats.text;
		m_EditStatistics->SetWindowText(mpt::ToUnicode(s));
	}	else
	{
		if(theApp.GetSoundDevicesManager()->IsDeviceUnavailable(m_CurrentDeviceInfo.GetIdentifier()))
		{
			m_EditStatistics->SetWindowText(U_("Device currently unavailable."));
		} else
		{
			m_EditStatistics->SetWindowText(U_(""));
		}
	}
}


//////////////////
// COptionsMixer

void COptionsMixer::RegisterMessageMap()
{
	PropertyPage::RegisterMessageMap();
	AddScroll(Qt::Horizontal, &COptionsMixer::OnHScroll);
	AddScroll(Qt::Vertical, &COptionsMixer::OnVScroll);
	AddNotify(IDC_COMBO_FILTER, mfcless::Notify::SelChange, &COptionsMixer::OnSettingsChanged);
	AddNotify(IDC_COMBO_AMIGA_TYPE, mfcless::Notify::SelChange, &COptionsMixer::OnSettingsChanged);
	AddNotify(IDC_RAMPING_IN, mfcless::Notify::EditUpdate, &COptionsMixer::OnRampingChanged);
	AddNotify(IDC_RAMPING_OUT, mfcless::Notify::EditUpdate, &COptionsMixer::OnRampingChanged);
	AddCommand(IDC_CHECK_SOFTPAN, &COptionsMixer::OnSettingsChanged);
	AddCommand(IDC_CHECK1, &COptionsMixer::OnAmigaChanged);
	AddCommand(IDC_BUTTON1, &COptionsMixer::OnDefaultRampSettings);
}


void COptionsMixer::DoDataExchange(DataExchange &dx)
{
	PropertyPage::DoDataExchange(dx);
	//{{AFX_DATA_MAP(COptionsSoundcard)
	ExchangeControl(dx, IDC_COMBO_FILTER, m_CbnResampling);
	ExchangeControl(dx, IDC_COMBO_AMIGA_TYPE, m_CbnAmigaType);
	ExchangeControl(dx, IDC_RAMPING_IN, m_CEditRampUp);
	ExchangeControl(dx, IDC_RAMPING_OUT, m_CEditRampDown);
	ExchangeControl(dx, IDC_EDIT_VOLRAMP_SAMPLES_UP, m_CInfoRampUp);
	ExchangeControl(dx, IDC_EDIT_VOLRAMP_SAMPLES_DOWN, m_CInfoRampDown);
	ExchangeControl(dx, IDC_SLIDER_STEREOSEP, m_SliderStereoSep);
	ExchangeControl(dx, IDC_SLIDER_PREAMP, m_SliderPreAmp);
	//}}AFX_DATA_MAP
}


COptionsMixer::COptionsMixer()
	: CPropertyPage{IDD_OPTIONS_MIXER}
{
	m_CEditRampUp->SetAccessibleSuffix(U_("microseconds up"));
	m_CEditRampDown->SetAccessibleSuffix(U_("microseconds down"));
}



bool COptionsMixer::OnInitDialog()
{
	PropertyPage::OnInitDialog();

	// Resampling type
	{
		const auto resamplingModes = Resampling::AllModes();
		for(auto mode : resamplingModes)
		{
			int index = mfcless::AddItem(m_CbnResampling, CTrackApp::GetResamplingModeName(mode, 2, true));
			mfcless::SetItemData(m_CbnResampling, index, mode);
			if(TrackerSettings::Instance().ResamplerMode == mode)
			{
				m_CbnResampling->setCurrentIndex(index);
			}
		}
	}

	// Amiga Resampler
	const bool enableAmigaResampler = TrackerSettings::Instance().ResamplerEmulateAmiga != Resampling::AmigaFilter::Off;
	SetChecked(IDC_CHECK1, enableAmigaResampler ? 1 : 0);
	m_CbnAmigaType->setEnabled(enableAmigaResampler ? true : false);
	static constexpr std::pair<const mpt::uchar *, Resampling::AmigaFilter> Filters[] =
	{
		{U_("A500 Filter"),  Resampling::AmigaFilter::A500},
		{U_("A1200 Filter"), Resampling::AmigaFilter::A1200},
		{U_("Unfiltered"),   Resampling::AmigaFilter::Unfiltered},
	};
	int sel = 0;
	for(const auto & [name, filter] : Filters)
	{
		const int item = mfcless::AddItem(m_CbnAmigaType, name);
		mfcless::SetItemData(m_CbnAmigaType, item, static_cast<uintptr_t>(filter));
		if(filter == TrackerSettings::Instance().ResamplerEmulateAmiga)
			sel = item;
	}
	m_CbnAmigaType->setCurrentIndex(sel);

	// volume ramping
	{
		m_CEditRampUp->SetWindowText(mpt::ToUnicode(mpt::ufmt::val(TrackerSettings::Instance().GetMixerSettings().GetVolumeRampUpMicroseconds())));
		m_CEditRampDown->SetWindowText(mpt::ToUnicode(mpt::ufmt::val(TrackerSettings::Instance().GetMixerSettings().GetVolumeRampDownMicroseconds())));
		static_cast<CSpinButtonCtrl *>(GetDlgItem(IDC_SPIN2))->SetRange32(0, int32_max);
		static_cast<CSpinButtonCtrl *>(GetDlgItem(IDC_SPIN3))->SetRange32(0, int32_max);
		UpdateRamping();
	}

	// Stereo Separation
	{
		m_SliderStereoSep->setRange(0, 32);
		m_SliderStereoSep->setValue(16);
		for (int n = 0; n <= 32; n++)
		{
			if ((int)TrackerSettings::Instance().MixerStereoSeparation <= 8 * n)
			{
				m_SliderStereoSep->setValue(n);
				break;
			}
		}
		UpdateStereoSep();
	}

	// soft pan
	{
		SetChecked(IDC_CHECK_SOFTPAN, (TrackerSettings::Instance().MixerFlags & SNDMIX_SOFTPANNING) ? 1 : 0);
	}

	// Pre-Amplification
	{
		m_SliderPreAmp->setTickInterval(5);
		m_SliderPreAmp->setRange(0, 40);
		int n = (TrackerSettings::Instance().MixerPreAmp - 64) / 8;
		if ((n < 0) || (n > 40)) n = 16;
		m_SliderPreAmp->setValue(n);
	}

	m_initialized = true;

	return true;
}


bool COptionsMixer::OnSetActive()
{
	CMainFrame::m_nLastOptionsPage = OPTIONS_PAGE_MIXER;
	return PropertyPage::OnSetActive();
}


void COptionsMixer::OnAmigaChanged()
{
	const bool enableAmigaResampler = IsChecked(IDC_CHECK1) != 0;
	m_CbnAmigaType->setEnabled(enableAmigaResampler ? true : false);
	OnSettingsChanged();
}


void COptionsMixer::OnRampingChanged()
{
	if(!m_initialized)
		return;
	UpdateRamping();
	OnSettingsChanged();
}


void COptionsMixer::OnDefaultRampSettings()
{
	m_CEditRampUp->SetWindowText(mpt::ToUnicode(mpt::ufmt::val(MixerSettings().GetVolumeRampUpMicroseconds())));
	m_CEditRampDown->SetWindowText(mpt::ToUnicode(mpt::ufmt::val(MixerSettings().GetVolumeRampDownMicroseconds())));
	OnRampingChanged();
}


void COptionsMixer::OnHScroll(mfcless::ScrollCode code, int pos, QWidget *p)
{
	PropertyPage::OnHScroll(n, pos, p);
	if(p == (CScrollBar *)&m_SliderStereoSep)
	{
		UpdateStereoSep();
		OnSettingsChanged();
	}
}


void COptionsMixer::UpdateRamping()
{
	MixerSettings settings = TrackerSettings::Instance().GetMixerSettings();
	mpt::ustring s;
	m_CEditRampUp->GetWindowText(s);
	settings.SetVolumeRampUpMicroseconds(mpt::parse<int32>(s));
	m_CEditRampDown->GetWindowText(s);
	settings.SetVolumeRampDownMicroseconds(mpt::parse<int32>(s));
	s.Format(U_("%i samples at %i Hz"), (int)settings.GetVolumeRampUpSamples(), (int)settings.gdwMixingFreq);
	m_CInfoRampUp->SetWindowText(s);
	s.Format(U_("%i samples at %i Hz"), (int)settings.GetVolumeRampDownSamples(), (int)settings.gdwMixingFreq);
	m_CInfoRampDown->SetWindowText(s);
}


void COptionsMixer::UpdateStereoSep()
{
	mpt::ustring s;
	s.Format(U_("%d%%"), ((8 * m_SliderStereoSep->value()) * 100) / 128);
	SetItemText(IDC_TEXT_STEREOSEP, s);
}


void COptionsMixer::OnOK()
{
	// resampler mode
	{
		TrackerSettings::Instance().ResamplerMode = static_cast<ResamplingMode>(mfcless::GetItemData(m_CbnResampling, m_CbnResampling->currentIndex()));
	}

	// Amiga Resampler
	if(IsChecked(IDC_CHECK1) == 0)
		TrackerSettings::Instance().ResamplerEmulateAmiga = Resampling::AmigaFilter::Off;
	else
		TrackerSettings::Instance().ResamplerEmulateAmiga = static_cast<Resampling::AmigaFilter>(mfcless::GetItemData(m_CbnAmigaType, m_CbnAmigaType->currentIndex()));

	// volume ramping
	{
		MixerSettings settings = TrackerSettings::Instance().GetMixerSettings();
		mpt::ustring s;
		m_CEditRampUp->GetWindowText(s);
		settings.SetVolumeRampUpMicroseconds(mpt::parse<int>(s));
		m_CEditRampDown->GetWindowText(s);
		settings.SetVolumeRampDownMicroseconds(mpt::parse<int>(s));
		TrackerSettings::Instance().SetMixerSettings(settings);
	}

	// stereo sep
	{
		TrackerSettings::Instance().MixerStereoSeparation = 8 * m_SliderStereoSep->value();
	}

	// soft pan
	{
		if(IsChecked(IDC_CHECK_SOFTPAN))
		{
			TrackerSettings::Instance().MixerFlags = TrackerSettings::Instance().MixerFlags | SNDMIX_SOFTPANNING;
		} else
		{
			TrackerSettings::Instance().MixerFlags = TrackerSettings::Instance().MixerFlags & ~SNDMIX_SOFTPANNING;
		}
	}

	// pre amp
	{
		int n = m_SliderPreAmp->value();
		if ((n >= 0) && (n <= 40)) // approximately +/- 10dB
		{
			TrackerSettings::Instance().MixerPreAmp = 64 + (n * 8);
		}
	}

	CMainFrame::GetMainFrame()->SetupPlayer();
	CMainFrame::GetMainFrame()->PostMessage(WM_MOD_INVALIDATEPATTERNS, HINT_MPTOPTIONS);

	PropertyPage::OnOK();
}


////////////////////////////////////////////////////////////////////////////////
//
// CEQSavePresetDlg
//

#ifndef NO_EQ

class CEQSavePresetDlg : public DialogBase
{
protected:
	EQPreset &m_EQ;

public:
	CEQSavePresetDlg(EQPreset &eq, QWidget *parent = nullptr) : DialogBase(IDD_SAVEPRESET, parent), m_EQ(eq) { }
	bool OnInitDialog();
	void OnOK();
};


bool CEQSavePresetDlg::OnInitDialog()
{
	CComboBox *pCombo = (CComboBox *)GetDlgItem(IDC_COMBO1);
	if (pCombo)
	{
		int ndx = 0;
		for (uint32 i=0; i<4; i++)
		{
			int n = pCombo->AddString(mpt::ToUnicode(mpt::Charset::Locale, TrackerSettings::Instance().m_EqUserPresets[i].szName));
			pCombo->SetItemData( n, i);
			if (!lstrcmpiA(TrackerSettings::Instance().m_EqUserPresets[i].szName, m_EQ.szName)) ndx = n;
		}
		pCombo->SetCurSel(ndx);
	}
	SetItemText(IDC_EDIT1, mpt::ToUnicode(mpt::Charset::Locale, m_EQ.szName));
	return true;
}


void CEQSavePresetDlg::OnOK()
{
	CComboBox *pCombo = (CComboBox *)GetDlgItem(IDC_COMBO1);
	if (pCombo)
	{
		int n = pCombo->GetCurSel();
		if ((n < 0) || (n >= 4)) n = 0;
		mpt::ustring s;
		s = GetItemText(IDC_EDIT1);
		mpt::String::WriteAutoBuf(m_EQ.szName) = mpt::ToCharset(mpt::Charset::Locale, s);
		TrackerSettings::Instance().m_EqUserPresets[n] = m_EQ;
	}
	DialogBase::OnOK();
}


void CEQSlider::Init(uint32 nID, uint32 n, QWidget *parent)
{
	m_nSliderNo = n;
	m_pParent = parent;
	SubclassDlgItem(nID, parent);
}


bool CEQSlider::PreTranslateMessage(mfcless::Message *pMsg)
{
	if ((pMsg) && (pMsg->message == mfcless::msg::RButtonUp) && (m_pParent))
	{
		m_x = LOWORD(pMsg->lParam);
		m_y = HIWORD(pMsg->lParam);
		m_pParent->PostMessage(WM_COMMAND, ID_EQSLIDER_BASE+m_nSliderNo, 0);
	}
	return WindowBase::PreTranslateMessage(pMsg);
}

#endif // !NO_EQ


//////////////////////////////////////////////////////////
// COptionsPlayer - DSP / EQ settings


#ifndef NO_EQ
#define EQ_MAX_FREQS	5

const uint32 gEqBandFreqs[MAX_EQ_BANDS][EQ_MAX_FREQS] =
{
	{ 100, 125, 150, 200, 250 },
	{ 300, 350, 400, 450, 500 },
	{ 600, 700, 800, 900, 1000 },
	{ 1250, 1500, 1750, 2000, 2500 },
	{ 3000, 3500, 4000, 4500, 5000 },
	{ 6000, 7000, 8000, 9000, 10000 },
};
#endif // !NO_EQ

void COptionsPlayer::RegisterMessageMap()
{
	PropertyPage::RegisterMessageMap();
	// #ifndef NO_EQ
	// EQ
	AddScroll(Qt::Vertical, &COptionsPlayer::OnVScroll);
	AddCommand(IDC_CHECK3, &COptionsPlayer::OnSettingsChanged);
	AddCommand(IDC_BUTTON1, &COptionsPlayer::OnEqUser1);
	AddCommand(IDC_BUTTON2, &COptionsPlayer::OnEqUser2);
	AddCommand(IDC_BUTTON3, &COptionsPlayer::OnEqUser3);
	AddCommand(IDC_BUTTON4, &COptionsPlayer::OnEqUser4);
	AddCommand(IDC_BUTTON5, &COptionsPlayer::OnSavePreset);
	AddCommandRange(ID_EQSLIDER_BASE, ID_EQSLIDER_BASE + MAX_EQ_BANDS, &COptionsPlayer::OnSliderMenu);
	AddCommandRange(ID_EQMENU_BASE, ID_EQMENU_BASE + EQ_MAX_FREQS, &COptionsPlayer::OnSliderFreq);
	// #endif // !NO_EQ
	// DSP
	AddScroll(Qt::Horizontal, &COptionsPlayer::OnHScroll);
	AddNotify(IDC_COMBO2, mfcless::Notify::SelChange, &COptionsPlayer::OnSettingsChanged);
	AddCommand(IDC_CHECK1, &COptionsPlayer::OnSettingsChanged);
	AddCommand(IDC_CHECK2, &COptionsPlayer::OnSettingsChanged);
	AddCommand(IDC_CHECK4, &COptionsPlayer::OnSettingsChanged);
	AddCommand(IDC_CHECK5, &COptionsPlayer::OnSettingsChanged);
	AddCommand(IDC_CHECK6, &COptionsPlayer::OnSettingsChanged);
	AddCommand(IDC_CHECK7, &COptionsPlayer::OnSettingsChanged);
}


void COptionsPlayer::DoDataExchange(DataExchange &dx)
{
	PropertyPage::DoDataExchange(dx);
	//{{AFX_DATA_MAP(COptionsPlayer)
	ExchangeControl(dx, IDC_COMBO2, m_CbnReverbPreset);
	ExchangeControl(dx, IDC_SLIDER1, m_SbXBassDepth);
	ExchangeControl(dx, IDC_SLIDER2, m_SbXBassRange);
	ExchangeControl(dx, IDC_SLIDER3, m_SbReverbDepth);
	ExchangeControl(dx, IDC_SLIDER5, m_SbSurroundDepth);
	ExchangeControl(dx, IDC_SLIDER6, m_SbSurroundDelay);
	ExchangeControl(dx, IDC_SLIDER4, m_SbBitCrushBits);
	//}}AFX_DATA_MAP
}


COptionsPlayer::COptionsPlayer() : CPropertyPage{IDD_OPTIONS_PLAYER}
#ifndef NO_EQ
	, m_EQPreset(TrackerSettings::Instance().m_EqSettings)
#endif
{
}


bool COptionsPlayer::OnInitDialog()
{
	PropertyPage::OnInitDialog();

	uint32 dwQuality = TrackerSettings::Instance().MixerDSPMask;

#ifndef NO_EQ
	for (uint32 i = 0; i < MAX_EQ_BANDS; i++)
	{
		m_Sliders[i].Init(IDC_SLIDER7 + i, i, this);
		m_Sliders[i].SetRange(0, 32);
		m_Sliders[i].SetTicFreq(4);
	}

	UpdateDialog();

	if (dwQuality & SNDDSP_EQ) SetChecked(IDC_CHECK3, 1);
#else
	EnableItem(IDC_CHECK3, false);
#endif

	// Effects
#ifndef NO_DSP
	if (dwQuality & SNDDSP_MEGABASS) SetChecked(IDC_CHECK1, 1);
#else
	EnableItem(IDC_CHECK1, false);
#endif
#ifndef NO_AGC
	if (dwQuality & SNDDSP_AGC) SetChecked(IDC_CHECK2, 1);
#else
	EnableItem(IDC_CHECK2, false);
#endif
#ifndef NO_DSP
	if (dwQuality & SNDDSP_SURROUND) SetChecked(IDC_CHECK4, 1);
#else
	EnableItem(IDC_CHECK4, false);
#endif
#ifndef NO_DSP
	if (dwQuality & SNDDSP_BITCRUSH) SetChecked(IDC_CHECK5, 1);
#else
	EnableItem(IDC_CHECK5, false);
#endif

#ifndef NO_DSP
	m_SbBitCrushBits->setRange(1, 24);
	m_SbBitCrushBits->setValue(TrackerSettings::Instance().m_BitCrushSettings.m_Bits);
#else
	m_SbBitCurshBits.EnableWindow(false);
#endif

#ifndef NO_DSP
	// Bass Expansion
	m_SbXBassDepth->setRange(0,4);
	m_SbXBassDepth->setValue(8-TrackerSettings::Instance().m_MegaBassSettings.m_nXBassDepth);
	m_SbXBassRange->setRange(0,4);
	m_SbXBassRange->setValue(4 - (TrackerSettings::Instance().m_MegaBassSettings.m_nXBassRange - 1) / 5);
#else
	m_SbXBassDepth->setEnabled(false);
	m_SbXBassRange->setEnabled(false);
#endif

#ifndef NO_REVERB
	// Reverb
	m_SbReverbDepth->setRange(1, 16);
	m_SbReverbDepth->setValue(TrackerSettings::Instance().m_ReverbSettings.m_nReverbDepth);
	uint32 nSel = 0;
	for (uint32 iRvb=0; iRvb<NUM_REVERBTYPES; iRvb++)
	{
		mpt::ustring pszName = mpt::ToUnicode(GetReverbPresetName(iRvb));
		if(!pszName.IsEmpty())
		{
			uint32 n = mfcless::AddItem(m_CbnReverbPreset, pszName);
			mfcless::SetItemData(m_CbnReverbPreset, n, iRvb);
			if (iRvb == TrackerSettings::Instance().m_ReverbSettings.m_nReverbType) nSel = n;
		}
	}
	m_CbnReverbPreset->setCurrentIndex(nSel);
	if (dwQuality & SNDDSP_REVERB) SetChecked(IDC_CHECK6, 1);
#else
	EnableItem(IDC_CHECK6, false);
	m_SbReverbDepth->setEnabled(false);
	m_CbnReverbPreset->setEnabled(false);
#endif

#ifndef NO_DSP
	// Surround
	{
		uint32 n = TrackerSettings::Instance().m_SurroundSettings.m_nProLogicDepth;
		if (n < 1) n = 1;
		if (n > 16) n = 16;
		m_SbSurroundDepth->setRange(1, 16);
		m_SbSurroundDepth->setValue(n);
		m_SbSurroundDelay->setRange(0, 8);
		m_SbSurroundDelay->setValue((TrackerSettings::Instance().m_SurroundSettings.m_nProLogicDelay-5)/5);
	}
#else
	m_SbSurroundDepth->setEnabled(false);
	m_SbSurroundDelay->setEnabled(false);
#endif

	return true;
}


bool COptionsPlayer::OnSetActive()
{
	CMainFrame::m_nLastOptionsPage = OPTIONS_PAGE_PLAYER;

	SetItemText(IDC_EQ_WARNING,
		U_("Note: This EQ is applied to any and all of the modules ")
		U_("that you load in OpenMPT; its settings are stored globally, ")
		U_("rather than in each file. This means that you should avoid ")
		U_("using it as part of your production process, and instead only ")
		U_("use it to correct deficiencies in your audio hardware."));

	return PropertyPage::OnSetActive();
}


void COptionsPlayer::OnHScroll(mfcless::ScrollCode code, int pos, QWidget *psb)
{
	if (nSBCode == SB_ENDSCROLL) return;
	if ((psb) && (psb->m_hWnd == m_SbReverbDepth->m_hWnd))
	{
#ifndef NO_REVERB
		uint32 n = m_SbReverbDepth->value();
		if ((n) && (n <= 16)) TrackerSettings::Instance().m_ReverbSettings.m_nReverbDepth = n;
		//if ((n) && (n <= 16)) CSoundFile::m_Reverb.m_Settings.m_nReverbDepth = n;
		CMainFrame::GetMainFrame()->SetupPlayer();
#endif
	} else
	{
		OnSettingsChanged();
	}
}


void COptionsPlayer::OnOK()
{
	uint32 dwQuality = 0;

#ifndef NO_DSP
	if (IsChecked(IDC_CHECK1)) dwQuality |= SNDDSP_MEGABASS;
#endif
#ifndef NO_AGC
	if (IsChecked(IDC_CHECK2)) dwQuality |= SNDDSP_AGC;
#endif
#ifndef NO_EQ
	if (IsChecked(IDC_CHECK3)) dwQuality |= SNDDSP_EQ;
#endif
#ifndef NO_DSP
	if (IsChecked(IDC_CHECK4)) dwQuality |= SNDDSP_SURROUND;
#endif
#ifndef NO_REVERB
	if (IsChecked(IDC_CHECK6)) dwQuality |= SNDDSP_REVERB;
#endif
#ifndef NO_DSP
	if (IsChecked(IDC_CHECK5)) dwQuality |= SNDDSP_BITCRUSH;
#endif

#ifndef NO_DSP
	{
		TrackerSettings::Instance().m_BitCrushSettings.m_Bits = m_SbBitCrushBits->value();
	}
#endif

#ifndef NO_DSP
	// Bass Expansion
	{
		uint32 nXBassDepth = 8-m_SbXBassDepth->value();
		if (nXBassDepth < 4) nXBassDepth = 4;
		if (nXBassDepth > 8) nXBassDepth = 8;
		uint32 nXBassRange = (4-m_SbXBassRange->value()) * 5 + 1;
		if (nXBassRange < 5) nXBassRange = 5;
		if (nXBassRange > 21) nXBassRange = 21;
		TrackerSettings::Instance().m_MegaBassSettings.m_nXBassDepth = nXBassDepth;
		TrackerSettings::Instance().m_MegaBassSettings.m_nXBassRange = nXBassRange;
	}
#endif
#ifndef NO_REVERB
	// Reverb
	{
		// Reverb depth is dynamically changed
		uint32 nReverbType = static_cast<uint32>(mfcless::GetItemData(m_CbnReverbPreset, m_CbnReverbPreset->currentIndex()));
		if (nReverbType < NUM_REVERBTYPES) TrackerSettings::Instance().m_ReverbSettings.m_nReverbType = nReverbType;
	}
#endif
#ifndef NO_DSP
	// Surround
	{
		uint32 nProLogicDepth = m_SbSurroundDepth->value();
		uint32 nProLogicDelay = 5 + (m_SbSurroundDelay->value() * 5);
		TrackerSettings::Instance().m_SurroundSettings.m_nProLogicDepth = nProLogicDepth;
		TrackerSettings::Instance().m_SurroundSettings.m_nProLogicDelay = nProLogicDelay;
	}
#endif

	TrackerSettings::Instance().MixerDSPMask = dwQuality;

	CMainFrame::GetMainFrame()->SetupPlayer();
	PropertyPage::OnOK();
}


#ifndef NO_EQ

void COptionsPlayer::UpdateEQ(bool bReset)
{
	CriticalSection cs;
	if(CMainFrame::GetMainFrame()->GetSoundFilePlaying())
		CMainFrame::GetMainFrame()->GetSoundFilePlaying()->SetEQGains(m_EQPreset.Gains, m_EQPreset.Freqs, bReset);
}


void COptionsPlayer::OnVScroll(mfcless::ScrollCode code, int pos, QWidget *pScrollBar)
{
	PropertyPage::OnVScroll(nSBCode, nPos, pScrollBar);
	for (uint32 i=0; i<MAX_EQ_BANDS; i++)
	{
		int n = 32 - m_Sliders[i].GetPos();
		if ((n >= 0) && (n <= 32)) m_EQPreset.Gains[i] = n;
	}
	UpdateEQ(false);
}


void COptionsPlayer::LoadEQPreset(const EQPreset &preset)
{
	m_EQPreset = preset;
	UpdateEQ(true);
	UpdateDialog();
}


void COptionsPlayer::OnSavePreset()
{
	CEQSavePresetDlg dlg(m_EQPreset, this);
	if (dlg.DoModal() == IDOK)
	{
		UpdateDialog();
	}
}


static mpt::ustring f2s(uint32 f)
{
	if (f < 1000)
	{
		return MPT_UFORMAT("{}Hz")(f);
	} else
	{
		uint32 fHi = f / 1000u;
		uint32 fLo = f % 1000u;
		if(fLo)
		{
			return MPT_UFORMAT("{}.{}kHz")(fHi, mpt::ufmt::dec0<1>(fLo/100));
		} else
		{
			return MPT_UFORMAT("{}kHz")(fHi);
		}
	}
}


void COptionsPlayer::UpdateDialog()
{
	for (uint32 i=0; i<MAX_EQ_BANDS; i++)
	{
		int n = 32 - m_EQPreset.Gains[i];
		if (n < 0) n = 0;
		if (n > 32) n = 32;
		if (n != (m_Sliders[i].GetPos() & 0xFFFF)) m_Sliders[i].SetPos(n);
		SetItemText(IDC_TEXT1 + i, f2s(m_EQPreset.Freqs[i]));
	}
	for(unsigned int i = 0; i < std::size(TrackerSettings::Instance().m_EqUserPresets); i++)
	{
		SetItemText(IDC_BUTTON1 + i, mpt::ToUnicode(mpt::Charset::Locale, TrackerSettings::Instance().m_EqUserPresets[i].szName));
	}
}


void COptionsPlayer::OnSliderMenu(uint32 nID)
{
	uint32 n = nID - ID_EQSLIDER_BASE;
	if (n < MAX_EQ_BANDS)
	{
		mfcless::Menu hMenu = mfcless::Menu::CreatePopup();
		m_nSliderMenu = n;
		if (!hMenu) return;
		const uint32 *pFreqs = gEqBandFreqs[m_nSliderMenu];
		for (uint32 i = 0; i < EQ_MAX_FREQS; i++)
		{
			uint32 d = mfcless::menu::String;
			if (m_EQPreset.Freqs[m_nSliderMenu] == pFreqs[i]) d |= mfcless::menu::Checked;
			hMenu.AppendMenu(d, ID_EQMENU_BASE+i, f2s(pFreqs[i]));
		}
		mfcless::Point pt(m_Sliders[m_nSliderMenu].m_x, m_Sliders[m_nSliderMenu].m_y);
		m_Sliders[m_nSliderMenu].ClientToScreen(&pt);
		hMenu.TrackPopupMenu(mfcless::menu::LeftAlign|mfcless::menu::RightButton, pt.x, pt.y, this);
		hMenu.DestroyMenu();
	}
}


void COptionsPlayer::OnSliderFreq(uint32 nID)
{
	uint32 n = nID - ID_EQMENU_BASE;
	if ((m_nSliderMenu < MAX_EQ_BANDS) && (n < EQ_MAX_FREQS))
	{
		uint32 f = gEqBandFreqs[m_nSliderMenu][n];
		if (f != m_EQPreset.Freqs[m_nSliderMenu])
		{
			m_EQPreset.Freqs[m_nSliderMenu] = f;
			UpdateEQ(true);
			UpdateDialog();
		}
	}
}

#endif // !NO_EQ


/////////////////////////////////////////////////////////////
// CMidiSetupDlg

void CMidiSetupDlg::RegisterMessageMap()
{
	PropertyPage::RegisterMessageMap();
	AddNotify(IDC_COMBO1, mfcless::Notify::SelChange, &CMidiSetupDlg::OnSettingsChanged);
	AddNotify(IDC_COMBO2, mfcless::Notify::SelChange, &CMidiSetupDlg::OnSettingsChanged);
	AddNotify(IDC_COMBO3, mfcless::Notify::SelChange, &CMidiSetupDlg::OnSettingsChanged);
	AddNotify(IDC_COMBO4, mfcless::Notify::SelChange, &CMidiSetupDlg::OnSettingsChanged);
	AddNotify(IDC_COMBO5, mfcless::Notify::SelChange, &CMidiSetupDlg::OnSettingsChanged);
	AddCommand(IDC_BUTTON1, &CMidiSetupDlg::OnRenameDevice);
	AddCommand(IDC_CHECK1, &CMidiSetupDlg::OnSettingsChanged);
	AddCommand(IDC_CHECK2, &CMidiSetupDlg::OnSettingsChanged);
	AddCommand(IDC_CHECK3, &CMidiSetupDlg::OnSettingsChanged);
	AddCommand(IDC_CHECK4, &CMidiSetupDlg::OnSettingsChanged);
	AddCommand(IDC_CHECK5, &CMidiSetupDlg::OnSettingsChanged);
	AddCommand(IDC_MIDI_TO_PLUGIN, &CMidiSetupDlg::OnSettingsChanged);
	AddCommand(IDC_MIDI_MACRO_CONTROL, &CMidiSetupDlg::OnSettingsChanged);
	AddCommand(IDC_MIDIVOL_TO_NOTEVOL, &CMidiSetupDlg::OnSettingsChanged);
	AddCommand(IDC_MIDIPLAYCONTROL, &CMidiSetupDlg::OnSettingsChanged);
	AddCommand(IDC_MIDIPLAYPATTERNONMIDIIN, &CMidiSetupDlg::OnSettingsChanged);
	AddNotify(IDC_EDIT1, mfcless::Notify::EditChange, &CMidiSetupDlg::OnSettingsChanged);
	AddNotify(IDC_EDIT2, mfcless::Notify::EditChange, &CMidiSetupDlg::OnSettingsChanged);
	AddNotify(IDC_EDIT3, mfcless::Notify::EditChange, &CMidiSetupDlg::OnSettingsChanged);
	AddNotify(IDC_EDIT4, mfcless::Notify::EditChange, &CMidiSetupDlg::OnSettingsChanged);
}


void CMidiSetupDlg::DoDataExchange(DataExchange &dx)
{
	PropertyPage::DoDataExchange(dx);
	//{{AFX_DATA_MAP(COptionsSoundcard)
	ExchangeControl(dx, IDC_SPIN1, m_SpinSpd);
	ExchangeControl(dx, IDC_SPIN2, m_SpinPat);
	ExchangeControl(dx, IDC_SPIN3, m_SpinAmp);
	ExchangeControl(dx, IDC_COMBO1, m_InputDevice);
	ExchangeControl(dx, IDC_COMBO2, m_ATBehaviour);
	ExchangeControl(dx, IDC_COMBO3, m_Quantize);
	ExchangeControl(dx, IDC_COMBO4, m_ContinueMode);
	ExchangeControl(dx, IDC_COMBO5, m_RecordPitchBend);
	ExchangeControl(dx, IDC_EDIT3, m_editAmp);
	//}}AFX_DATA_MAP
}


CMidiSetupDlg::CMidiSetupDlg(FlagSet<MidiSetup> flags, uint32 device)
	: CPropertyPage{IDD_OPTIONS_MIDI}
	, m_midiSetup{flags}
	, m_nMidiDevice{device}
{
	m_editAmp->SetAccessibleSuffix(U_("%"));
}


bool CMidiSetupDlg::OnInitDialog()
{
	PropertyPage::OnInitDialog();
	// Flags
	if(m_midiSetup[MidiSetup::RecordVelocity]) SetChecked(IDC_CHECK1, 1);
	if(m_midiSetup[MidiSetup::RecordNoteOff]) SetChecked(IDC_CHECK2, 1);
	if(m_midiSetup[MidiSetup::EnableMidiInOnStartup]) SetChecked(IDC_CHECK3, 1);
	if(m_midiSetup[MidiSetup::TransposeKeyboard]) SetChecked(IDC_CHECK4, 1);
	if(m_midiSetup[MidiSetup::SendMidiToPlugins]) SetChecked(IDC_MIDI_TO_PLUGIN, 1);
	if(m_midiSetup[MidiSetup::RecordCCsAsMacros]) SetChecked(IDC_MIDI_MACRO_CONTROL, 1);
	if(m_midiSetup[MidiSetup::ApplyChannelVolumeToVelocity]) SetChecked(IDC_MIDIVOL_TO_NOTEVOL, 1);
	if(m_midiSetup[MidiSetup::RespondToPlayControl]) SetChecked(IDC_MIDIPLAYCONTROL, 1);
	if(m_midiSetup[MidiSetup::PlayPatternOnMidiNote]) SetChecked(IDC_MIDIPLAYPATTERNONMIDIIN, 1);

	// Midi In Device
	RefreshDeviceList(m_nMidiDevice);

	// Continue behaviour
	mfcless::AddItem(m_ContinueMode, U_("From Cursor Position"));
	mfcless::AddItem(m_ContinueMode, U_("From Start of Pattern"));
	m_ContinueMode->setCurrentIndex(m_midiSetup[MidiSetup::PlayPatternFromStart] ? 1 : 0);

	// Aftertouch behaviour
	m_ATBehaviour->clear();
	static constexpr std::pair<const mpt::uchar *, RecordAftertouch> aftertouchOptions[] =
	{
		{ U_("Do not record Aftertouch"), RecordAftertouch::DoNotRecord },
		{ U_("Record as Volume Commands"), RecordAftertouch::RecordAsVolume },
		{ U_("Record as MIDI Macros"), RecordAftertouch::RecordAsMacro },
	};

	for(const auto & [str, value] : aftertouchOptions)
	{
		int item = mfcless::AddItem(m_ATBehaviour, str);
		mfcless::SetItemData(m_ATBehaviour, item, static_cast<uintptr_t>(value));
		if(value == TrackerSettings::Instance().aftertouchBehaviour)
		{
			m_ATBehaviour->setCurrentIndex(item);
		}
	}

	// Pitch Bend behaviour
	m_RecordPitchBend->clear();
	static constexpr std::pair<const mpt::uchar *, RecordPitchBend> pitchBendOptions[] =
	{
		{ U_("Do not record Pitch Bends"), RecordPitchBend::DoNotRecord },
		{ U_("Record only as MIDI Macros"), RecordPitchBend::RecordAsMacro },
		{ U_("Record as Finetune or MIDI Macros"), RecordPitchBend::RecordAsFinetuneOrMacro },
		{ U_("Record only as Finetune"), RecordPitchBend::RecordAsFinetune },
	};

	for (const auto & [str, value] : pitchBendOptions)
	{
		int item = mfcless::AddItem(m_RecordPitchBend, str);
		mfcless::SetItemData(m_RecordPitchBend, item, static_cast<uintptr_t>(value));
		if (value == TrackerSettings::Instance().pitchBendBehaviour)
		{
			m_RecordPitchBend->setCurrentIndex(item);
		}
	}

	// Note Velocity amp
	SetDlgItemInt(IDC_EDIT3, TrackerSettings::Instance().midiVelocityAmp);
	m_SpinAmp->SetRange(1, 10000);

	SetItemText(IDC_EDIT4, mpt::ToUnicode(IgnoredCCsToString(TrackerSettings::Instance().midiIgnoreCCs)));

	// Midi Import settings
	SetDlgItemInt(IDC_EDIT1, TrackerSettings::Instance().midiImportTicks);
	SetDlgItemInt(IDC_EDIT2, TrackerSettings::Instance().midiImportPatternLen);

	// Note quantization
	m_Quantize->clear();
	static constexpr std::pair<const mpt::uchar *, uint32> quantizeOptions[] =
	{
		{ U_("1/4th Notes"),  4 },  { U_("1/6th Notes"),  6 },
		{ U_("1/8th Notes"),  8 },  { U_("1/12th Notes"), 12 },
		{ U_("1/16th Notes"), 16 }, { U_("1/24th Notes"), 24 },
		{ U_("1/32nd Notes"), 32 }, { U_("1/48th Notes"), 48 },
		{ U_("1/64th Notes"), 64 }, { U_("1/96th Notes"), 96 },
	};

	for(const auto & [str, value]: quantizeOptions)
	{
		int item = mfcless::AddItem(m_Quantize, str);
		mfcless::SetItemData(m_Quantize, item, value);
		if(value == TrackerSettings::Instance().midiImportQuantize)
		{
			m_Quantize->setCurrentIndex(item);
		}
	}
	m_SpinSpd->SetRange(2, 16);
	m_SpinPat->SetRange(ModSpecs::mptm.patternRowsMin, ModSpecs::mptm.patternRowsMax);
	return true;
}


void CMidiSetupDlg::RefreshDeviceList(uint32 currentDevice)
{
	m_InputDevice->setUpdatesEnabled(false);
	m_InputDevice->clear();
	uint32 ndevs = midiInGetNumDevs();
	for(uint32 i = 0; i < ndevs; i++)
	{
		MIDIINCAPS mic;
		mic.szPname[0] = 0;
		if(midiInGetDevCaps(i, &mic, sizeof(mic)) == MMSYSERR_NOERROR)
		{
			int item = mfcless::AddItem(m_InputDevice, theApp.GetFriendlyMIDIPortName(mpt::ToUnicode(mpt::String::ReadWinBuf(mic.szPname)), true));
			mfcless::SetItemData(m_InputDevice, item, i);
			if(i == currentDevice)
			{
				m_InputDevice->setCurrentIndex(item);
			}
		}
	}
	m_InputDevice->setUpdatesEnabled(true);
	m_InputDevice->update(false);
}


void CMidiSetupDlg::OnRenameDevice()
{
	int n = m_InputDevice->currentIndex();
	if(n >= 0)
	{
		uint32 device = static_cast<uint32>(mfcless::GetItemData(m_InputDevice, n));
		MIDIINCAPS mic;
		mic.szPname[0] = 0;
		midiInGetDevCaps(device, &mic, sizeof(mic));
		mpt::ustring name = mic.szPname;
		mpt::ustring friendlyName = theApp.GetSettings().Read(U_("MIDI Input Ports"), mpt::ToUnicode(name), name);
		CInputDlg dlg(this, U_("New name for ") + name + U_(":"), friendlyName);
		if(dlg.DoModal() == IDOK)
		{
			if(dlg.resultAsString.IsEmpty() || dlg.resultAsString == name)
				theApp.GetSettings().Remove(U_("MIDI Input Ports"), mpt::ToUnicode(name));
			else
				theApp.GetSettings().Write(U_("MIDI Input Ports"), mpt::ToUnicode(name), dlg.resultAsString);
			RefreshDeviceList(device);
		}
	}
}


void CMidiSetupDlg::OnOK()
{
	CMainFrame *pMainFrm = CMainFrame::GetMainFrame();
	m_midiSetup.set(MidiSetup::RecordVelocity, IsChecked(IDC_CHECK1) != 0);
	m_midiSetup.set(MidiSetup::RecordNoteOff, IsChecked(IDC_CHECK2) != 0);
	m_midiSetup.set(MidiSetup::EnableMidiInOnStartup, IsChecked(IDC_CHECK3)  != 0);
	m_midiSetup.set(MidiSetup::TransposeKeyboard, IsChecked(IDC_CHECK4) != 0);
	m_midiSetup.set(MidiSetup::SendMidiToPlugins, IsChecked(IDC_MIDI_TO_PLUGIN) != 0);
	m_midiSetup.set(MidiSetup::RecordCCsAsMacros, IsChecked(IDC_MIDI_MACRO_CONTROL) != 0);
	m_midiSetup.set(MidiSetup::ApplyChannelVolumeToVelocity, IsChecked(IDC_MIDIVOL_TO_NOTEVOL) != 0);
	m_midiSetup.set(MidiSetup::RespondToPlayControl, IsChecked(IDC_MIDIPLAYCONTROL) != 0);
	m_midiSetup.set(MidiSetup::PlayPatternOnMidiNote, IsChecked(IDC_MIDIPLAYPATTERNONMIDIIN) != 0);
	m_midiSetup.set(MidiSetup::PlayPatternFromStart, m_ContinueMode->currentIndex() == 1);

	int n = m_InputDevice->currentIndex();
	if(n >= 0)
		m_nMidiDevice = static_cast<uint32>(mfcless::GetItemData(m_InputDevice, n));
	else
		m_nMidiDevice = MIDI_MAPPER;

	TrackerSettings::Instance().aftertouchBehaviour = static_cast<RecordAftertouch>(mfcless::GetItemData(m_ATBehaviour, m_ATBehaviour->currentIndex()));
	TrackerSettings::Instance().pitchBendBehaviour = static_cast<RecordPitchBend>(mfcless::GetItemData(m_RecordPitchBend, m_RecordPitchBend->currentIndex()));

	TrackerSettings::Instance().midiVelocityAmp = static_cast<uint16>(Clamp(GetItemInt(IDC_EDIT3), 1u, 10000u));

	mpt::ustring cc;
	cc = GetItemText(IDC_EDIT4);
	TrackerSettings::Instance().midiIgnoreCCs = StringToIgnoredCCs(mpt::ToUnicode(cc));

	int minVal, maxVal;
	m_SpinSpd->GetRange32(minVal, maxVal);
	TrackerSettings::Instance().midiImportTicks = static_cast<uint8>(Clamp(static_cast<int>(GetItemInt(IDC_EDIT1)), minVal, maxVal));
	m_SpinPat->GetRange32(minVal, maxVal);
	TrackerSettings::Instance().midiImportPatternLen = static_cast<ROWINDEX>(Clamp(static_cast<int>(GetItemInt(IDC_EDIT2)), minVal, maxVal));
	if(m_Quantize->currentIndex() != -1)
	{
		TrackerSettings::Instance().midiImportQuantize = static_cast<uint32>(mfcless::GetItemData(m_Quantize, m_Quantize->currentIndex()));
	}

	if(pMainFrm)
		pMainFrm->SetupMidi(m_midiSetup, m_nMidiDevice);
	PropertyPage::OnOK();
}


bool CMidiSetupDlg::OnSetActive()
{
	CMainFrame::m_nLastOptionsPage = OPTIONS_PAGE_MIDI;
	return PropertyPage::OnSetActive();
}


// Wine


void COptionsWine::RegisterMessageMap()
{
	PropertyPage::RegisterMessageMap();
	AddCommand(IDC_CHECK_WINE_ENABLE, &COptionsWine::OnSettingsChanged);
	AddNotify(IDC_COMBO_WINE_PULSEAUDIO, mfcless::Notify::SelChange, &COptionsWine::OnSettingsChanged);
	AddNotify(IDC_COMBO_WINE_PORTAUDIO, mfcless::Notify::SelChange, &COptionsWine::OnSettingsChanged);
	AddNotify(IDC_COMBO_WINE_RTAUDIO, mfcless::Notify::SelChange, &COptionsWine::OnSettingsChanged);
}


COptionsWine::COptionsWine()
	: CPropertyPage(IDD_OPTIONS_WINE)
{
	return;
}


void COptionsWine::DoDataExchange(DataExchange &dx)
{
	PropertyPage::DoDataExchange(dx);
	//{{AFX_DATA_MAP(COptionsWine)
	ExchangeControl(dx, IDC_COMBO_WINE_PULSEAUDIO, m_CbnPulseAudio);
	ExchangeControl(dx, IDC_COMBO_WINE_PORTAUDIO, m_CbnPortAudio);
	ExchangeControl(dx, IDC_COMBO_WINE_RTAUDIO, m_CbnRtAudio);
	//}}AFX_DATA_MAP
}


bool COptionsWine::OnInitDialog()
{
	PropertyPage::OnInitDialog();
	EnableItem(IDC_CHECK_WINE_ENABLE, mpt::OS::Windows::IsWine() ? true : false);
	SetChecked(IDC_CHECK_WINE_ENABLE, TrackerSettings::Instance().WineSupportEnabled ? 1 : 0);
	int index;
	index = mfcless::AddItem(m_CbnPulseAudio, _T("Auto"    )); mfcless::SetItemData(m_CbnPulseAudio, index, 1);
	index = mfcless::AddItem(m_CbnPulseAudio, _T("Enabled" )); mfcless::SetItemData(m_CbnPulseAudio, index, 2);
	index = mfcless::AddItem(m_CbnPulseAudio, U_("Disabled")); mfcless::SetItemData(m_CbnPulseAudio, index, 0);
	m_CbnPulseAudio->setCurrentIndex(0);
	for(index = 0; index < 3; ++index)
	{
		if(mfcless::GetItemData(m_CbnPulseAudio, index) == static_cast<uint32>(TrackerSettings::Instance().WineSupportEnablePulseAudio))
		{
			m_CbnPulseAudio->setCurrentIndex(index);
		}
	}
	index = mfcless::AddItem(m_CbnPortAudio, _T("Auto"    )); mfcless::SetItemData(m_CbnPortAudio, index, 1);
	index = mfcless::AddItem(m_CbnPortAudio, _T("Enabled" )); mfcless::SetItemData(m_CbnPortAudio, index, 2);
	index = mfcless::AddItem(m_CbnPortAudio, U_("Disabled")); mfcless::SetItemData(m_CbnPortAudio, index, 0);
	for(index = 0; index < 3; ++index)
	{
		if(mfcless::GetItemData(m_CbnPortAudio, index) == static_cast<uint32>(TrackerSettings::Instance().WineSupportEnablePortAudio))
		{
			m_CbnPortAudio->setCurrentIndex(index);
		}
	}
	index = mfcless::AddItem(m_CbnRtAudio, _T("Auto"    )); mfcless::SetItemData(m_CbnRtAudio, index, 1);
	index = mfcless::AddItem(m_CbnRtAudio, _T("Enabled" )); mfcless::SetItemData(m_CbnRtAudio, index, 2);
	index = mfcless::AddItem(m_CbnRtAudio, U_("Disabled")); mfcless::SetItemData(m_CbnRtAudio, index, 0);
	for(index = 0; index < 3; ++index)
	{
		if(mfcless::GetItemData(m_CbnRtAudio, index) == static_cast<uint32>(TrackerSettings::Instance().WineSupportEnableRtAudio))
		{
			m_CbnRtAudio->setCurrentIndex(index);
		}
	}
	return true;
}


void COptionsWine::OnSettingsChanged()
{
	SetModified(true);
}


void COptionsWine::OnOK()
{
	TrackerSettings::Instance().WineSupportEnabled = IsChecked(IDC_CHECK_WINE_ENABLE) ? true : false;
	TrackerSettings::Instance().WineSupportEnablePulseAudio = static_cast<int32>(mfcless::GetItemData(m_CbnPulseAudio, m_CbnPulseAudio->currentIndex()));
	TrackerSettings::Instance().WineSupportEnablePortAudio = static_cast<int32>(mfcless::GetItemData(m_CbnPortAudio, m_CbnPortAudio->currentIndex()));
	TrackerSettings::Instance().WineSupportEnableRtAudio = static_cast<int32>(mfcless::GetItemData(m_CbnRtAudio, m_CbnRtAudio->currentIndex()));
	PropertyPage::OnOK();
}


bool COptionsWine::OnSetActive()
{
	CMainFrame::m_nLastOptionsPage = OPTIONS_PAGE_WINE;
	return PropertyPage::OnSetActive();
}


OPENMPT_NAMESPACE_END
