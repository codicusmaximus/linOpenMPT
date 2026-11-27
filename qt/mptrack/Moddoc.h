/*
 * ModDoc.h
 * --------
 * Purpose: Converting between various module formats.
 * Notes  : mfcless twin of upstream's mptrack/Moddoc.h. Same declarations in
 *          the same order. CDocument becomes a QObject that keeps what
 *          CDocument kept (path, title, modified flag, views); message
 *          handlers are plain member functions.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"

#include "Sndfile.h"
#include "../common/misc_util.h"
#include "../common/mptTime.h"
#include "Undo.h"
#include "Notification.h"

#include "mfcless/MessageMap.h"
#include "DocumentView.h"

#include <QtCore/QObject>
#include <QtCore/QSize>

class QWidget;

OPENMPT_NAMESPACE_BEGIN

class EncoderFactoryBase;
class CChildFrame;
struct UpdateHint;

/////////////////////////////////////////////////////////////////////////
// Split Keyboard Settings (pattern editor)

struct SplitKeyboardSettings
{
	enum
	{
		splitOctaveRange = 9,
	};

	bool IsSplitActive() const { return (octaveLink && (octaveModifier != 0)) || (splitInstrument != 0) || (splitVolume != 0); }

	int octaveModifier = 0;	// determines by how many octaves the notes should be transposed up or down
	ModCommand::NOTE splitNote = NOTE_MIDDLEC - 1;
	ModCommand::INSTR splitInstrument = 0;
	ModCommand::VOL splitVolume = 0;
	bool octaveLink = false;	// apply octaveModifier
};

enum InputTargetContext : int8;


struct LogEntry
{
	LogLevel level;
	mpt::ustring message;
	LogEntry() : level(LogInformation) {}
	LogEntry(LogLevel l, const mpt::ustring &m) : level(l), message(m) {}
};


enum LogMode
{
	LogModeInstantReporting,
	LogModeGather,
};


class ScopedLogCapturer
{
private:
	CModDoc &m_modDoc;
	LogMode m_oldLogMode;
	mpt::ustring m_title;
	QWidget *m_pParent;
	bool m_showLog;
public:
	ScopedLogCapturer(CModDoc &modDoc, const mpt::ustring &title = {}, QWidget *parent = nullptr, bool showLog = true);
	~ScopedLogCapturer();
	void ShowLog(bool force = false);
	[[deprecated]] void ShowLog(const std::string &preamble, bool force = false);
	void ShowLog(const mpt::ustring &preamble, bool force = false);
};


struct PlayNoteParam
{
	std::bitset<128> *m_notesPlaying = nullptr;
	SmpLength m_loopStart = 0, m_loopEnd = 0, m_sampleOffset = 0;
	int32 m_volume = -1;
	SAMPLEINDEX m_sample = 0;
	INSTRUMENTINDEX m_instr = 0;
	CHANNELINDEX m_currentChannel = CHANNELINDEX_INVALID;
	ModCommand::NOTE m_note;

	PlayNoteParam(ModCommand::NOTE note) : m_note(note) { }

	PlayNoteParam& LoopStart(SmpLength loopStart) { m_loopStart = loopStart; return *this; }
	PlayNoteParam& LoopEnd(SmpLength loopEnd) { m_loopEnd = loopEnd; return *this; }
	PlayNoteParam& Offset(SmpLength sampleOffset) { m_sampleOffset = sampleOffset; return *this; }

	PlayNoteParam& Volume(int32 volume) { m_volume = volume; return *this; }
	PlayNoteParam& Sample(SAMPLEINDEX sample) { m_sample = sample; return *this; }
	PlayNoteParam& Instrument(INSTRUMENTINDEX instr) { m_instr = instr; return *this; }
	PlayNoteParam& Channel(CHANNELINDEX channel) { m_currentChannel = channel; return *this; }

	PlayNoteParam& CheckNNA(std::bitset<128> &notesPlaying) { m_notesPlaying = &notesPlaying; return *this; }
};


enum class RecordGroup : uint8
{
	NoGroup = 0,
	Group1 = 1,
	Group2 = 2,
};


class CModDoc final : public QObject, public mfcless::MessageMap
{
	Q_OBJECT
protected:
	friend ScopedLogCapturer;
	mutable std::vector<LogEntry> m_Log;
	LogMode m_LogMode = LogModeInstantReporting;
	CSoundFile m_SndFile;

	QObject *m_hWndFollow = nullptr;
	FlagSet<Notification::Type, uint16> m_notifyType;
	Notification::Item m_notifyItem = 0;
	QSize m_szOldPatternScrollbarsPos = { -10, -10 };

	CPatternUndo m_PatternUndo;
	CSampleUndo m_SampleUndo;
	CInstrumentUndo m_InstrumentUndo;
	SplitKeyboardSettings m_SplitKeyboardSettings;	// this is maybe not the best place to keep them, but it should do the job
	mpt::Date::Unix m_creationTime;

	std::atomic<bool> m_modifiedAutosave = false; // Modified since last autosave?

public:
	class NoteToChannelMap : public std::array<CHANNELINDEX, NOTE_MAX - NOTE_MIN + 1>
	{
	public:
		NoteToChannelMap() { fill(CHANNELINDEX_INVALID); }
	};
	NoteToChannelMap m_noteChannel;	// Note -> Preview channel assignment

	bool m_ShowSavedialog = false;
	bool m_bHasValidPath = false; //becomes true if document is loaded or saved.

protected:
	// Note-off event buffer for MIDI sustain pedal
	std::array<std::vector<uint32>, 16> m_midiSustainBuffer;
	std::array<std::bitset<128>, 16> m_midiPlayingNotes;
	std::bitset<16> m_midiSustainActive;

	std::vector<RecordGroup> m_multiRecordGroup;

	// mfcless: what CDocument kept.
	mpt::PathString m_strPathName;
	mpt::ustring m_strTitle;
	bool m_bModified = false;
	std::vector<mfcless::DocumentView *> m_views;

public: // created by CModDocTemplate (upstream: from serialization only)
	CModDoc();

// public members
public:
	CSoundFile &GetSoundFile() { return m_SndFile; }
	const CSoundFile &GetSoundFile() const { return m_SndFile; }

	bool IsModified() const { return m_bModified; }
	void SetModified(bool modified = true);
	bool ModifiedSinceLastAutosave();
	void SetShowSaveDialog(bool b) { m_ShowSavedialog = b; }
	void PostMessageToAllViews(uint32 uMsg, uintptr_t wParam = 0, intptr_t lParam = 0);
	void SendNotifyMessageToAllViews(uint32 uMsg, uintptr_t wParam = 0, intptr_t lParam = 0);
	void SendMessageToActiveView(uint32 uMsg, uintptr_t wParam = 0, intptr_t lParam = 0);
	MODTYPE GetModType() const noexcept { return m_SndFile.GetType(); }
	INSTRUMENTINDEX GetNumInstruments() const { return m_SndFile.m_nInstruments; }
	SAMPLEINDEX GetNumSamples() const { return m_SndFile.m_nSamples; }

	// Logging for general progress and error events.
	void AddToLog(LogLevel level, const mpt::ustring &text) const;
	/*[[deprecated]]*/ void AddToLog(const std::string &text) const { AddToLog(LogInformation, mpt::ToUnicode(mpt::Charset::Locale, text)); }
	/*[[deprecated]]*/ void AddToLog(const char *text) const { AddToLog(LogInformation, mpt::ToUnicode(mpt::Charset::Locale, text ? text : "")); }

	const std::vector<LogEntry> & GetLog() const { return m_Log; }
	mpt::ustring GetLogString() const;
	LogLevel GetMaxLogLevel() const;
