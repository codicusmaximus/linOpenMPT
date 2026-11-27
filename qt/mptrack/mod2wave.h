/*
 * mod2wave.h
 * ----------
 * Purpose: Module to steaming audio (WAV, MP3, etc.) conversion (dialog + conversion code).
 * Notes  : mfcless twin of upstream's mptrack/mod2wave.h.
 *          Draft from qt/tools/twin.py, not yet edited by hand.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "mfcless/Controls.h"

#include "AccessibleControls.h"
#include "DialogBase.h"
#include "ProgressDialog.h"
#include "Settings.h"
#include "openmpt/streamencoder/StreamEncoder.hpp"
#include "StreamEncoderSettings.h"
#include "../soundlib/Snd_defs.h"


class QComboBox;
class QLineEdit;
class QWidget;

OPENMPT_NAMESPACE_BEGIN

class CModDoc;
class CSoundFile;
struct SubSong;

struct CWaveConvertSettings
{
	std::vector<EncoderFactoryBase*> EncoderFactories;
	std::vector<std::unique_ptr<EncoderSettingsConf>> EncoderSettings;

	Setting<mpt::ustring> EncoderName;
	std::size_t EncoderIndex;

	StoredTags storedTags;
	FileTags Tags;

	int repeatCount = 1;
	ORDERINDEX minOrder = ORDERINDEX_INVALID, maxOrder = ORDERINDEX_INVALID;
	SAMPLEINDEX sampleSlot = 0;

	bool normalize = false;
	bool silencePlugBuffers = false;
	bool outputToSample = false;

	std::size_t FindEncoder(const mpt::ustring &name) const;
	void SelectEncoder(std::size_t index);
	EncoderFactoryBase *GetEncoderFactory() const;
	const Encoder::Traits *GetTraits() const;
	EncoderSettingsConf &GetEncoderSettings() const;
	Encoder::Settings GetEncoderSettingsWithDetails() const;
	CWaveConvertSettings(SettingsContainer &conf, const std::vector<EncoderFactoryBase*> &encFactories);
};


class CWaveConvert : public DialogBase
{
	Q_OBJECT
public:
	CWaveConvertSettings m_Settings;
	CSoundFile &m_SndFile;
	uint64 m_dwSongLimit = 0;
	std::vector<SubSong> m_subSongs;

	bool m_bGivePlugsIdleTime = false;
	bool m_bChannelMode = false;     // Render by channel
	bool m_bInstrumentMode = false;  // Render by instrument

private:
	const Encoder::Traits *encTraits = nullptr;

	QComboBox *m_CbnFileType = nullptr, *m_CbnSampleRate = nullptr, *m_CbnChannels = nullptr, *m_CbnDither = nullptr, *m_CbnSampleFormat = nullptr, *m_CbnSampleSlot = nullptr;
	mfcless::UpDown *m_SpinLoopCount = nullptr, *m_SpinMinOrder = nullptr, *m_SpinMaxOrder = nullptr, *m_SpinSubsongIndex = nullptr;

	AccessibleEdit m_EditLoopCount, m_EditSubSong, m_EditMinOrder;
	QLineEdit *m_EditTitle = nullptr, *m_EditAuthor = nullptr, *m_EditURL = nullptr, *m_EditAlbum = nullptr, *m_EditYear = nullptr;
	QComboBox *m_CbnGenre = nullptr;
	QLineEdit *m_EditGenre = nullptr;
	size_t m_selectedSong = 0;
	const ORDERINDEX m_nNumOrders;
	bool m_locked = true; 

public:
	CWaveConvert(QWidget *parent, ORDERINDEX minOrder, ORDERINDEX maxOrder, ORDERINDEX numOrders, CModDoc &modDoc, const std::vector<EncoderFactoryBase *> &encFactories);
	~CWaveConvert();

private:
	void FillFileTypes();
	void FillSamplerates();
	void FillChannels();
	void FillFormats();
	void FillDither();
	void FillTags();

	void LoadTags();

	void SaveEncoderSettings();
	void SaveTags();

	void UpdateSubsongName();
	void UpdateDialog();

	bool OnInitDialog() override;
	void DoDataExchange(DataExchange &dx) override;
	void OnOK() override;

	void OnCheckTimeLimit();
	void OnCheckChannelMode();
	void OnCheckInstrMode();
	void OnFileTypeChanged();
	void OnSamplerateChanged();
	void OnChannelsChanged();
	void OnDitherChanged();
	void OnFormatChanged();
	void OnSubsongChanged();
	void OnPlayerOptions();
	void OnExportModeChanged();
	void OnSampleSlotChanged();

	void RegisterMessageMap() override;
};


class CDoWaveConvert: public CProgressDialog
{
public:
	uint64 m_dwSongLimit = 0;
	bool m_bGivePlugsIdleTime = false;

public:
	CDoWaveConvert(CSoundFile &sndFile, std::ostream &f, const mpt::ustring &caption, const CWaveConvertSettings &settings, const SubSong &subSong, QWidget *parent = nullptr)
		: CProgressDialog(parent)
		, m_Settings(settings)
		, m_SndFile(sndFile)
		, fileStream(f)
		, caption(caption)
		, m_subSong(subSong)
	{ }

	void Run() override;

private:
	const CWaveConvertSettings &m_Settings;
	CSoundFile &m_SndFile;
	std::ostream &fileStream;
	const mpt::ustring &caption;
	const SubSong &m_subSong;
};


OPENMPT_NAMESPACE_END
