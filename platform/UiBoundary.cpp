// linOpenMPT platform layer: the points where upstream's engine and app logic
// call into the Windows UI, implemented headless.
//
// Everything here is a placeholder for the Qt application (plan steps 3-4):
// message boxes go to stderr, file dialogs report "cancelled", there is no
// main window yet. Plugin editor windows abort if anything tries to open one,
// because nothing can until the Qt UI exists.

#include "stdafx.h"
#include "Mainfrm.h"
#include "Reporting.h"
#include "FileDialog.h"
#include "HighDPISupport.h"
#include "PatternClipboard.h"
#include "AbstractVstEditor.h"
#include "DefaultVstEditor.h"
#include "plugins/LFOPluginEditor.h"
#include "plugins/MidiInOut.h"
#include "DialogBase.h"
#include "ResizableDialog.h"
#include "UpdateHints.h"
#include "../soundlib/plugins/LFOPlugin.h"

#include <cstdlib>
#include <iostream>

OPENMPT_NAMESPACE_BEGIN

[[noreturn]] static void NotPortedYet(const char *what)
{
	std::cerr << "linOpenMPT: " << what << " is not available yet (needs the Qt UI)." << std::endl;
	std::abort();
}

static std::string Utf8(const AnyStringLocale &s)
{
	return mpt::ToCharset(mpt::Charset::UTF8, mpt::ustring(s));
}

// ---- main frame -------------------------------------------------------------
CMainFrame *CMainFrame::GetMainFrame() noexcept
{
	return static_cast<CMainFrame *>(theApp.m_pMainWnd);   // no main window yet: nullptr
}

void CMainFrame::SetHelpText(const TCHAR *)
{
}

// ---- message boxes ----------------------------------------------------------
void Reporting::Notification(const AnyStringLocale &text, const CWnd *)
{
	std::cerr << "[notice] " << Utf8(text) << std::endl;
}

void Reporting::Error(const AnyStringLocale &text, const CWnd *)
{
	std::cerr << "[error] " << Utf8(text) << std::endl;
}

ConfirmAnswer Reporting::Confirm(const AnyStringLocale &text, const AnyStringLocale &caption, bool showCancel, bool, const CWnd *)
{
	std::cerr << "[confirm] " << Utf8(caption) << ": " << Utf8(text) << " -> " << (showCancel ? "cancel" : "no") << std::endl;
	return showCancel ? cnfCancel : cnfNo;
}

// ---- file dialogs -----------------------------------------------------------
bool FileDialog::Show(CWnd *)
{
	return false;   // as if the user cancelled
}

// ---- window placement -------------------------------------------------------
bool HighDPISupport::GetWindowPlacement(HWND, WINDOWPLACEMENT &)
{
	return false;
}

// ---- pattern clipboard (upstream PatternClipboard.cpp needs the Win32
// clipboard; the in-memory part is reproduced here) -------------------------
PatternClipboard PatternClipboard::instance;

void PatternClipboard::SetClipboardSize(clipindex_t maxEntries)
{
	instance.m_clipboards.resize(maxEntries, {"", _T("unused")});
	LimitMax(instance.m_activeClipboard, maxEntries - 1);
}

// ---- plugin editor windows ----------------------------------------------------
// Upstream's editors are MFC dialogs. Constructing one means something tried
// to open a plugin editor, which needs the Qt UI.
BOOL DialogBase::OnInitDialog() { return TRUE; }
BOOL DialogBase::PreTranslateMessage(MSG *) { return FALSE; }
INT_PTR DialogBase::OnToolHitTest(CPoint, TOOLINFO *) const { return -1; }
BOOL ResizableDialog::OnInitDialog() { return TRUE; }
void ResizableDialog::PostNcDestroy() { }
void ResizableDialog::ResizeDynamicLayout() { }

CAbstractVstEditor::CAbstractVstEditor(IMixPlugin &plugin) : m_VstPlugin(plugin) { NotPortedYet("plugin editor windows"); }
CAbstractVstEditor::~CAbstractVstEditor() { }
int CAbstractVstEditor::GetLearnMacro() { return -1; }
void CAbstractVstEditor::SetLearnMacro(int) { }
void CAbstractVstEditor::SetupMenu(bool) { }
void CAbstractVstEditor::PostNcDestroy() { }
bool CAbstractVstEditor::OpenEditor(CWnd *) { return false; }
void CAbstractVstEditor::DoClose() { }
void CAbstractVstEditor::UpdateView(UpdateHint) { }
BOOL CAbstractVstEditor::PreTranslateMessage(MSG *) { return FALSE; }

CDefaultVstEditor::CDefaultVstEditor(IMixPlugin &plugin) : CAbstractVstEditor(plugin) { }
CDefaultVstEditor::~CDefaultVstEditor() { }
bool CDefaultVstEditor::OpenEditor(CWnd *) { return false; }
void CDefaultVstEditor::DoDataExchange(CDataExchange *) { }
void CDefaultVstEditor::UpdateControls(bool) { }

LFOPluginEditor::LFOPluginEditor(LFOPlugin &plugin) : CAbstractVstEditor(plugin), m_lfoPlugin(plugin), m_locked(false) { }
bool LFOPluginEditor::OpenEditor(CWnd *) { return false; }
void LFOPluginEditor::UpdateParamDisplays() { }
void LFOPluginEditor::UpdateParam(int32) { }
void LFOPluginEditor::UpdateView(UpdateHint) { }
void LFOPluginEditor::DoDataExchange(CDataExchange *) { }

// ---- MIDI I/O plugin (needs a MIDI backend; plan step 4) -------------------
IMixPlugin *MidiInOut::Create(VSTPluginLib &, CSoundFile &, SNDMIXPLUGIN &)
{
	return nullptr;
}

OPENMPT_NAMESPACE_END

// ---- Win32 calls that target windows ----------------------------------------
SHORT GetAsyncKeyState(int)
{
	return 0;
}

BOOL PostMessage(HWND, UINT, WPARAM, LPARAM)
{
	return FALSE;
}

BOOL CWnd::PostMessage(UINT, WPARAM, LPARAM)
{
	return FALSE;
}