protected:
	LogMode GetLogMode() const { return m_LogMode; }
	void SetLogMode(LogMode mode) { m_LogMode = mode; }
	void ClearLog();
	uint32 ShowLog(const mpt::ustring &preamble, const mpt::ustring &title = {}, QWidget *parent = nullptr);
	uint32 ShowLog(const mpt::ustring &title = {}, QWidget *parent = nullptr) { return ShowLog(mpt::ustring(), title, parent); }

public:

	void ClearFilePath() { m_strPathName = mpt::PathString(); }

	void ViewPattern(uint32 nPat, uint32 nOrd);
	void ViewSample(uint32 nSmp);
	void ViewInstrument(uint32 nIns);
	QObject *GetFollowWnd() const { return m_hWndFollow; }
	void SetFollowWnd(QObject *hwnd);

	void SetNotifications(FlagSet<Notification::Type> type, Notification::Item item = 0) { m_notifyType = type; m_notifyItem = item; }
	FlagSet<Notification::Type, uint16> GetNotificationType() const { return m_notifyType; }
	Notification::Item GetNotificationItem() const { return m_notifyItem; }

	void ActivateWindow();

	void OnSongProperties();

	void PrepareUndoForAllPatterns(bool storeChannelInfo = false, const char *description = "");
	CPatternUndo &GetPatternUndo() { return m_PatternUndo; }
	CSampleUndo &GetSampleUndo() { return m_SampleUndo; }
	CInstrumentUndo &GetInstrumentUndo() { return m_InstrumentUndo; }
	SplitKeyboardSettings &GetSplitKeyboardSettings() { return m_SplitKeyboardSettings; }

	mpt::Date::Unix GetCreationTime() const { return m_creationTime; }

