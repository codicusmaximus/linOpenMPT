/*
 * CleanupSong.h
 * ---------------
 * Purpose: Dialog for cleaning up modules (rearranging, removing unused items).
 * Notes  : mfcless twin of upstream's mptrack/CleanupSong.h.
 *          Hand-edited from the twin.py draft.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "DialogBase.h"

class QWidget;

OPENMPT_NAMESPACE_BEGIN

class CModDoc;

class CModCleanupDlg : public DialogBase
{
	Q_OBJECT
private:
	enum CleanupOptions
	{
		// patterns
		kCleanupPatterns = 0,
		kRemovePatterns,
		kRearrangePatterns,
		kRemoveDuplicatePatterns,
		// orders
		kMergeSequences,
		kRemoveOrders,
		// samples
		kCleanupSamples,
		kRemoveSamples,
		kRearrangeSamples,
		kOptimizeSamples,
		// instruments
		kCleanupInstruments,
		kRemoveAllInstruments,
		// plugins
		kCleanupPlugins,
		kRemoveAllPlugins,
		// misc
		kResetVariables,
		kCleanupChannels,

		kNone,
		kMaxCleanupOptions = kNone
	};

	CModDoc &modDoc;
	static bool m_CheckBoxes[kMaxCleanupOptions]; // Checkbox state
	static const uint16 m_CleanupIDtoDlgID[kMaxCleanupOptions]; // Checkbox -> Control ID LUT
	static const CleanupOptions m_MutuallyExclusive[kMaxCleanupOptions]; // Options that are mutually exclusive to each other.

	// Actual cleanup implementations:
	// Patterns
	bool RemoveDuplicatePatterns();
	bool RemoveUnusedPatterns(); // Remove unused patterns
	bool RearrangePatterns(); // Rearrange patterns
	bool RemoveAllPatterns();
	// Orders
	bool MergeSequences();
	bool RemoveAllOrders();
	// Samples
	bool RemoveUnusedSamples(); // Remove unused samples
	bool RemoveAllSamples();
	bool RearrangeSamples(); // Rearrange sample list
	bool OptimizeSamples(); // Remove unused sample data
	// Instruments
	bool RemoveUnusedInstruments(); // Remove unused instruments
	bool RemoveAllInstruments();
	// Plugins
	bool RemoveUnusedPlugins(); // Remove ununsed plugins
	bool RemoveAllPlugins();
	// Misc
	bool ResetVariables(); // Turn module into samplepack (convert to IT, remove patterns, etc.)
	bool RemoveUnusedChannels();

public:
	CModCleanupDlg(CModDoc &modParent, QWidget *parent);

protected:
	bool OnInitDialog() override;
	void OnOK() override;
	mpt::ustring GetToolTipText(uint32 id, QWidget * hwnd) const override;
	void OnPresetCleanupSong();
	void OnPresetCompoCleanup();
	void OnVerifyMutualExclusive();
	void RegisterMessageMap() override;
};

OPENMPT_NAMESPACE_END