// operations
public:
	bool ChangeModType(MODTYPE wType);

	bool ChangeNumChannels(CHANNELINDEX nNewChannels, const bool showCancelInRemoveDlg = true);
	bool RemoveChannels(const std::vector<bool> &keepMask, bool verbose = false);
	void CheckUsedChannels(std::vector<bool> &usedMask, CHANNELINDEX maxRemoveCount = MAX_BASECHANNELS) const;

	CHANNELINDEX ReArrangeChannels(const std::vector<CHANNELINDEX> &fromToArray, const bool createUndoPoint = true);
	SAMPLEINDEX ReArrangeSamples(const std::vector<SAMPLEINDEX> &newOrder);
	INSTRUMENTINDEX ReArrangeInstruments(const std::vector<INSTRUMENTINDEX> &newOrder, deleteInstrumentSamples removeSamples = doNoDeleteAssociatedSamples);
	SEQUENCEINDEX ReArrangeSequences(const std::vector<SEQUENCEINDEX> &newOrder);

	bool ConvertInstrumentsToSamples();
	bool ConvertSamplesToInstruments();
	PLUGINDEX RemovePlugs(const std::vector<bool> &keepMask);
	bool RemovePlugin(PLUGINDEX plugin);

	void ClonePlugin(SNDMIXPLUGIN &target, const SNDMIXPLUGIN &source);
	void AppendModule(const CSoundFile &source);

	// Create a new pattern and, if order position is specified, inserts it into the order list.
	PATTERNINDEX InsertPattern(ROWINDEX rows, ORDERINDEX ord = ORDERINDEX_INVALID);
	SAMPLEINDEX InsertSample();
	INSTRUMENTINDEX InsertInstrument(SAMPLEINDEX sample = SAMPLEINDEX_INVALID, INSTRUMENTINDEX duplicateSource = INSTRUMENTINDEX_INVALID, bool silent = false);
	INSTRUMENTINDEX InsertInstrumentForPlugin(PLUGINDEX plug);
	INSTRUMENTINDEX HasInstrumentForPlugin(PLUGINDEX plug) const;
	void InitializeInstrument(ModInstrument *pIns);
	bool RemoveOrder(SEQUENCEINDEX nSeq, ORDERINDEX nOrd);
	bool RemovePattern(PATTERNINDEX nPat);
	bool RemoveSample(SAMPLEINDEX nSmp);
	bool RemoveInstrument(INSTRUMENTINDEX nIns);

	void ProcessMIDI(uint32 midiData, SAMPLEINDEX smp, INSTRUMENTINDEX ins, IMixPlugin *plugin, InputTargetContext ctx);
	CHANNELINDEX PlayNote(PlayNoteParam &params, NoteToChannelMap *noteChannel = nullptr);
	bool NoteOff(uint32 note, bool fade = false, INSTRUMENTINDEX ins = INSTRUMENTINDEX_INVALID, CHANNELINDEX currentChn = CHANNELINDEX_INVALID);
	void CheckNNA(ModCommand::NOTE note, INSTRUMENTINDEX ins, std::bitset<128> &playingNotes);
	void UpdateOPLInstrument(SAMPLEINDEX smp);

	bool IsNotePlaying(uint32 note, SAMPLEINDEX nsmp = 0, INSTRUMENTINDEX nins = 0);
	bool MuteChannel(CHANNELINDEX nChn, bool bMute);
	bool UpdateChannelMuteStatus(CHANNELINDEX nChn);
	bool MuteSample(SAMPLEINDEX nSample, bool bMute);
	bool MuteInstrument(INSTRUMENTINDEX nInstr, bool bMute);
	// Returns true if toggling the mute status of a channel should set the document as modified given the current module format and settings.
	bool MuteToggleModifiesDocument() const;

	bool SurroundChannel(CHANNELINDEX nChn, bool bSurround);
	bool SetChannelGlobalVolume(CHANNELINDEX nChn, uint16 nVolume);
	bool SetChannelDefaultPan(CHANNELINDEX nChn, uint16 nPan);
	bool IsChannelMuted(CHANNELINDEX nChn) const;
	bool IsSampleMuted(SAMPLEINDEX nSample) const;
	bool IsInstrumentMuted(INSTRUMENTINDEX nInstr) const;

	bool NoFxChannel(CHANNELINDEX nChn, bool bNoFx, bool updateMix = true);
	bool IsChannelNoFx(CHANNELINDEX nChn) const;

	RecordGroup GetChannelRecordGroup(CHANNELINDEX channel) const;
	void SetChannelRecordGroup(CHANNELINDEX channel, RecordGroup recordGroup);
	void ToggleChannelRecordGroup(CHANNELINDEX channel, RecordGroup recordGroup);
	void ReinitRecordState();

	CHANNELINDEX GetNumChannels() const noexcept { return m_SndFile.GetNumChannels(); }
	uint32 GetPatternSize(PATTERNINDEX nPat) const;
	bool IsChildSample(INSTRUMENTINDEX nIns, SAMPLEINDEX nSmp) const;
	INSTRUMENTINDEX FindSampleParent(SAMPLEINDEX sample) const;
	SAMPLEINDEX FindInstrumentChild(INSTRUMENTINDEX nIns) const;
	bool MoveOrder(ORDERINDEX nSourceNdx, ORDERINDEX nDestNdx, bool bUpdate = true, bool bCopy = false, SEQUENCEINDEX nSourceSeq = SEQUENCEINDEX_INVALID, SEQUENCEINDEX nDestSeq = SEQUENCEINDEX_INVALID);
	bool ExpandPattern(PATTERNINDEX nPattern);
	bool ShrinkPattern(PATTERNINDEX nPattern);

	bool SetDefaultChannelColors() { return SetDefaultChannelColors(0, GetNumChannels()); }
	bool SetDefaultChannelColors(CHANNELINDEX channel) { return SetDefaultChannelColors(channel, channel + 1u); }
	bool SetDefaultChannelColors(CHANNELINDEX minChannel, CHANNELINDEX maxChannel);
	bool SupportsChannelColors() const { return GetModType() & (MOD_TYPE_XM | MOD_TYPE_IT | MOD_TYPE_MPT); }

	bool CopyEnvelope(INSTRUMENTINDEX nIns, EnvelopeType nEnv);
	bool SaveEnvelope(INSTRUMENTINDEX nIns, EnvelopeType nEnv, const mpt::PathString &fileName);
	bool PasteEnvelope(INSTRUMENTINDEX nIns, EnvelopeType nEnv);
	bool LoadEnvelope(INSTRUMENTINDEX nIns, EnvelopeType nEnv, const mpt::PathString &fileName);

	intptr_t ActivateView(uint32 nIdView, uint32 dwParam);
	// Notify all views of document updates (GUI thread only)
	void UpdateAllViews(mfcless::DocumentView *pSender, UpdateHint hint, QObject *pHint=NULL);
	// Notify all views of document updates (for non-GUI threads)
	void UpdateAllViews(UpdateHint hint);
	void GetEditPosition(ROWINDEX &row, PATTERNINDEX &pat, ORDERINDEX &ord);
	intptr_t OnCustomKeyMsg(uintptr_t, intptr_t);
	void TogglePluginEditor(uint32 m_nCurrentPlugin, bool onlyThisEditor = false);
	void RecordParamChange(PLUGINDEX slot, PlugParamIndex param);
	void LearnMacro(int macro, PlugParamIndex param);
	void SetElapsedTime(ORDERINDEX nOrd, ROWINDEX nRow, bool setSamplePos);
	void SetLoopSong(bool loop);

	// Global settings to pattern effect conversion
	bool GlobalVolumeToPattern();

	bool HasMPTHacks(const bool autofix = false);

	void FixNullStrings();

// Fix: save pattern scrollbar position when switching to other tab
	QSize GetOldPatternScrollbarsPos() const { return m_szOldPatternScrollbarsPos; };
	void SetOldPatternScrollbarsPos( QSize s ){ m_szOldPatternScrollbarsPos = s; };

	void OnFileWaveConvert(ORDERINDEX nMinOrder, ORDERINDEX nMaxOrder);
	void OnFileWaveConvert(ORDERINDEX nMinOrder, ORDERINDEX nMaxOrder, const std::vector<EncoderFactoryBase*> &encFactories);

	// Returns formatted ModInstrument name.
	// [in] bEmptyInsteadOfNoName: In case of unnamed instrument string, "(no name)" is returned unless this
	//                             parameter is true is case which an empty name is returned.
	// [in] bIncludeIndex: True to include instrument index in front of the instrument name, false otherwise.
	mpt::ustring GetPatternViewInstrumentName(INSTRUMENTINDEX nInstr, bool bEmptyInsteadOfNoName = false, bool bIncludeIndex = true) const;

	mpt::ustring FormatSubsongName(const std::vector<SubSong> &songs, size_t subSong);

	// Check if a given channel contains data.
	bool IsChannelUnused(CHANNELINDEX nChn) const;
	// Check whether a sample is used.
	// In sample mode, the sample numbers in all patterns are checked.
	// In instrument mode, it is only checked if a sample is referenced by an instrument (but not if the sample is actually played anywhere)
	bool IsSampleUsed(SAMPLEINDEX sample, bool searchInMutedChannels = true) const;
	// Check whether an instrument is used (only for instrument mode).
	bool IsInstrumentUsed(INSTRUMENTINDEX instr, bool searchInMutedChannels = true) const;

	void InitChannel(CHANNELINDEX chn);
	
	// protected members
protected:

	void InitializeMod();

	CChildFrame *GetChildFrame(); //rewbs.customKeys

// Overrides
	// mfcless: CDocument's overrides and what they relied on.
public:
	bool OnNewDocument();
	bool OnOpenDocument(const mpt::PathString &filename);
	void OnCloseDocument();
	void SafeFileClose();
	bool OnSaveDocument(const mpt::PathString &filename, const bool setPath = true);
	bool SaveFile(const mpt::PathString &filename, bool allowRelativeSamplePaths);

	void SetPathName(const mpt::PathString &filename, bool bAddToMRU = true);
	mpt::PathString GetPathNameMpt() const
	{
		return m_strPathName;
	}
	const mpt::ustring &GetTitle() const { return m_strTitle; }
	void SetTitle(const mpt::ustring &title);

	bool SaveModified();
	bool SaveAllSamples(bool showPrompt = true);
	bool SaveSample(SAMPLEINDEX smp);

	bool DoSave(const mpt::PathString &filename, bool setPath = true);
	void DeleteContents();

	// mfcless: CDocument's view list.
	void AddView(mfcless::DocumentView *view);
	void RemoveView(mfcless::DocumentView *view);
	const std::vector<mfcless::DocumentView *> &GetViews() const { return m_views; }

	// Get the sample index for the current pattern cell (resolves instrument note maps, etc)
	SAMPLEINDEX GetSampleIndex(const ModCommand &m, ModCommand::INSTR lastInstr = 0) const;
	// Get group (octave) size from given instrument (or sample in sample mode)
	int GetInstrumentGroupSize(INSTRUMENTINDEX instr) const;
	int GetBaseNote(INSTRUMENTINDEX instr) const;
	ModCommand::NOTE GetNoteWithBaseOctave(int noteOffset, INSTRUMENTINDEX instr) const;
	INSTRUMENTINDEX GetParentInstrumentWithSameName(SAMPLEINDEX smp) const;

	size_t GetSubsongForCurrentEditPos(const std::vector<SubSong> &subsongs) const;

	// Convert a linear volume property to decibels, and format the value as a readable string
	static mpt::ustring LinearToDecibelsString(double value, double valueAtZeroDB);
	// Convert a linear volume property to decibels
	static double LinearToDecibels(double value, double valueAtZeroDB);
	// Format a decibel value as a readable string
	static mpt::ustring DecibelsToStrings(double dB);
	// Convert a panning value to a more readable string
	static mpt::ustring PanningToString(int32 value, int32 valueAtCenter);

	void SerializeViews() const;
	void DeserializeViews();

	// View MIDI Mapping dialog for given plugin and parameter combination.
	void ViewMIDIMapping(PLUGINDEX plugin = PLUGINDEX_INVALID, PlugParamIndex param = 0);

// Implementation
public:
	~CModDoc() override;

// Generated message map functions
public:
	void RegisterMessageMap() override;
	void OnFileWaveConvert();
	void OnFileMidiConvert();
	void OnFileOPLExport();
	void OnFileCompatibilitySave();
	void OnPlayerPlay();
	void OnPlayerStop();
	void OnPlayerPause();
	void OnPlayerPlayFromStart();
	void OnPanic();
	void OnEditGlobals();
	void OnEditPatterns();
	void OnEditSamples();
	void OnEditInstruments();
	void OnEditComments();
	void OnShowCleanup();
	void OnShowSampleTrimmer();
	void OnSetupZxxMacros();
	void OnEstimateSongLength();
	void OnApproximateBPM();
	void OnUpdateXMITMPTOnly(mfcless::CommandState &state);
	void OnUpdateHasEditHistory(mfcless::CommandState &state);
	void OnUpdateHasMIDIMappings(mfcless::CommandState &state);
	void OnUpdateCompatExportableOnly(mfcless::CommandState &state);
	void OnPatternRestart() { OnPatternRestart(true); } //rewbs.customKeys
	void OnPatternRestart(bool loop); //rewbs.customKeys
	void OnPatternPlay(); //rewbs.customKeys
	void OnPatternPlayNoLoop(); //rewbs.customKeys
	void OnViewEditHistory();
	void OnViewMPTHacks();
	void OnViewTempoSwingSettings();
	void OnSaveCopy();
	void OnSaveTemplateModule();
	void OnAppendModule();
	void OnViewMIDIMapping() { ViewMIDIMapping(); }
	void OnChannelManager();
private:

	void ChangeFileExtension(MODTYPE nNewType);
	CHANNELINDEX FindAvailableChannel() const;
};

/////////////////////////////////////////////////////////////////////////////

OPENMPT_NAMESPACE_END
